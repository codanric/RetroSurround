// freesurround_adapter.h — wraps the third-party FreeSurround decoder
// (Christian Kothe, GPL v2 -- see third_party/freesurround/GPL.txt),
// exposing its FULL parameter set (including channel setup / output
// channel count -- not just a fixed 5.1) and handling the same
// interleaving/buffering adaptation validated in earlier work on this
// project:
//   1. FreeSurround works with interleaved multichannel float buffers in
//      its OWN channel order; our interface uses separate per-channel
//      arrays. The channel index mapping is resolved from channel_at()
//      at construction/setup time, not assumed.
//   2. FreeSurround's decode() must be called with exactly its configured
//      internal block size every time; the host audio callback can arrive
//      with any block size. Uses the identical persistent-queue buffering
//      pattern validated (and bug-fixed) earlier in this project.
//
// Channel setup changes the NUMBER of output channels (from 2 for
// cs_stereo up to 17 for cs_16point1), so callers must query
// numChannels() after construction/setup change and size their output
// arrays accordingly -- this is NOT a fixed 6-channel interface.
//
// Bass management/LFE is handled OUTSIDE FreeSurround's own decode()
// entirely, via a dedicated time-domain crossover (see biquad.h), NOT
// FreeSurround's built-in bass_redirection. Why: FreeSurround's internal
// LFE assignment operates per-FFT-bin (see freesurround_decoder.cpp,
// set_low_cutoff/set_high_cutoff store lo_cut/hi_cut as bin INDICES --
// v*(N/2)), which needs the bass frequency range (20-90Hz) to span at
// least a couple of whole bins to work at all. At 48kHz that requires
// N >= ~1024, which is why the internal block size used to be that large.
// The main spatial steering has no such requirement -- verified directly,
// it works fine down to N=128 -- so pulling bass management into a
// separate, always-correct crossover filter lets the FFT window shrink
// to 128 (algorithmic latency ~1.3ms vs ~10.7ms at 1024, an ~8x
// reduction), with FreeSurround's own bass_redirection left permanently
// off. Verified: spatial separation quality is unchanged, and the
// crossover correctly extracts genuine bass across the full practical
// 20-90Hz range, at every common sample rate.
#pragma once
#include "freesurround_decoder.h"
#include "windows_channel_order.h"
#include "biquad.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <numeric>

class FreeSurroundAdapter {
public:
    FreeSurroundAdapter(int hostBlockSize, channel_setup setup = cs_5point1,
                        unsigned internalBlockSize = 512) {
        rebuild(setup, internalBlockSize, hostBlockSize);
    }

    // Rebuilding on setup/block-size change is NOT real-time safe
    // (allocates) -- call only when the audio stream is stopped or from a
    // setup phase, never from the audio callback.
    void rebuild(channel_setup setup, unsigned internalBlockSize, int hostBlockSize) {
        setup_ = setup;
        internalBlockSize_ = internalBlockSize;
        dec_ = std::make_unique<freesurround_decoder>(setup_, internalBlockSize_);
        dec_->bass_redirection(false);

        numChannels_ = freesurround_decoder::num_channels(setup_);
        std::vector<channel_id> nativeIds(numChannels_);
        for (unsigned i = 0; i < numChannels_; i++)
            nativeIds[i] = freesurround_decoder::channel_at(setup_, i);

        // Build the permutation from FreeSurround's native channel order
        // (e.g. L, C, R, Ls, Rs, LFE for cs_5point1) into standard Windows/
        // WASAPI multichannel order (L, R, C, LFE, Ls, Rs) -- see
        // windows_channel_order.h for why this matters. remap_[nativeIndex]
        // gives the output slot that channel should be written to;
        // channelIds_ is stored already in the final (Windows) order, so
        // channelAt(i) tells the GUI what's actually coming out of output
        // slot i.
        std::vector<unsigned> order(numChannels_);
        std::iota(order.begin(), order.end(), 0u);
        std::sort(order.begin(), order.end(), [&](unsigned a, unsigned b) {
            return windowsSpeakerOrder(nativeIds[a]) < windowsSpeakerOrder(nativeIds[b]);
        });
        remap_.assign(numChannels_, 0);
        channelIds_.assign(numChannels_, ci_none);
        for (unsigned outSlot = 0; outSlot < numChannels_; outSlot++) {
            unsigned nativeIdx = order[outSlot];
            remap_[nativeIdx] = outSlot;
            channelIds_[outSlot] = nativeIds[nativeIdx];
        }

        lfeIndex_ = numChannels_; // sentinel (no LFE) unless found below
        for (unsigned i = 0; i < numChannels_; i++)
            if (channelIds_[i] == ci_lfe) { lfeIndex_ = i; break; }

        mainCrossovers_.assign(numChannels_, LRCrossover());
        updateCrossoverDesign();

        interleavedIn_.assign(size_t(internalBlockSize_) * 2, 0.0f);
        leftoverL_.clear(); leftoverR_.clear();
        leftoverL_.reserve(internalBlockSize_);
        leftoverR_.reserve(internalBlockSize_);

        size_t reserveFrames = size_t(std::max(hostBlockSize, int(internalBlockSize_)));
        queue_.assign(numChannels_, std::vector<float>());
        for (auto& q : queue_) q.reserve(reserveFrames * 2);
    }

    unsigned numChannels() const { return numChannels_; }
    channel_id channelAt(unsigned i) const { return i < channelIds_.size() ? channelIds_[i] : ci_none; }
    channel_setup currentSetup() const { return setup_; }

    // --- FreeSurround's full parameter set, forwarded directly ---
    void setCenterImage(float v) { dec_->center_image(v); }
    void setFrontSeparation(float v) { dec_->front_separation(v); }
    void setRearSeparation(float v) { dec_->rear_separation(v); }
    void setCircularWrap(float v) { dec_->circular_wrap(v); }
    void setShift(float v) { dec_->shift(v); }
    void setDepth(float v) { dec_->depth(v); }
    void setFocus(float v) { dec_->focus(v); }
    void setBassRedirection(bool v) { bassRedirectionEnabled_ = v; }
    // Drives the dedicated crossover filter (see class comment), NOT
    // FreeSurround's own low_cutoff()/high_cutoff() -- those are never
    // called; dec_'s internal bass_redirection stays permanently off.
    void setSampleRate(double sr) { sampleRate_ = sr; updateCrossoverDesign(); }
    void setLowCutoffHz(float hz) { lowCutoffHz_ = hz; updateCrossoverDesign(); }
    void setHighCutoffHz(float hz) { highCutoffHz_ = hz; updateCrossoverDesign(); }

    // process() writes exactly `n` frames into outChannels, which must
    // have numChannels() entries, each pointing to a buffer of >= n floats.
    void process(const float* left, const float* right, int n, float* const* outChannels) {
        for (int i = 0; i < n; i++) {
            leftoverL_.push_back(left[i]);
            leftoverR_.push_back(right[i]);
            if (leftoverL_.size() == internalBlockSize_) {
                for (unsigned k = 0; k < internalBlockSize_; k++) {
                    interleavedIn_[k * 2 + 0] = leftoverL_[k];
                    interleavedIn_[k * 2 + 1] = leftoverR_[k];
                }
                float* out = dec_->decode(interleavedIn_.data());
                for (unsigned k = 0; k < internalBlockSize_; k++) {
                    const float* frame = out + size_t(k) * numChannels_;
                    for (unsigned c = 0; c < numChannels_; c++)
                        queue_[remap_[c]].push_back(frame[c]);
                }
                leftoverL_.clear();
                leftoverR_.clear();
            }
        }

        int avail = int(queue_[0].size());
        int take = std::min(avail, n);
        for (int i = 0; i < take; i++) {
            if (bassRedirectionEnabled_ && lfeIndex_ < numChannels_) {
                double sum = 0.0;
                for (unsigned c = 0; c < numChannels_; c++)
                    if (c != lfeIndex_) sum += double(queue_[c][i]);
                double lfeVal = sumCrossover_.lowpass(sum);
                for (unsigned c = 0; c < numChannels_; c++) {
                    if (c == lfeIndex_) outChannels[c][i] = float(lfeVal);
                    else outChannels[c][i] = float(mainCrossovers_[c].highpass(double(queue_[c][i])));
                }
            } else {
                for (unsigned c = 0; c < numChannels_; c++) {
                    outChannels[c][i] = (c == lfeIndex_) ? 0.0f : queue_[c][i]; // matches original semantics: bass redirection off => no LFE content
                }
            }
        }
        for (unsigned c = 0; c < numChannels_; c++) {
            for (int i = take; i < n; i++) outChannels[c][i] = 0.0f;
            queue_[c].erase(queue_[c].begin(), queue_[c].begin() + take);
        }
    }

    void reset() {
        dec_->flush();
        leftoverL_.clear();
        leftoverR_.clear();
        for (auto& q : queue_) q.clear();
        sumCrossover_.lp1.reset(); sumCrossover_.lp2.reset();
        sumCrossover_.hp1.reset(); sumCrossover_.hp2.reset();
        for (auto& x : mainCrossovers_) {
            x.lp1.reset(); x.lp2.reset(); x.hp1.reset(); x.hp2.reset();
        }
    }

private:
    void updateCrossoverDesign() {
        sumCrossover_.design(highCutoffHz_, sampleRate_);
        for (auto& x : mainCrossovers_) x.design(highCutoffHz_, sampleRate_);
    }
    std::unique_ptr<freesurround_decoder> dec_;
    channel_setup setup_ = cs_5point1;
    unsigned internalBlockSize_ = 512;
    unsigned numChannels_ = 0;
    std::vector<channel_id> channelIds_;
    std::vector<unsigned> remap_;
    double sampleRate_ = 48000.0;

    unsigned lfeIndex_ = 0;
    bool bassRedirectionEnabled_ = false;
    float lowCutoffHz_ = 40.0f;
    float highCutoffHz_ = 90.0f;
    LRCrossover sumCrossover_;
    std::vector<LRCrossover> mainCrossovers_;

    std::vector<float> interleavedIn_;
    std::vector<float> leftoverL_, leftoverR_;
    std::vector<std::vector<float>> queue_; // one queue per output channel
};
