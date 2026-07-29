// juce_channel_sets.h — builds juce::AudioChannelSet objects directly
// from FreeSurround's own channel_at() results, rather than trying to
// match FreeSurround's setups to JUCE's named presets (create6point1(),
// create7point1(), etc.). That matching is genuinely risky: JUCE
// distinguishes leftSurround (classic/back), leftSurroundSide (explicit
// side), and leftSurroundRear (explicit rear) as different positions, and
// none of JUCE's named 6.1/7.1 presets line up exactly with FreeSurround's
// own channel semantics for every setup. Building each channel set
// directly from FreeSurround's verified channel list guarantees positional
// correctness by construction instead of by assumption.
//
// Scope ("Tier 1"): the setups that map onto standard, few-enough-channel
// JUCE ChannelTypes with high confidence -- stereo+LFE, 4.1, 5.1 (and its
// channel-identical cs_legacy variant), 6.1, and 7.1's three variants
// (standard side+back, panorama, tri-center). The denser 8.1-and-beyond
// setups need speaker positions most consumer hardware doesn't have and
// aren't included here.
#pragma once
#include "freesurround_decoder.h"
#include <JuceHeader.h>

inline juce::AudioChannelSet::ChannelType juceChannelTypeFor(channel_id id)
{
    using CT = juce::AudioChannelSet::ChannelType;
    switch (id) {
        case ci_front_left:         return CT::left;
        case ci_front_right:        return CT::right;
        case ci_front_center:       return CT::centre;
        case ci_lfe:                return CT::LFE;
        case ci_back_left:          return CT::leftSurround;       // FreeSurround's plain "back" == JUCE's classic surround position
        case ci_back_right:         return CT::rightSurround;
        case ci_side_center_left:   return CT::leftSurroundSide;
        case ci_side_center_right:  return CT::rightSurroundSide;
        case ci_back_center:        return CT::centreSurround;
        case ci_front_center_left:  return CT::leftCentre;
        case ci_front_center_right: return CT::rightCentre;
        default:                    return CT::unknown; // Tier 2 channels (side-front, side-back, back-center-left/right) -- not built here
    }
}

// Builds a JUCE AudioChannelSet matching a FreeSurround channel_setup's
// exact channel list. Returns an empty/disabled set if any channel in the
// setup falls outside Tier 1 (defensive -- so an unsupported setup fails
// an isBusesLayoutSupported() check cleanly rather than silently building
// a wrong or incomplete layout).
inline juce::AudioChannelSet buildChannelSetFor(channel_setup setup)
{
    juce::Array<juce::AudioChannelSet::ChannelType> types;
    unsigned n = freesurround_decoder::num_channels(setup);
    for (unsigned i = 0; i < n; i++) {
        auto ct = juceChannelTypeFor(freesurround_decoder::channel_at(setup, i));
        if (ct == juce::AudioChannelSet::unknown) return juce::AudioChannelSet::disabled();
        types.add(ct);
    }
    return juce::AudioChannelSet::channelSetWithChannels(types);
}
