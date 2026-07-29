// windows_channel_order.h — maps FreeSurround's internal channel_id
// values to the standard Windows/WASAPI multichannel speaker order (the
// ascending bit position of the WAVEFORMATEXTENSIBLE SPEAKER_* masks --
// SPEAKER_FRONT_LEFT=0x1, SPEAKER_FRONT_RIGHT=0x2, SPEAKER_FRONT_CENTER=0x4,
// SPEAKER_LOW_FREQUENCY=0x8, SPEAKER_BACK_LEFT=0x10, SPEAKER_BACK_RIGHT=0x20,
// etc.). This is the order Windows itself -- and therefore any physical
// output device or soundbar receiving a "5.1" stream from Windows --
// expects to receive channels in.
//
// FreeSurround's own channel_id enum is NOT in this order: for cs_5point1
// it produces L, C, R, Ls, Rs, LFE, while Windows expects L, R, C, LFE,
// Ls, Rs. Writing FreeSurround's output straight to a device's output
// channels without reordering scrambles the result (Center plays from
// the Right speaker, LFE plays from the Left-Surround speaker, etc.) --
// exactly the "channels all wrong" symptom this file fixes.
#pragma once
#include "freesurround_decoder.h"

// Position in standard Windows channel order for a given FreeSurround
// channel_id (0 = first channel Windows expects, i.e. Front Left).
// FreeSurround's side/back-center variants beyond what Windows' standard
// mask distinguishes (used only in its densest 9.1+ setups, which don't
// map onto any standard Windows speaker configuration regardless) are
// given a sensible adjacent position so front/back grouping is at least
// preserved rather than left undefined.
inline int windowsSpeakerOrder(channel_id id)
{
    switch (id) {
        case ci_front_left:         return 0;  // SPEAKER_FRONT_LEFT
        case ci_front_right:        return 1;  // SPEAKER_FRONT_RIGHT
        case ci_front_center:       return 2;  // SPEAKER_FRONT_CENTER
        case ci_lfe:                return 3;  // SPEAKER_LOW_FREQUENCY
        case ci_back_left:          return 4;  // SPEAKER_BACK_LEFT
        case ci_back_right:         return 5;  // SPEAKER_BACK_RIGHT
        case ci_front_center_left:  return 6;  // SPEAKER_FRONT_LEFT_OF_CENTER
        case ci_front_center_right: return 7;  // SPEAKER_FRONT_RIGHT_OF_CENTER
        case ci_back_center:        return 8;  // SPEAKER_BACK_CENTER
        case ci_side_center_left:   return 9;  // SPEAKER_SIDE_LEFT
        case ci_side_center_right:  return 10; // SPEAKER_SIDE_RIGHT
        case ci_side_front_left:    return 11;
        case ci_side_front_right:   return 12;
        case ci_side_back_left:     return 13;
        case ci_side_back_right:    return 14;
        case ci_back_center_left:   return 15;
        case ci_back_center_right: return 16;
        default:                    return 17; // ci_none / unrecognized
    }
}

inline const char* channelDisplayName(channel_id id)
{
    switch (id) {
        case ci_front_left:         return "Front Left";
        case ci_front_right:        return "Front Right";
        case ci_front_center:       return "Front Center";
        case ci_lfe:                return "LFE";
        case ci_back_left:          return "Back Left";
        case ci_back_right:         return "Back Right";
        case ci_front_center_left:  return "Front Center-Left";
        case ci_front_center_right: return "Front Center-Right";
        case ci_back_center:        return "Back Center";
        case ci_side_center_left:   return "Side Left";
        case ci_side_center_right:  return "Side Right";
        case ci_side_front_left:    return "Side Front-Left";
        case ci_side_front_right:   return "Side Front-Right";
        case ci_side_back_left:     return "Side Back-Left";
        case ci_side_back_right:    return "Side Back-Right";
        case ci_back_center_left:   return "Back Center-Left";
        case ci_back_center_right: return "Back Center-Right";
        default:                    return "Unknown";
    }
}
