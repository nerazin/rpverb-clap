// Deterministic stimulus for the golden test. The lab drives the original RPVerb image with the same
// stimulus and prints the hash tests/golden.cpp expects, so the test pins the original's behaviour
// without shipping it.
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include "rpverb_device.hpp"

namespace rpverb_golden {

struct Prop { int tag; int steps; double def; };  // steps 0: continuous 0..1
inline const Prop kProps[] = {
    {94, 18, 0}, {95, 5, 0}, {43, 5, 0}, {105, 2, 1}, {100, 2, 1}, {8, 0, 0.5}, {28, 2, 0}, {30, 0, 1}, {29, 2, 1},
    {50, 2, 0}, {53, 0, 0}, {70, 2, 0}, {73, 0, 1}, {72, 0, 0.929999828338623046875}, {75, 0, 0.184999942779541015625},
    {74, 0, 0.309999942779541015625}, {90, 0, 0.5299999713897705078125}, {92, 0, 0.49499988555908203125}, {71, 2, 0},
    {54, 2, 1}, {3, 0, 0.679999828338623046875}, {4, 0, 1}, {2, 0, 0.534999847412109375}, {9, 0, 1},
    {5, 0, 0.2399997711181640625}, {93, 0, 0.856999874114990234375}, {13, 0, 0}, {49, 0, 1}, {6, 0, 1}, {7, 0, 0},
    {83, 2, 0}, {86, 0, 0.769999980926513671875}, {85, 0, 0.8199999332427978515625}, {87, 0, 0.3399999141693115234375},
    {84, 2, 0}, {37, 2, 0}, {44, 0, 1}, {45, 0, 0.4399998188018798828125}, {46, 0, 0.559999942779541015625},
    {97, 0, 1}, {48, 2, 1}, {47, 2, 0}, {39, 2, 0}, {40, 0, 0.5}, {41, 0, 0.5}, {42, 0, 0.5}, {38, 2, 0}};
constexpr int kPropCount = int(sizeof kProps / sizeof kProps[0]);
constexpr long kBatches = 6000;

struct Stim {
    uint64_t s;
    double value[128]{};
    int power = 1;
    float level = 0.3f;
    bool conn[4] = {true, true, false, false};
    bool playing = false, cvConn[10]{};
    double cv[10]{}, rate = 44100;

    explicit Stim(uint64_t seed) : s(seed) { for (auto& p : kProps) value[p.tag] = p.def; }
    uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return uint32_t(s >> 33); }
    double uni() { return next() * (1.0 / 2147483648.0); }
    int below(int n) { return int(next() % uint32_t(n)); }
    float sample() { return float(uni() * 2.0 - 1.0) * level; }

    double pick(const Prop& p) {
        if (p.steps) return below(p.steps);
        const int k = below(6);
        return k == 0 ? 0.0 : (k == 1 ? 1.0 : uni());
    }
    void change(std::vector<int>& tags, std::vector<double>& vals, const Prop& p) {
        value[p.tag] = pick(p);
        tags.push_back(p.tag);
        vals.push_back(value[p.tag]);
    }

    // One 64-frame batch: inputs, host state and the property changes (tag, new value).
    void batch(long b, rpverb::BatchIO& io, std::vector<int>& tags, std::vector<double>& vals) {
        tags.clear(); vals.clear();
        if (b == 0) {
            for (auto& p : kProps) { tags.push_back(p.tag); vals.push_back(value[p.tag]); }
        } else if (below(400) == 0) {
            for (int k = 0; k < 12; k++) change(tags, vals, kProps[below(kPropCount)]);
        } else if (below(6) == 0) {
            const int n = 1 + below(3);
            for (int k = 0; k < n; k++) change(tags, vals, kProps[below(kPropCount)]);
        }
        if (b % 40 == 0) { static const float lv[] = {0, 1e-6f, 1e-3f, 0.1f, 0.5f, 1.0f, 0.3f, 0.3f}; level = lv[below(8)]; }
        const bool impulse = below(30) == 0;
        for (int i = 0; i < rpverb::kBatch; i++) {
            for (int c = 0; c < 4; c++) io.in[c][i] = sample();
            if (impulse && i == 0) io.in[0][i] = io.in[1][i] = 1.0f;
        }
        if (below(500) == 0) conn[below(2)] ^= 1;
        if (!conn[0] && !conn[1]) conn[0] = true;
        if (below(300) == 0) conn[2] = conn[3] = !conn[2];
        if (below(150) == 0) playing = !playing;
        if (b > 0 && below(3000) == 0) rate = rate == 44100 ? 48000 : 44100;
        for (int c = 0; c < 10; c++) {
            if (below(200) == 0) cvConn[c] = !cvConn[c];
            if (below(10) == 0) cv[c] = uni() * 2.0 - 1.0;
        }
        if (below(600) == 0) power = below(3);
        if (power != 1 && below(100) == 0) power = 1;
        memcpy(io.conn, conn, sizeof conn);
        io.playing = playing;
        io.sampleRate = rate;
        memcpy(io.cv, cv, sizeof cv);
        memcpy(io.cvConn, cvConn, sizeof cvConn);
        io.powerMode = power;
        io.resetButton = below(700) == 0;
        io.space = value[94]; io.colour = value[95]; io.envelopeMode = value[43];
        io.changes = int(tags.size());
    }
};

inline uint64_t fnv(uint64_t h, const void* d, size_t n) {
    const uint8_t* p = static_cast<const uint8_t*>(d);
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}

}  // namespace rpverb_golden
