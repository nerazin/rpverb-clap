// RP-Verb CLAP parameters: one per panel control, with the original property tag as the CLAP id.
#pragma once
#include <cstdint>

namespace rpverb_clap {

enum : uint32_t { kIdPower = 200, kIdReset = 201, kIdSideInput = 202 };

struct ParamDef {
    uint32_t id;             // original property tag (1..127) or one of the ids above
    const char* name;
    const char* module;
    double def;
    int steps;               // 0: continuous 0..1; otherwise integer 0..steps-1
    const char* const* labels;
    int cv;                  // CV input this knob's modulation drives (-1: none)
};

inline const char* const kOffOn[] = {"Off", "On"};
inline const char* const kPower[] = {"Off", "On", "Bypass"};
inline const char* const kRouting[] = {"Direct", "Into Reverb"};
inline const char* const kEnsembleRouting[] = {"Into Reverb", "Direct + Reverb"};
inline const char* const kEnvelopeModes[] = {"Audio Control", "Space Size", "Reverb Length", "Size + Length",
                                             "Size + Length + Audio"};
inline const char* const kSpaces[] = {"Hall Clean", "Hall Cave", "Hall Swirl", "Hall Wild 1", "Hall Wild 2",
                                      "Room Cold", "Room Edged 1", "Room Edged 2", "Room Storm 1", "Room Storm 2",
                                      "Room Storm 3", "Vintage Type 1", "Vintage Type 2", "Vintage Type 3",
                                      "Space Orbit 1", "Space Orbit 2", "Space Orbit 3", "Space Orbit 4"};
inline const char* const kColours[] = {"Clean", "Multi 01", "Multi 02", "Multi 03", "Swirl"};

// Defaults are the original's init patch (presets/00_init in the lab). Dry/Wet is not saved in
// patches: 0.5 here is unverified.
inline const ParamDef kParams[] = {
    {kIdPower, "Power", "Global", 1, 3, kPower, -1},
    {kIdReset, "Reset", "Global", 0, 2, kOffOn, -1},
    {8, "Dry/Wet", "Global", 0.5, 0, nullptr, 0},
    {54, "Reverb", "Reverb", 1, 2, kOffOn, -1},
    {2, "Pre-Delay", "Reverb", 0.534999847412109375, 0, nullptr, -1},
    {13, "Pre-Delay Disorder", "Reverb", 0, 0, nullptr, -1},
    {3, "Space Size", "Reverb", 0.679999828338623046875, 0, nullptr, 5},
    {9, "Space Disorder", "Reverb", 1, 0, nullptr, -1},
    {4, "Reverb Length", "Reverb", 1, 0, nullptr, 6},
    {5, "Reverb Damping", "Reverb", 0.2399997711181640625, 0, nullptr, 8},
    {93, "Reverb Volume", "Reverb", 0.856999874114990234375, 0, nullptr, -1},
    {49, "Diffusion", "Reverb", 1, 0, nullptr, 7},
    {6, "Low-Pass", "Reverb", 1, 0, nullptr, -1},
    {7, "High-Pass", "Reverb", 0, 0, nullptr, -1},
    {94, "Space Type", "Reverb", 0, 18, kSpaces, -1},
    {95, "Colour", "Reverb", 0, 5, kColours, -1},
    {70, "Early Reflections", "Early", 0, 2, kOffOn, -1},
    {73, "Early Volume", "Early", 1, 0, nullptr, 3},
    {71, "Early Routing", "Early", 0, 2, kRouting, -1},
    {72, "Early Length", "Early", 0.929999828338623046875, 0, nullptr, -1},
    {75, "Early Damping", "Early", 0.184999942779541015625, 0, nullptr, -1},
    {74, "Early Feedback", "Early", 0.309999942779541015625, 0, nullptr, -1},
    {90, "Early Side", "Early", 0.5299999713897705078125, 0, nullptr, -1},
    {92, "Early Cross", "Early", 0.49499988555908203125, 0, nullptr, -1},
    {83, "Late Reflection", "Late", 0, 2, kOffOn, -1},
    {86, "Late Volume", "Late", 0.769999980926513671875, 0, nullptr, 4},
    {84, "Late Routing", "Late", 0, 2, kRouting, -1},
    {85, "Late Length", "Late", 0.8199999332427978515625, 0, nullptr, -1},
    {87, "Late Damping", "Late", 0.3399999141693115234375, 0, nullptr, -1},
    {37, "Envelope", "Envelope", 0, 2, kOffOn, -1},
    {43, "Envelope Mode", "Envelope", 0, 5, kEnvelopeModes, -1},
    {48, "Mono Input", "Envelope", 1, 2, kOffOn, -1},
    {47, "Latch Envelope", "Envelope", 0, 2, kOffOn, -1},
    {44, "Attack", "Envelope", 1, 0, nullptr, -1},
    {45, "Hold", "Envelope", 0.4399998188018798828125, 0, nullptr, -1},
    {46, "Release", "Envelope", 0.559999942779541015625, 0, nullptr, -1},
    {97, "Envelope Amount", "Envelope", 1, 0, nullptr, -1},
    {28, "Ensemble", "Ensemble", 0, 2, kOffOn, -1},
    {30, "Ensemble Volume", "Ensemble", 1, 0, nullptr, 1},
    {29, "Ensemble Routing", "Ensemble", 1, 2, kEnsembleRouting, -1},
    {50, "Distortion", "Distortion", 0, 2, kOffOn, -1},
    {53, "Distortion Amount", "Distortion", 0, 0, nullptr, 2},
    {39, "EQ", "EQ", 0, 2, kOffOn, -1},
    {40, "EQ Low", "EQ", 0.5, 0, nullptr, -1},
    {41, "EQ Mid", "EQ", 0.5, 0, nullptr, -1},
    {42, "EQ High", "EQ", 0.5, 0, nullptr, -1},
    {38, "EQ Post Audio", "EQ", 0, 2, kOffOn, -1},
    {100, "HQ Mode", "Back Panel", 1, 2, kOffOn, -1},
    {105, "Reset on Stop", "Back Panel", 1, 2, kOffOn, -1},
    {kIdSideInput, "Side Input", "Back Panel", 0, 2, kOffOn, -1},
};
constexpr int kParamCount = int(sizeof kParams / sizeof kParams[0]);

inline int paramIndex(uint32_t id) {
    for (int i = 0; i < kParamCount; i++) if (kParams[i].id == id) return i;
    return -1;
}

}  // namespace rpverb_clap
