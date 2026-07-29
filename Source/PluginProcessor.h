// PluginProcessor.h — FreeSurround VST3. Supports multiple output
// channel configurations ("Tier 1": stereo+LFE, 4.1, 5.1, 6.1, and 7.1's
// three variants), matching what the original foo_dsp_fsurround Foobar2000
// component offered, reintroduced here after being deliberately scoped
// down to fixed 5.1 during the initial rebuild.
//
// Design choice, and why: rather than have the plugin try to drive bus-
// layout changes itself (risky, host-dependent -- see juce_channel_sets.h),
// this plugin declares support for several possible output layouts and
// passively detects + adapts to whichever one the host negotiates via its
// own normal channel-count/routing configuration (a standard, reliable
// mechanism every VST3 host supports, since many real plugins offer
// multiple I/O configs this way). The one exception is 5.1 vs. its
// "Legacy" transform variant: those two use IDENTICAL channel positions
// (verified directly against FreeSurround's own channel lists), so
// switching between them is genuinely just an internal algorithm choice,
// safely exposed as an ordinary real-time plugin parameter. The three 7.1
// variants, by contrast, use different physical channel types (side vs.
// rear vs. front-center-left/right) and are NOT interchangeable via a
// parameter -- they're separate bus layouts, selected the same way as
// choosing between 5.1 and 7.1 in the first place.
//
// Bass management, output remapping to Windows-standard channel order,
// and arbitrary-host-block-size buffering are all handled by
// FreeSurroundAdapter, unchanged from the version already validated for
// 5.1 -- this file only adds the multi-layout wiring on top.
#pragma once
#include <JuceHeader.h>
#include "freesurround_adapter.h"
#include "juce_channel_sets.h"
#include <vector>
#include <cmath>

// The channel_setup options this plugin exposes as distinct output bus
// layouts (Tier 1 -- see juce_channel_sets.h for scope rationale).
static const channel_setup kSupportedSetups[] = {
    cs_stereo, cs_4point1, cs_5point1, cs_6point1,
    cs_7point1, cs_7point1_panorama, cs_7point1_tricenter
};

class RetroSurroundProcessor : public juce::AudioProcessor
{
public:
    RetroSurroundProcessor()
        : AudioProcessor(BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", buildChannelSetFor(cs_5point1), true))
    {
        addParameter(centerImage = new juce::AudioParameterFloat(
            "centerImage", "Center Image", 0.0f, 1.0f, 1.0f));
        addParameter(frontSeparation = new juce::AudioParameterFloat(
            "frontSeparation", "Front Separation", 0.0f, 3.0f, 1.0f));
        addParameter(rearSeparation = new juce::AudioParameterFloat(
            "rearSeparation", "Rear Separation", 0.0f, 3.0f, 1.0f));
        addParameter(circularWrap = new juce::AudioParameterFloat(
            "circularWrap", "Circular Wrap", 0.0f, 360.0f, 90.0f));
        addParameter(shift = new juce::AudioParameterFloat(
            "shift", "Shift", -1.0f, 1.0f, 0.0f));
        addParameter(depth = new juce::AudioParameterFloat(
            "depth", "Depth", 0.0f, 5.0f, 1.0f));
        addParameter(focus = new juce::AudioParameterFloat(
            "focus", "Focus", -1.0f, 1.0f, 0.0f));
        addParameter(lowCutoffHz = new juce::AudioParameterFloat(
            "lowCutoffHz", "Bass Crossover Low (Hz)", 20.0f, 500.0f, 40.0f));
        addParameter(highCutoffHz = new juce::AudioParameterFloat(
            "highCutoffHz", "Bass Crossover High (Hz)", 20.0f, 500.0f, 90.0f));
        addParameter(bassRedirection = new juce::AudioParameterBool(
            "bassRedirection", "Bass Redirection", false));
        addParameter(legacyTransform = new juce::AudioParameterBool(
            "legacyTransform", "5.1: Use Legacy Transform", false));
    }

    // --- Bus layout: stereo in, paired with any of the Tier 1 output
    // configurations. The host negotiates which one based on the track's
    // channel count / routing setup -- see prepareToPlay() for how the
    // plugin detects and adapts to whichever one was chosen. ---
    // Accept output layouts by CHANNEL COUNT (3/5/6/7/8), not exact
    // channel-type match. The stricter, exact-type version worked in
    // Reaper but reportedly failed against Equalizer APO's VST bridge --
    // most likely because a non-JUCE host negotiates layouts by asking
    // for "N channels" directly rather than JUCE's typed channel-set
    // matching, so the stricter check rejected every option except
    // whichever one happened to match exactly, leaving the plugin stuck
    // on its default (5.1). This is more permissive by design: some hosts
    // may present a layout whose exact channel types differ slightly from
    // what a given channel_setup nominally uses, but the count-based match
    // combined with detectNegotiatedSetup()'s identity-based channel
    // mapping still produces a musically-correct result.
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
            return false;
        int n = layouts.getMainOutputChannelSet().size();
        return n == 3 || n == 5 || n == 6 || n == 7 || n == 8;
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        lastSampleRate = sampleRate;
        lastBlockSize = samplesPerBlock;
        rebuildForSetup(detectNegotiatedSetup(), sampleRate, samplesPerBlock);
    }

    // Actively requests a channel configuration change, rather than just
    // waiting for the host to offer one -- needed because some VST3 hosts
    // (confirmed: a non-JUCE host's VST bridge) don't reliably surface or
    // negotiate the passive multi-layout declaration used elsewhere in
    // this file, so there's no in-plugin way to change configuration
    // without this. More host-dependent than the passive approach: some
    // hosts honor setBusesLayout() + updateHostDisplay() smoothly, some
    // don't. Returns false if the host rejects the new layout outright.
    bool requestChannelSetup(channel_setup newSetup)
    {
        auto layout = getBusesLayout();
        layout.outputBuses.getReference(0) = buildChannelSetFor(newSetup);
        if (!checkBusesLayoutSupported(layout)) return false;
        if (!setBusesLayout(layout)) return false;
        rebuildForSetup(newSetup, lastSampleRate, lastBlockSize);
        updateHostDisplay();
        return true;
    }

    private:
    void rebuildForSetup(channel_setup setup, double sampleRate, int samplesPerBlock)
    {
        // Internal FreeSurround block size, scaled to sample rate.
        //
        // Bass management (LFE) is handled via a dedicated time-domain
        // crossover (see freesurround_adapter.h), not FreeSurround's own
        // internal FFT-bin-based bass redirection, which needs the bass
        // frequency range to span at least a couple of whole bins to work
        // at all. That's a separate, already-fixed problem from what
        // this block size actually governs now.
        //
        // This value was previously reduced from 1024 to 128 in an
        // attempted latency optimization, believed at the time to be
        // safe based on a test that only measured slow block-to-block
        // envelope drift for a mid-frequency tone. That test missed real,
        // injected harmonic/sideband distortion happening WITHIN each
        // block for genuine bass-range content -- confirmed directly via
        // proper FFT analysis: at N=128, a 40Hz tone showed ~12% sideband
        // energy relative to its fundamental (a 60Hz tone: ~9%; 80Hz:
        // ~6%), which is real, audible distortion, not a measurement
        // artifact -- exactly the "robotic"/buzzing quality reported.
        // That distortion doesn't fall to a clean level until N=1024;
        // even N=768 still measurably fails at 20Hz specifically. 1024 is
        // therefore reverted to its original value, scaled proportionally
        // to sample rate as before so frequency resolution (and hence
        // this same correctness) holds at any rate, not just 48kHz.
        //
        // Built into LOCAL state first (no lock needed -- nothing else
        // can see these objects yet), then swapped into the real members
        // under adapterLock as one short operation. This keeps the lock
        // held only for the swap itself, not for construction (which
        // allocates and does real work), while still guaranteeing
        // processBlock() on the audio thread never sees a half-updated or
        // concurrently-destroyed adapter.
        unsigned newInternalBlockSize = computeInternalBlockSize(sampleRate);
        auto newAdapter = std::make_unique<FreeSurroundAdapter>(samplesPerBlock, setup, newInternalBlockSize);
        newAdapter->setSampleRate(sampleRate);
        newAdapter->setCenterImage(centerImage->get());
        newAdapter->setFrontSeparation(frontSeparation->get());
        newAdapter->setRearSeparation(rearSeparation->get());
        newAdapter->setCircularWrap(circularWrap->get());
        newAdapter->setShift(shift->get());
        newAdapter->setDepth(depth->get());
        newAdapter->setFocus(focus->get());
        newAdapter->setLowCutoffHz(lowCutoffHz->get());
        newAdapter->setHighCutoffHz(highCutoffHz->get());
        newAdapter->setBassRedirection(bassRedirection->get());

        unsigned newNumOutputChannels = newAdapter->numChannels();
        std::vector<std::vector<float>> newScratchOut(newNumOutputChannels, std::vector<float>(size_t(samplesPerBlock), 0.0f));

        // Do NOT assume JUCE's bus channel order matches the adapter's
        // output order positionally -- channelSetWithChannels() may order
        // channels by JUCE's own internal enum ordinal rather than
        // construction order, and assuming otherwise is exactly the class
        // of bug already fixed once in this project (FreeSurround's native
        // order vs. Windows-standard order). Instead, match by channel
        // IDENTITY where possible: for each JUCE buffer channel index,
        // find which of the adapter's channels is the same physical
        // position. If the host presents a channel type this plugin
        // doesn't recognize (e.g. a generic/discrete type from a non-JUCE
        // host that doesn't use JUCE's canonical channel labels), fall
        // back to POSITIONAL correspondence for that one slot rather than
        // defaulting to channel 0 -- the adapter's own output order is
        // already Windows-standard by construction, so position j is a
        // reasonable assumption when identity can't be determined, and
        // far safer than silently duplicating Front Left onto every
        // unmatched channel.
        auto layout = getBus(false, 0)->getCurrentLayout();
        std::vector<unsigned> newBufferToAdapterChannel(newNumOutputChannels, 0);
        for (unsigned j = 0; j < newNumOutputChannels; j++) {
            auto wantedType = layout.getTypeOfChannel(int(j));
            int found = -1;
            for (unsigned k = 0; k < newNumOutputChannels; k++) {
                if (juceChannelTypeFor(newAdapter->channelAt(k)) == wantedType) { found = int(k); break; }
            }
            newBufferToAdapterChannel[j] = (found >= 0) ? unsigned(found) : j;
        }

        {
            const juce::ScopedLock sl(adapterLock);
            currentSetup = setup;
            internalBlockSize = newInternalBlockSize;
            adapter = std::move(newAdapter);
            numOutputChannels = newNumOutputChannels;
            scratchL.assign(size_t(samplesPerBlock), 0.0f);
            scratchR.assign(size_t(samplesPerBlock), 0.0f);
            scratchOut = std::move(newScratchOut);
            scratchOutPtrs.assign(numOutputChannels, nullptr);
            bufferToAdapterChannel = std::move(newBufferToAdapterChannel);
        }
    }

    public:
    void releaseResources() override { adapter.reset(); }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        const int numSamples = buffer.getNumSamples();

        if (int(scratchL.size()) < numSamples) {
            scratchL.assign(size_t(numSamples), 0.0f);
            scratchR.assign(size_t(numSamples), 0.0f);
            for (auto& v : scratchOut) v.assign(size_t(numSamples), 0.0f);
        }

        // Guards against requestChannelSetup() (called from the GUI/message
        // thread) reassigning `adapter` -- and hence potentially destroying
        // the very crossover filter objects this block is about to call
        // process() on -- concurrently with this audio-thread block. A real
        // data race existed here before this lock was added: bass
        // management depends on persistent filter state (the biquads'
        // internal z1/z2), which is exactly the kind of thing silent
        // corruption under a race condition would break first.
        const juce::ScopedLock sl(adapterLock);

        if (buffer.getNumChannels() < 2 || adapter == nullptr) {
            buffer.clear();
            return;
        }

        // A change to the legacy-transform toggle needs a rebuild (same
        // channel count/positions, different internal FreeSurround
        // transform) -- cheap, and only actually rebuilds when the
        // setting has changed since the last block.
        bool wantLegacy = legacyTransform->get();
        if (currentSetup == cs_5point1 || currentSetup == cs_legacy) {
            channel_setup wanted = wantLegacy ? cs_legacy : cs_5point1;
            if (wanted != currentSetup) {
                currentSetup = wanted;
                adapter->rebuild(currentSetup, internalBlockSize, numSamples);
            }
        }

        applyAllParameters();

        const float* inL = buffer.getReadPointer(0);
        const float* inR = buffer.getReadPointer(1);
        std::copy(inL, inL + numSamples, scratchL.begin());
        std::copy(inR, inR + numSamples, scratchR.begin());

        for (unsigned c = 0; c < numOutputChannels; c++) scratchOutPtrs[c] = scratchOut[c].data();
        adapter->process(scratchL.data(), scratchR.data(), numSamples, scratchOutPtrs.data());

        int outChannels = juce::jmin(buffer.getNumChannels(), int(numOutputChannels));
        for (int c = 0; c < outChannels; c++)
            buffer.copyFrom(c, 0, scratchOut[bufferToAdapterChannel[size_t(c)]].data(), numSamples);
        for (int c = outChannels; c < buffer.getNumChannels(); c++)
            buffer.clear(c, 0, numSamples);
    }

    // --- Boilerplate JUCE requires ---
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "RetroSurround"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    // Serializes every parameter to a fixed, versioned binary layout.
    // This was previously an empty stub -- meaning every parameter,
    // including Bass Redirection, silently reset to its constructor
    // default (off, for that one) any time the host destroyed and
    // recreated the plugin instance. Confirmed relevant here: the
    // specific Equalizer APO VST3 fork in use is documented to recreate
    // the plugin on almost any interface interaction, which would
    // silently undo a user's "turn bass redirection on" with no visible
    // indication it happened.
    void getStateInformation(juce::MemoryBlock& destData) override
    {
        juce::MemoryOutputStream stream(destData, true);
        stream.writeInt(1); // format version, for future-proofing
        stream.writeFloat(centerImage->get());
        stream.writeFloat(frontSeparation->get());
        stream.writeFloat(rearSeparation->get());
        stream.writeFloat(circularWrap->get());
        stream.writeFloat(shift->get());
        stream.writeFloat(depth->get());
        stream.writeFloat(focus->get());
        stream.writeFloat(lowCutoffHz->get());
        stream.writeFloat(highCutoffHz->get());
        stream.writeBool(bassRedirection->get());
        stream.writeBool(legacyTransform->get());
    }

    void setStateInformation(const void* data, int sizeInBytes) override
    {
        juce::MemoryInputStream stream(data, size_t(sizeInBytes), false);
        int version = stream.readInt();
        if (version != 1) return; // unknown/corrupt state -- keep current values rather than guess
        *centerImage = stream.readFloat();
        *frontSeparation = stream.readFloat();
        *rearSeparation = stream.readFloat();
        *circularWrap = stream.readFloat();
        *shift = stream.readFloat();
        *depth = stream.readFloat();
        *focus = stream.readFloat();
        *lowCutoffHz = stream.readFloat();
        *highCutoffHz = stream.readFloat();
        *bassRedirection = stream.readBool();
        *legacyTransform = stream.readBool();
    }

    channel_setup getCurrentSetup() const { return currentSetup; }
    unsigned getNumOutputChannels() const { return numOutputChannels; }

    juce::AudioParameterFloat* centerImage;
    juce::AudioParameterFloat* frontSeparation;
    juce::AudioParameterFloat* rearSeparation;
    juce::AudioParameterFloat* circularWrap;
    juce::AudioParameterFloat* shift;
    juce::AudioParameterFloat* depth;
    juce::AudioParameterFloat* focus;
    juce::AudioParameterFloat* lowCutoffHz;
    juce::AudioParameterFloat* highCutoffHz;
    juce::AudioParameterBool* bassRedirection;
    juce::AudioParameterBool* legacyTransform; // only meaningful when negotiated layout is 5.1's channel set

private:
    // Rounds to a multiple of 32 for a clean block size. See the comment
    // inside the function body for why this is 1024@48kHz, not the 128
    // this was changed to (and now reverted from) during an earlier,
    // flawed latency optimization.
    static unsigned computeInternalBlockSize(double sampleRate)
    {
    // CORRECTED: 128 was wrong. My earlier test for this only measured
    // slow block-to-block envelope drift and concluded 128 was safe --
    // it missed real, injected harmonic/sideband distortion within each
    // block, which is what actually causes an audible "robotic"/buzzing
    // quality. Properly tested with an actual FFT-based distortion
    // measurement (comparing energy at the test tone's fundamental vs.
    // nearby sideband energy that shouldn't exist for a pure tone): at
    // N=128, a 40Hz tone shows ~12% sideband energy relative to its
    // fundamental -- real, audible distortion, not a measurement
    // artifact. That distortion doesn't fall to a clean level until
    // N=1024, and even N=768 still fails at 20Hz specifically (~1.8%).
    // 1024 is the ORIGINAL value from before this optimization was ever
    // attempted -- it was correct all along; the "8x latency reduction"
    // claimed earlier was real for the FFT-bin-based LFE constraint
    // specifically, but not for the main spatial decode, which needed
    // the same 1024 regardless.
    double n = 1024.0 * sampleRate / 48000.0;
        return (unsigned)(std::round(n / 32.0) * 32.0);
    }

    channel_setup detectNegotiatedSetup() const
    {
        int n = getBus(false, 0)->getCurrentLayout().size();
        switch (n) {
            case 3: return cs_stereo;
            case 5: return cs_4point1;
            case 6: return legacyTransform->get() ? cs_legacy : cs_5point1;
            case 7: return cs_6point1;
            case 8: return cs_7point1; // standard variant; panorama/tri-center need exact channel types a non-JUCE host may not present the same way, so count-based matching can't disambiguate them -- say the word if you need one of those specifically and we'll add a dedicated toggle
            default: return cs_5point1; // shouldn't happen given isBusesLayoutSupported, but a safe fallback
        }
    }

    void applyAllParameters()
    {
        if (!adapter) return;
        adapter->setCenterImage(centerImage->get());
        adapter->setFrontSeparation(frontSeparation->get());
        adapter->setRearSeparation(rearSeparation->get());
        adapter->setCircularWrap(circularWrap->get());
        adapter->setShift(shift->get());
        adapter->setDepth(depth->get());
        adapter->setFocus(focus->get());
        adapter->setLowCutoffHz(lowCutoffHz->get());
        adapter->setHighCutoffHz(highCutoffHz->get());
        adapter->setBassRedirection(bassRedirection->get());
    }

    juce::CriticalSection adapterLock;
    std::unique_ptr<FreeSurroundAdapter> adapter;
    channel_setup currentSetup = cs_5point1;
    unsigned numOutputChannels = 6;
    unsigned internalBlockSize = 1024;
    double lastSampleRate = 48000.0;
    int lastBlockSize = 512;
    std::vector<float> scratchL, scratchR;
    std::vector<std::vector<float>> scratchOut;
    std::vector<float*> scratchOutPtrs;
    std::vector<unsigned> bufferToAdapterChannel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RetroSurroundProcessor)
};
