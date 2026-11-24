// RP-Verb core: a hand port of the RPVerb 1.0.7 Rack Extension DSP (x86-64 build).
// The state is one block with the original object's layout, so every field can be checked
// against the original byte for byte. Offsets in comments and names are the original's.
// Build with -ffp-contract=off -fno-fast-math -fno-strict-aliasing: same float operations in
// the same order as the original.
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace rpverb {

constexpr size_t kObjectSize = 0x19c9b80;

template<class T> inline T& at(void* p, size_t off) { return *reinterpret_cast<T*>(static_cast<uint8_t*>(p) + off); }
inline float& F(void* p, size_t off) { return at<float>(p, off); }
inline int32_t& I(void* p, size_t off) { return at<int32_t>(p, off); }
inline uint8_t& U8(void* p, size_t off) { return at<uint8_t>(p, off); }
inline double& D(void* p, size_t off) { return at<double>(p, off); }
inline uint8_t* P(void* p, size_t off) { return static_cast<uint8_t*>(p) + off; }

// The original imports cos, log10, powf and sin from the system libm. Call the same functions through
// volatile pointers so the compiler cannot fold a call with constant arguments to a correctly rounded
// value that libm does not return (it did: powf(2, -4/8.3) came out 1 ulp off).
inline float powF(float x, float y) { static float (*const volatile f)(float, float) = ::powf; return f(x, y); }
inline double cosLib(double x) { static double (*const volatile f)(double) = ::cos; return f(x); }
inline double sinLib(double x) { static double (*const volatile f)(double) = ::sin; return f(x); }
inline double log10Lib(double x) { static double (*const volatile f)(double) = ::log10; return f(x); }
inline float cosD(float x) { return float(cosLib(double(x))); }

// ---------------------------------------------------------------- tables (copied, owner-approved)
// 0xb440: EQ history rotation, rows of (newest, previous, oldest) indices
constexpr int32_t kRotation[4][3] = {{2, 1, 0}, {0, 2, 1}, {1, 0, 2}, {0, 0, 0}};
// constant band normalisers written by the EQ design (0x3880), as float bits
constexpr uint32_t kEqNorm[10] = {0x3f313a93, 0x3f2c3c9f, 0x3f2c0ebf, 0x3f286595, 0x3f28ef35,
                                  0x3f29e1b1, 0x3f2a3055, 0x3f28adac, 0x3f263886, 0x3f52b021};

// 0xb470: Colour presets (Clean, Multi 01, Multi 02, Multi 03, Swirl), 12 modulator values each, float bits
constexpr uint32_t kColour[5][12] = {
    {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0x3ecccccd, 0x3d4ede55, 0x3e3a2e7f, 0x3f333333, 0x3cf83dff, 0x3df83dff, 0x3f800000, 0x3cf83dff, 0x3e4ccccd, 0x3f666666, 0x3d783dff, 0x3e1b26c0},
    {0x3f638e3b, 0x3d783dff, 0x3d257eaa, 0x3ef3120a, 0x3d4ede55, 0x3cf83dff, 0x3eba2e7f, 0x3d257eaa, 0x3ca57eaa, 0x3f800000, 0x3d4ede55, 0x3cf83dff},
    {0x3f2aaab0, 0x3de38e2a, 0x3df83dff, 0x3f638e3b, 0x3dba2e7f, 0x3e10ced5, 0x3f800000, 0x3de38e2a, 0x3d4ede55, 0x3f5e6245, 0x3dcede55, 0x3de38e2a},
    {0x3f2aaab0, 0x3e8ba2e0, 0x3df83dff, 0x3f638e3b, 0x3e95faca, 0x3e10ced5, 0x3f800000, 0x3e8676ea, 0x3d4ede55, 0x3f5e6245, 0x3e814af5, 0x3de38e2a},
};
// 0xb560: Space presets (Hall Clean ... Space Orbit 4), 10 values each, float bits
constexpr uint32_t kSpace[18][10] = {
    {0x3ca57eaa, 0x3ca3d70a, 0x3c23d70a, 0x3c23d70a, 0x3ca3d70a, 0x3cf5c28f, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0x3ca57eaa, 0x3f49b270, 0x3ca57eaa, 0x3f1b26d0, 0x3cf83dff, 0x3f3f5a86, 0x3f800000, 0x00000000, 0x00000000, 0x00000000},
    {0x3ca57eaa, 0x3f1364e0, 0x3ca57eaa, 0x3f03e0f0, 0x3ca57eaa, 0x3f1890d6, 0x3f800000, 0x00000000, 0x00000000, 0x00000000},
    {0x3d4ede55, 0x3ebf5a75, 0x3cf83dff, 0x3ee8ba1f, 0x3d257eaa, 0x3ef83e21, 0x3f800000, 0x00000000, 0x00000000, 0x00000000},
    {0x3df83dff, 0x3c257eaa, 0x3dcede55, 0x3ca57eaa, 0x3dba2e7f, 0x3c257eaa, 0x3f800000, 0x00000000, 0x00000000, 0x00000000},
    {0x3f0147ae, 0x3d257eaa, 0x3ef837b5, 0x3ca57eaa, 0x3efd70a4, 0x3c257eaa, 0x00000000, 0x3f800000, 0x00000000, 0x00000000},
    {0x3e10cb29, 0x3e44866a, 0x3e3a2c67, 0x3c257eaa, 0x3e638866, 0x3e8ba2e0, 0x00000000, 0x3f800000, 0x00000000, 0x00000000},
    {0x3e257a78, 0x3e44866a, 0x3dba29c7, 0x3e638e2a, 0x3e10cdc8, 0x3d90ced5, 0x00000000, 0x3f800000, 0x00000000, 0x00000000},
    {0x3efd6388, 0x3f800000, 0x3f800000, 0x3c257a78, 0x3ef837b5, 0x3f5e5c92, 0x00000000, 0x3f800000, 0x00000000, 0x00000000},
    {0x3f800000, 0x3f3f559b, 0x3f7d696e, 0x3f3793de, 0x3f7ad40b, 0x3f3a29c7, 0x3f800000, 0x3f800000, 0x00000000, 0x00000000},
    {0x3f800000, 0x3f7ad384, 0x3f7d696e, 0x3f800000, 0x3f7ad40b, 0x3f783e10, 0x3f800000, 0x3f800000, 0x00000000, 0x00000000},
    {0x3ca57eaa, 0x3ca3d70a, 0x3c23d70a, 0x3c23d70a, 0x3ca3d70a, 0x3cf5c28f, 0x00000000, 0x00000000, 0x3f800000, 0x00000000},
    {0x3dcede55, 0x3c257eaa, 0x3df83dff, 0x3cf83dff, 0x3da57eaa, 0x3ca57eaa, 0x00000000, 0x00000000, 0x3f800000, 0x00000000},
    {0x3ca57eaa, 0x3f1364e0, 0x3ca57eaa, 0x3f03e0f0, 0x3ca57eaa, 0x3f1890d6, 0x3f800000, 0x00000000, 0x3f800000, 0x00000000},
    {0x3d4ede55, 0x3ebf5a75, 0x3cf83dff, 0x3ee8ba1f, 0x3d257eaa, 0x3ef83e21, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000},
    {0x3ca57eaa, 0x3f49b270, 0x3ca57eaa, 0x3f1b26d0, 0x3cf83dff, 0x3f3f5a86, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000},
    {0x3ca57eaa, 0x3f1364e0, 0x3ca57eaa, 0x3f03e0f0, 0x3ca57eaa, 0x3f1890d6, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000},
    {0x3d4ede55, 0x3ebf5a75, 0x3cf83dff, 0x3ee8ba1f, 0x3d257eaa, 0x3ef83e21, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000},
};

inline float clip01(float v) { return (0.0f > v) ? 0.0f : ((1.0f < v) ? 1.0f : v); }  // ucomiss/ja then minss 1, v
inline float bitsF(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

// ---------------------------------------------------------------- EQ (state block at 0x1688e34)
// 0x3880: band-pass biquads for 10 normalised frequencies x[k] = f/fs
inline void eqDesign(uint8_t* e, const float x[10]) {
    float r = 0, c = 0;
    for (int k = 0; k < 10; k++) {
        // clear the band's history, then write its coefficients
        if (k == 0) {
            F(e, 0x220) = 0; F(e, 0x21c) = 0; F(e, 0x218) = 0;
            F(e, 0x230) = 0; F(e, 0x22c) = 0; F(e, 0x228) = 0;
            I(e, 0x14) = 0x3f99999a;  // 1.2
        }
        F(e, 0x120 + 16 * k) = 0; F(e, 0x11c + 16 * k) = 0; F(e, 0x118 + 16 * k) = 0;
        F(e, 0x20 + 16 * k) = 0; F(e, 0x1c + 16 * k) = 0; F(e, 0x18 + 16 * k) = 0;
        I(e, 0x338 + 4 * k) = int32_t(kEqNorm[k]);
        const float w = x[k] * 6.283185005187988f;
        const float t = w * 0.5f;
        const float one2 = k < 3 ? 1.2f : F(e, 0x14);
        const float two4 = k < 3 ? 2.4000000953674316f : one2 + one2;
        const float num = one2 - t;
        const float den = w + two4;
        r = num / den;
        const float r2 = r + 0.5f;
        c = cosD(w);
        c = c * r2;
        F(e, 0x278 + 4 * k) = 0.5f - r;
        F(e, 0x2b8 + 4 * k) = c * -2.0f;
        F(e, 0x2f8 + 4 * k) = r + r;
    }
    at<uint64_t>(e, 0) = 0;
    I(e, 0x8) = 1; I(e, 0xc) = 2;
}

// 0x4120: band gain 10^((40v-20)/20), applied to the band's normaliser and its 0x278 term
inline void eqBandGain(uint8_t* e, int band, float v) {
    float a = v * 40.0f;
    a = a + -20.0f;
    a = a / 20.0f;
    float g = powF(10.0f, a);
    g = g * F(e, 0x338 + 4 * band);
    F(e, 0x378 + 4 * band) = g;
    g = g * F(e, 0x278 + 4 * band);
    F(e, 0x238 + 4 * band) = g;
}

// 0x4190: n band-passes in parallel per channel; outputs are the band sums
inline void eqProcess(uint8_t* e, int n, float inL, float inR, float* outL, float* outR) {
    int idx = I(e, 0) + 1;
    if (idx == 3) idx = 0;
    I(e, 0) = idx;
    I(e, 0x4) = kRotation[idx][0];
    I(e, 0x8) = kRotation[idx][1];
    I(e, 0xc) = kRotation[idx][2];
    const int a = I(e, 0x4), b = I(e, 0x8), c = I(e, 0xc);
    F(e, 0x218 + 4 * a) = inL;
    F(e, 0x228 + 4 * a) = inR;
    float sumL = 0, sumR = 0;
    if (n > 0) {
        const float d = F(e, 0x218 + 4 * a) - F(e, 0x218 + 4 * c);
        for (int k = 0; k < n; k++) {
            float y = F(e, 0x238 + 4 * k) * d;
            y = y - F(e, 0x2b8 + 4 * k) * F(e, 0x18 + 16 * k + 4 * b);
            y = y - F(e, 0x2f8 + 4 * k) * F(e, 0x18 + 16 * k + 4 * c);
            y = y + 1.0000000036274937e-15f;
            F(e, 0x18 + 16 * k + 4 * a) = y;
            sumL = sumL + y;
        }
        const float dR = F(e, 0x228 + 4 * a) - F(e, 0x228 + 4 * c);
        for (int k = 0; k < n; k++) {
            float y = F(e, 0x238 + 4 * k) * dR;
            y = y - F(e, 0x2b8 + 4 * k) * F(e, 0x118 + 16 * k + 4 * b);
            y = y - F(e, 0x2f8 + 4 * k) * F(e, 0x118 + 16 * k + 4 * c);
            y = y + 1.0000000036274937e-15f;
            F(e, 0x118 + 16 * k + 4 * a) = y;
            sumR = sumR + y;
        }
    }
    *outL = sumL;
    *outR = sumR;
}

// ---------------------------------------------------------------- envelope follower (0x260 bytes)
// fields: 0x4 gated peak, 0x8 previous, 0xc rms, 0x10 peak, 0x18 sum of squares, 0x1c peak decay,
// 0x20 rate scale, 0x24 attack step, 0x28 release step, 0x2c hold time, 0x34 hold scale,
// 0x3c level, 0x40 hold counter, 0x44 hold step, 0x4c ring index, 0x58 stage, 0x5c ring[64],
// 0x25c "latch off" flag
inline void followerReset(uint8_t* f, float holdScale, float rateScale) {  // 0x4430
    F(f, 0x34) = holdScale;
    I(f, 0x0) = 0; F(f, 0xc) = 0; F(f, 0x10) = 0; F(f, 0x4) = 0; F(f, 0x8) = 0;
    I(f, 0x4c) = 0;
    I(f, 0x30) = 0x3f800000;
    F(f, 0x48) = 0; F(f, 0x18) = 0;
    for (int i = 0; i < 64; i++) F(f, 0x5c + 4 * i) = 0;
    I(f, 0x1c) = 0x3f7fdf3b;
    I(f, 0x28) = 0x3f800000; I(f, 0x24) = 0x3f800000;
    F(f, 0x2c) = 0; F(f, 0x40) = 0;
    I(f, 0x44) = 0x447a0000;
    F(f, 0x20) = rateScale;
    I(f, 0x58) = 2;
    F(f, 0x3c) = 0;
    U8(f, 0x25c) = 0;
    memset(f, 0, 16);
    I(f, 0x10) = 0;
}

inline void followerSetDecay(uint8_t* f, float v, uint8_t latchOff) {  // 0x44f0
    const uint8_t old = U8(f, 0x25c);
    U8(f, 0x25c) = latchOff;
    if (old != latchOff) { memset(f, 0, 16); I(f, 0x10) = 0; }
    if (latchOff) { I(f, 0x1c) = 0x3f7fdf3b; return; }
    I(f, 0x1c) = (-0.5f > v) ? 0x3f7fdf3b : 0x3f7c0000;
}

inline void followerClear(uint8_t* f) {  // 0x4540
    F(f, 0xc) = 0; F(f, 0x8) = 0; F(f, 0x4) = 0;
    I(f, 0x4c) = 0; F(f, 0x18) = 0;
    for (int i = 0; i < 64; i++) F(f, 0x5c + 4 * i) = 0;
    F(f, 0x40) = 0;
    I(f, 0x58) = 2;
    U8(f, 0x25c) = 0;
    F(f, 0x3c) = 0;
}

inline void followerGate(uint8_t* f) {  // 0x45a0
    if (F(f, 0x4) > 0.0f) I(f, 0x58) = 0;
}

inline float followerRate(float v) {  // shared by 0x45c0 / 0x4610
    float e = v * 14.0f;
    float p = powF(2.0f, e);
    return p + 32.0f;
}
inline void followerSetAttack(uint8_t* f, float v) { const float s = F(f, 0x20); F(f, 0x24) = s / followerRate(v); }   // 0x45c0
inline void followerSetRelease(uint8_t* f, float v) { const float s = F(f, 0x20); F(f, 0x28) = s / followerRate(v); }  // 0x4610
inline void followerSetHold(uint8_t* f, float v) {  // 0x4660
    float e = v * 14.0f;
    float p = powF(2.0f, e);
    p = p / 1000.0f;
    p = p * F(f, 0x34);
    p = p + 32.0f;
    F(f, 0x2c) = p;
    F(f, 0x44) = (p > 1.0000000116860974e-07f) ? 1000.0f / p : 1000.0f;
}

inline void followerProcess(uint8_t* f, float x) {  // 0x46c0
    F(f, 0x8) = F(f, 0x4);
    const float x2 = x * x;
    const int ri = I(f, 0x4c);
    float s = F(f, 0x18) - F(f, 0x5c + 4 * ri);
    s = s + x2;
    F(f, 0x18) = (0.0f > s) ? 0.0f : s;  // maxss 0, s
    F(f, 0x5c + 4 * ri) = x2;
    I(f, 0x4c) = (ri + 1) & 0x3f;
    float m = F(f, 0x18) * 0.015625f;
    const float rms = float(::sqrt(double(m)));
    F(f, 0xc) = rms;
    float pk = F(f, 0x10);
    pk = (rms > pk) ? rms : pk * F(f, 0x1c);
    F(f, 0x10) = pk;
    const float g = (pk < 0.009999999776482582f) ? 0.0f : pk;
    F(f, 0x4) = g;
    if (U8(f, 0x25c) != 0) {  // latch off
        if (g > 0.0f && 0.009999999776482582f > F(f, 0x8) && I(f, 0x58) == 2 && 9.999999747378752e-05f > F(f, 0x3c))
            I(f, 0x58) = 0;
        switch (uint32_t(I(f, 0x58))) {
        case 0: {
            const float l = F(f, 0x3c) + F(f, 0x24);
            F(f, 0x3c) = l;
            if (!(l < 1.0f) && !std::isnan(l)) { I(f, 0x58) = 1; F(f, 0x3c) = 1.0f; F(f, 0x40) = 1000.0f; }
            return;
        }
        case 1: {
            const float h = F(f, 0x40) - F(f, 0x44);
            F(f, 0x40) = h;
            if (0.0f > h) I(f, 0x58) = 2;
            return;
        }
        case 2: {
            const float l = F(f, 0x3c) - F(f, 0x28);
            F(f, 0x3c) = l;
            F(f, 0x40) = 0;
            if (0.0f > l) F(f, 0x3c) = 0;
            return;
        }
        case 3: {
            const float l = F(f, 0x3c) + -0.05000000074505806f;
            F(f, 0x3c) = l;
            if (0.0f > l) { F(f, 0x3c) = 0; I(f, 0x58) = 0; }
            return;
        }
        default: return;
        }
    }
    // latch on
    if (g > 0.0f && 0.009999999776482582f > F(f, 0x8) && I(f, 0x58) == 2) I(f, 0x58) = 0;
    if (g == 0.0f && F(f, 0x3c) > 0.9900000095367432f) I(f, 0x58) = 2;
    const int st = I(f, 0x58);
    if (st == 2) {
        const float l = F(f, 0x3c) - F(f, 0x28);
        F(f, 0x3c) = l;
        F(f, 0x40) = 0;
        if (0.0f > l) F(f, 0x3c) = 0;
    } else if (st == 0) {
        const float l = F(f, 0x3c) + F(f, 0x24);
        F(f, 0x3c) = l;
        if (l > 1.0f) F(f, 0x3c) = 1.0f;
    }
}

// ---------------------------------------------------------------- small device entry points
constexpr size_t kParams = 0x1f4c;  // float value per property slot
inline float& param(uint8_t* o, int slot) { return F(o, kParams + 4 * size_t(slot)); }
inline void setSlot(uint8_t* o, int slot, float v) { param(o, slot) = v; }  // 0x48e0
inline void setSidechain(uint8_t* o, float l, float r) { F(o, 0x19c9b4c) = l; F(o, 0x19c9b50) = r; }  // 0x51d0
inline void setSidechainConnected(uint8_t* o, uint8_t c) { U8(o, 0x19c9b78) = c; }  // 0x51f0
constexpr size_t kFollowerMono = 0x16891f4, kFollowerL = 0x1689454, kFollowerR = 0x16896b4;
inline void envelopeGate(uint8_t* o, uint8_t g) {  // 0x48f0
    const uint8_t old = U8(o, 0x19c9b79);
    U8(o, 0x19c9b79) = g;
    if (old == g || g != 1) return;
    followerGate(P(o, kFollowerL));
    followerGate(P(o, kFollowerR));
    followerGate(P(o, kFollowerMono));
}

// ---------------------------------------------------------------- reverb instance (0x5a05c0 bytes)
// 0x0 comb feedback, 0x4 damping, 0x8 sample rate, 0x1c/0x20 input delay L/R, 0x24 / 0x40024 input
// delay lines (65536 each), 0x80024 per-comb length modulators, 0x8004c output gain, 0x80054 tone
// flag, 0x80058 comb count, 0x8005c comb gain, 0x80060 / 0x300290 comb banks A / B, allpasses L at
// 0x5804c0 + i*0x4014 and R at 0x590510 + i*0x4014, 0x5a0560 / 0x5a0588 comb lengths L / R,
// 0x5a05b0 input delay write index.
struct V4 { float v[4]; };
inline V4 ld(uint8_t* p, size_t off) { V4 r; memcpy(r.v, p + off, 16); return r; }
inline void st(uint8_t* p, size_t off, const V4& a) { memcpy(p + off, a.v, 16); }
inline V4 splat(float x) { return {{x, x, x, x}}; }
inline V4 operator*(const V4& a, const V4& b) { return {{a.v[0] * b.v[0], a.v[1] * b.v[1], a.v[2] * b.v[2], a.v[3] * b.v[3]}}; }
inline V4 operator+(const V4& a, const V4& b) { return {{a.v[0] + b.v[0], a.v[1] + b.v[1], a.v[2] + b.v[2], a.v[3] + b.v[3]}}; }
inline V4 operator-(const V4& a, const V4& b) { return {{a.v[0] - b.v[0], a.v[1] - b.v[1], a.v[2] - b.v[2], a.v[3] - b.v[3]}}; }

constexpr size_t kBankA = 0x80060, kBankB = 0x300290;
constexpr size_t kApL = 0x5804c0, kApR = 0x590510, kApStride = 0x4014;

inline void combBankInit(uint8_t* c) {  // 0x8a30
    I(c, 0x1d8) = 0;
    for (size_t off : {0x90, 0xa0, 0xb0, 0x110, 0x120, 0x130, 0x140, 0x150, 0x160, 0xe0, 0xf0, 0x100}) st(c, off, splat(0));
    for (int k = 0; k < 10; k++) I(c, 0x1b0 + 4 * k) = 1;
    st(c, 0x170, splat(1.0f)); st(c, 0x180, splat(0)); st(c, 0x190, splat(0)); st(c, 0x1a0, splat(1.0f));
    memset(c + 0x230, 0, 5000 * 4);  // only the first 500 rows of the comb lines
}

// one bank: five stereo combs, as lanes (L a, L b, R a, R b) x2 and (L c, -, R c, -)
inline void combBank(uint8_t* c, float* accL, float* accR, float inL, float dl, float inR, float dr) {  // 0xaa90
    const int32_t w = I(c, 0x1d8);
    float d[10];
    for (int k = 0; k < 10; k++) {
        const int32_t ri = int32_t(uint32_t(w) - uint32_t(I(c, 0x1b0 + 4 * k)));
        I(c, 0x1dc + 4 * k) = ri;
        d[k] = ri > 0 ? F(c, 0x230 + 40 * size_t(uint16_t(ri)) + 4 * k) : 0.0f;
        F(c, 0x208 + 4 * k) = d[k];
    }
    const V4 v0 = {{d[0], d[1], d[5], d[6]}}, v1 = {{d[2], d[3], d[7], d[8]}}, v2 = {{d[4], 0, d[9], 0}};
    st(c, 0x0, v0); st(c, 0x10, v1); st(c, 0x20, v2);
    const V4 in1 = {{inL, dl, inR, dr}}, in2 = {{inL, 0, inR, 0}};
    st(c, 0x30, in1); st(c, 0x40, in1); st(c, 0x50, in2);
    const V4 a = ld(c, 0xe0), b = ld(c, 0xf0), f = ld(c, 0x100), g = ld(c, 0x170), h = ld(c, 0x190), k = ld(c, 0x180), m = ld(c, 0x1a0);
    auto lane = [&](size_t sOff, size_t uOff, size_t wOff, const V4& v, const V4& in, V4& y) {
        const V4 s = ld(c, sOff) * a + v * b;
        st(c, sOff, s);
        const V4 t = (s * f + in) * g;
        const V4 u = ld(c, uOff) * h + t;
        st(c, uOff, u);
        const V4 ww = ld(c, wOff) * m + k * u;
        st(c, wOff, ww);
        y = u - ww;
    };
    V4 y0, y1, y2;
    lane(0x90, 0x110, 0x140, v0, in1, y0);
    lane(0xa0, 0x120, 0x150, v1, in1, y1);
    lane(0xb0, 0x130, 0x160, v2, in2, y2);
    st(c, 0x60, y0); st(c, 0x70, y1); st(c, 0x80, y2);
    const size_t row = 0x230 + 40 * size_t(uint16_t(w));
    const float out[10] = {y0.v[0], y0.v[1], y1.v[0], y1.v[1], y2.v[0], y0.v[2], y0.v[3], y1.v[2], y1.v[3], y2.v[2]};
    for (int q = 0; q < 10; q++) F(c, row + 4 * q) = out[q];
    I(c, 0x1d8) = int32_t(uint32_t(w) + 1);
    float l = *accL;
    for (int q = 0; q < 5; q++) l = l + F(c, 0x208 + 4 * q);
    *accL = l;
    float r = *accR;
    for (int q = 5; q < 10; q++) r = r + F(c, 0x208 + 4 * q);
    *accR = r;
}

// lengths of bank A are (L0..L4, R0..R4) and of bank B (L5..L9, R5..R9)
inline void storeCombLengths(uint8_t* R, const int32_t L[10], const int32_t Rr[10]) {
    for (int k = 0; k < 5; k++) {
        I(R, kBankA + 0x1b0 + 4 * k) = L[k];
        I(R, kBankA + 0x1c4 + 4 * k) = Rr[k];
        I(R, kBankB + 0x1b0 + 4 * k) = L[5 + k];
        I(R, kBankB + 0x1c4 + 4 * k) = Rr[5 + k];
    }
}

inline void reverbClear(uint8_t* R) {  // 0x7850
    combBankInit(R + kBankA);
    combBankInit(R + kBankB);
    for (int i = 0; i < 4; i++) {
        for (size_t base : {kApL, kApR}) {
            uint8_t* ap = R + base + i * kApStride;
            memset(ap, 0, 0x4000);
            I(ap, 0x4004) = 0; I(ap, 0x4008) = 0;
        }
    }
    F(R, 0x5a05b0) = 0;
    for (int i = 0; i < 500; i++) { F(R, 0x24 + 4 * i) = 0; F(R, 0x40024 + 4 * i) = 0; }
}

inline void reverbBuildLengths(uint8_t* R) {  // 0x72f0
    const float sr = F(R, 0x8);
    int32_t L[10], Rr[10];
    for (int i = 0; i < 10; i++) {
        L[i] = int32_t(F(R, 0x5a0560 + 4 * i) + 0.5f);
        Rr[i] = int32_t(F(R, 0x5a0588 + 4 * i) + 0.5f);
        F(R, 0x80024 + 4 * i) = 1.0f;
    }
    const float scale = sr / 44100.0f;
    storeCombLengths(R, L, Rr);
    for (size_t off : {0xf0, 0x100, 0x110, 0x170, 0x180, 0x190, 0x1a0, 0x1b0, 0x1c0, 0x140, 0x150, 0x160})
        st(R, 0x80000 + off, splat(0));
    I(R, 0x80238) = 0;
    memset(R + 0x80290, 0, 500 * 40);
    for (size_t off : {0x320, 0x330, 0x340, 0x3a0, 0x3b0, 0x3c0, 0x3d0, 0x3e0, 0x3f0, 0x370, 0x380, 0x390})
        st(R, 0x300000 + off, splat(0));
    I(R, 0x300468) = 0;
    memset(R + 0x3004c0, 0, 500 * 40);
    static const int32_t apL[4] = {556, 441, 341, 225}, apR[4] = {581, 466, 366, 250};  // 0xb230 (Freeverb tunings)
    for (int i = 0; i < 4; i++) {
        uint8_t* a = R + kApL + i * kApStride;
        I(a, 0x4000) = int32_t(float(apL[i]) * scale + 0.5f);
        I(a, 0x400c) = 0x3f000000;
        I(a, 0x4004) = 0; I(a, 0x4008) = 0;
        I(a, 0x4010) = 0x3f800000;
        memset(a, 0, 0x4000);
        uint8_t* b = R + kApR + i * kApStride;
        I(b, 0x4000) = int32_t(float(apR[i]) * scale + 0.5f);
        I(b, 0x400c) = 0x3f000000;
        I(b, 0x4004) = 0; I(b, 0x4008) = 0;
        I(b, 0x4010) = 0x3f800000;
        memset(b, 0, 0x4000);
    }
    reverbClear(R);
}

inline void reverbInit(uint8_t* R) {  // 0x70c0
    I(R, 0) = 0x3f6d2714;
    const float d0 = F(R, 4);
    st(R, 0x300370, splat(d0));
    st(R, 0x300380, splat(1.0f - d0));
    st(R, 0x300390, splat(bitsF(0x3f6d2714)));
    float x = F(R, 0x8) * 0.5f;
    x = x / 44100.0f;
    const float d = (1.0f < x) ? 0.5444999933242798f : x * 0.550000011920929f;
    F(R, 4) = d;
    st(R, 0x80140, splat(d));
    st(R, 0x80150, splat(1.0f - d));
    st(R, 0x80160, splat(bitsF(0x3f6d2714)));
    reverbBuildLengths(R);
    F(R, 0x14) = 0;
    F(R, 0x8004c) = 1.0f;
    F(R, 0x1c) = 0; F(R, 0x20) = 0;
    I(R, 0x5a05b0) = 0;
    U8(R, 0x80054) = 0;
    I(R, 0) = 0x3f266666;
    F(R, 4) = 1.0f;
    I(R, 0x80058) = 10;
    I(R, 0x8005c) = 0x3ccccccd;
    for (int i = 0; i < 10; i++) F(R, 0x80024 + 4 * i) = 1.0f;
}

inline void reverbSetFeedback(uint8_t* R, float v) {  // 0x71f0: 0.65 + 0.25 log10(1 + 23.5 v)
    float x = v * 23.5f;
    x = x + 1.0f;
    const float f = float(log10Lib(double(x)) * 0.25 + 0.6499999761581421);
    F(R, 0) = f;
    const float d = F(R, 4);
    st(R, 0x80140, splat(d)); st(R, 0x80150, splat(1.0f - d)); st(R, 0x80160, splat(f));
    st(R, 0x300370, splat(d)); st(R, 0x300380, splat(1.0f - d)); st(R, 0x300390, splat(f));
}

inline void reverbSetDamping(uint8_t* R, float v) {  // 0x7280 (bank A only)
    float x = v * F(R, 0x8);
    x = x / 44100.0f;
    const float d = (1.0f < x) ? 0.5444999933242798f : x * 0.550000011920929f;
    F(R, 4) = d;
    st(R, 0x80140, splat(d)); st(R, 0x80150, splat(1.0f - d)); st(R, 0x80160, splat(F(R, 0)));
}

inline void reverbSetCombs(uint8_t* R, int n, float g) { I(R, 0x80058) = n; F(R, 0x8005c) = g; }  // 0x7720

// 0x7730: new comb lengths for a new sample rate (L[0..9], R[0..9]); no-op when the rate is unchanged
inline void reverbSetLengths(uint8_t* R, float sr, const float len[20]) {
    const int32_t n = int32_t(sr);
    if (float(n) == 0.0f) return;
    if (n == int32_t(F(R, 0x8))) return;
    for (int i = 0; i < 20; i++) F(R, 0x5a0560 + 4 * i) = len[i];
    F(R, 0x8) = sr;
    reverbBuildLengths(R);
}

inline void reverbSetInputDelay(uint8_t* R, float l, float r) { F(R, 0x1c) = l; F(R, 0x20) = r; }  // 0x7ae0
inline void reverbSetCombMod(uint8_t* R, float a, float b) {  // 0x7ed0
    for (int i = 0; i < 5; i++) { F(R, 0x80024 + 8 * i) = a; F(R, 0x80028 + 8 * i) = b; }
}
inline void reverbSetOutputGain(uint8_t* R, float v) { F(R, 0x8004c) = v * F(R, 0x8005c); }  // 0x83c0
inline void reverbSetAllpassGain(uint8_t* R, float v) {  // 0x83e0
    const float g = v * 0.5f;
    for (int i = 0; i < 4; i++) { F(R, kApL + i * kApStride + 0x4010) = g; F(R, kApR + i * kApStride + 0x4010) = g; }
}

inline void toneCoeffs(float lo, float hi, float sr, float& A, float& B, float& Q, float& C) {
    float p = powF(2.0f, lo * 10.0f);
    p = p * 20.0f;
    p = p / sr;
    float q = powF(2.0f, hi * 10.0f);
    q = q * 20.0f;
    q = q / sr;
    const float p2 = p + p, q2 = q + q;
    A = (0.9998999834060669f < p2) ? 1.0f : p2;
    B = 1.0f - A;
    Q = (q2 < 9.999999747378752e-05f) ? 0.0f : q2;
    C = 1.0f - Q;
}
inline void reverbSetTone(uint8_t* R, float a, float b, float c, float d) {  // 0x7f30
    U8(R, 0x80054) = 0;
    const float lo1 = clip01(a - c), hi1 = clip01(c + b);
    if (0.9900000095367432f > lo1 || hi1 > 9.999999747378752e-05f) U8(R, 0x80054) = 1;
    const float sr = F(R, 0x8);
    float A, B, Q, C;
    toneCoeffs(lo1, hi1, sr, A, B, Q, C);
    st(R, 0x801d0, splat(A)); st(R, 0x801e0, splat(Q)); st(R, 0x801f0, splat(B)); st(R, 0x80200, splat(C));
    const float lo2 = clip01(a - d), hi2 = clip01(b + d);
    if (0.9900000095367432f > lo2 || hi2 > 0.009999999776482582f) U8(R, 0x80054) = 1;
    toneCoeffs(lo2, hi2, sr, A, B, Q, C);
    st(R, 0x300400, splat(A)); st(R, 0x300410, splat(Q)); st(R, 0x300420, splat(B)); st(R, 0x300430, splat(C));
}

inline void reverbScaleLengths(uint8_t* R, float x) {  // 0x81d0
    int32_t L[10], Rr[10];
    for (int i = 0; i < 10; i++) {
        float a = F(R, 0x5a0560 + 4 * i) * x;
        a = a * F(R, 0x80024 + 4 * i);
        L[i] = int32_t(a + 0.5f);
        float b = F(R, 0x5a0588 + 4 * i) * x;
        b = b * F(R, 0x80024 + 4 * i);
        Rr[i] = int32_t(b + 0.5f);
        if (L[i] == 0) L[i] = 1;
        if (Rr[i] == 0) Rr[i] = 1;
    }
    storeCombLengths(R, L, Rr);
}

inline float delayTap(uint8_t* R, size_t buf, float t, int32_t w) {
    const int32_t i = int32_t(t);
    const float fi = float(i);
    const int32_t a = int32_t(uint32_t(w) - uint32_t(i)), b = int32_t(uint32_t(w) + ~uint32_t(i));
    const float x0 = a >= 0 ? F(R, buf + 4 * size_t(uint16_t(a))) : 0.0f;
    const float fr = t - fi;
    const float x1 = b >= 0 ? F(R, buf + 4 * size_t(uint16_t(b))) : 0.0f;
    return fr * (x1 - x0) + x0;
}
inline void inputDelayRead(uint8_t* R, float* l, float* r) {  // 0x7af0
    const int32_t w = I(R, 0x5a05b0);
    *l = delayTap(R, 0x24, F(R, 0x1c), w);
    *r = delayTap(R, 0x40024, F(R, 0x20), w);
    I(R, 0x5a05b0) = int32_t(uint32_t(w) + 1);
}

inline float allpass(uint8_t* a, float x) {
    int32_t i = I(a, 0x4004) + 1;
    if (i == I(a, 0x4000)) i = 0;
    I(a, 0x4004) = i;
    const float bo = F(a, 4 * size_t(int64_t(i)));
    F(a, 4 * size_t(int64_t(i))) = F(a, 0x4010) * bo + x;
    return bo - x;
}
inline void reverbProcess(uint8_t* R, float inL, float inR, float* outL, float* outR) {  // 0x7ba0
    F(R, 0x24 + 4 * size_t(uint16_t(I(R, 0x5a05b0)))) = inL;
    F(R, 0x40024 + 4 * size_t(uint16_t(I(R, 0x5a05b0)))) = inR;
    float dl, dr;
    inputDelayRead(R, &dl, &dr);
    float accL = 0, accR = 0;
    U8(R, 0x80054) = 1;
    combBank(R + kBankA, &accL, &accR, inL, dl, inR, dr);
    if (I(R, 0x80058) >= 6) combBank(R + kBankB, &accL, &accR, inL, dl, inR, dr);
    *outL = accL; *outR = accR;
    float l = accL, r = accR;
    for (int i = 0; i < 4; i++) {
        l = allpass(R + kApL + i * kApStride, l);
        r = allpass(R + kApR + i * kApStride, r);
    }
    *outL = l * F(R, 0x8004c);
    *outR = r * F(R, 0x8004c);
}

// ---------------------------------------------------------------- controls

// 0x4940: copy the Colour row to slots 15..26 and the Space row to slots 61,62,64,65,67,68,96,98,103,104.
// shortcut: indices outside the tables are skipped; the original reads past them, but the host range is 0..4 / 0..17.
inline void applyPresets(uint8_t* o) {
    int c = int(param(o, 95));
    I(o, 0x1689930) = c;
    if (c >= 0 && c < 5)
        for (int k = 0; k < 12; k++) I(o, 0x1f88 + 4 * k) = int32_t(kColour[c][k]);
    int s = int(param(o, 94));
    I(o, 0x1689930) = s;
    if (s >= 0 && s < 18) {
        static const int slots[10] = {61, 62, 64, 65, 67, 68, 96, 98, 103, 104};
        for (int k = 0; k < 10; k++) I(o, kParams + 4 * slots[k]) = int32_t(kSpace[s][k]);
    }
}

constexpr size_t kEq = 0x1688e34;
inline void eqControl(uint8_t* o) {  // 0x8be0: three knobs spread over ten bands
    const float lo = param(o, 40) + -0.5f, mid = param(o, 41) + -0.5f, hi = -0.5f + param(o, 42);
    uint8_t* e = P(o, kEq);
    const float v0 = lo * 0.3330000042915344f + 0.5f;
    eqBandGain(e, 0, v0);
    const float v1 = lo * 0.6660000085830688f + 0.5f;
    eqBandGain(e, 1, v1);
    eqBandGain(e, 2, lo + 0.5f);
    const float m1 = mid * 0.3330000042915344f;
    eqBandGain(e, 3, v1 + m1);
    const float m2 = mid * 0.6660000085830688f;
    eqBandGain(e, 4, v0 + m2);
    eqBandGain(e, 5, mid + 0.5f);
    eqBandGain(e, 6, hi * 0.3330000042915344f + (m2 + 0.5f));
    const float h2 = hi * 0.6660000085830688f;
    eqBandGain(e, 7, (m1 + 0.5f) + h2);
    eqBandGain(e, 8, hi + 0.5f);
    eqBandGain(e, 9, h2 + 0.5f);
}

inline void envelopeControl(uint8_t* o) {  // 0x8da0
    const size_t order[3] = {kFollowerL, kFollowerR, kFollowerMono};
    for (size_t fo : order) {
        followerSetAttack(P(o, fo), param(o, 44));
        followerSetHold(P(o, fo), param(o, 45));
        followerSetRelease(P(o, fo), param(o, 46));
    }
    const uint8_t latchOff = 0.5f > param(o, 47);
    followerSetDecay(P(o, kFollowerMono), F(o, 0x16891f0), latchOff);
    followerSetDecay(P(o, kFollowerL), F(o, 0x16891f0), latchOff);
    followerSetDecay(P(o, kFollowerR), F(o, 0x16891f0), latchOff);
}

inline void ensembleSetup(uint8_t* o) {  // 0x4ac0: six voices
    const float s = param(o, 31) * 0.5f;
    float a = s + 0.5f;
    a = a + a;
    const float a2 = 2.0f - a;
    F(o, 0x1889a54) = clip01(a2);
    F(o, 0x1889a6c) = clip01(a);
    F(o, 0x1889a60) = clip01(a2);
    F(o, 0x1889a78) = clip01(a);
    float b = 0.5f - s;
    b = b + b;
    const float b2 = 2.0f - b;
    F(o, 0x1889a58) = clip01(b2);
    F(o, 0x1889a70) = clip01(b);
    F(o, 0x1889a64) = clip01(b2);
    F(o, 0x1889a7c) = clip01(b);
    F(o, 0x1889a5c) = 0; F(o, 0x1889a68) = 0; F(o, 0x1889a74) = 0; F(o, 0x1889a80) = 0;
    F(o, 0x27a4) = 1.0f - param(o, 34);
    const float sr = F(o, 0x2750);
    float w = 0.05000000074505806f * sr;
    w = w * param(o, 35);
    F(o, 0x27a0) = w + 1.0f;
    float q = powF(2.0f, param(o, 33) * 10.0f);
    q = q / sr;
    for (int i = 0; i < 6; i++) {
        const float fi = float(i);
        float e = F(o, 0x27a4) * -2.0f;
        e = e * fi;
        e = e / 5.300000190734863f;
        const float d = powF(2.0f, e) * 112640.0f;
        F(o, 0x1889a84 + 4 * i) = d;
        e = F(o, 0x27a4) * -2.0f;
        e = e * fi;
        e = e / 8.300000190734863f;
        const float g = powF(2.0f, e) * 0.9900000095367432f;
        F(o, 0x1889a9c + 4 * i) = g;
        e = F(o, 0x27a4) * -2.0f;
        e = e * fi;
        e = e / 5.0f;
        const float h = powF(2.0f, e) * F(o, 0x27a0);
        F(o, 0x1889a0c + 4 * i) = h;
        I(o, 0x1889acc + 4 * i) = int32_t(d * q);
        const float g2 = g * param(o, 32);
        const float om = 1.0f - g2;
        F(o, 0x1889a24 + 4 * i) = g2 * h;
        F(o, 0x1889a3c + 4 * i) = om * h;
    }
}

inline float octavesMinus1(float v, float s) { float p = powF(2.0f, v * 6.0f); p = p + -1.0f; return p * s; }
inline void modulatorControl(uint8_t* o) {  // 0x8eb0: Colour and Space values to depths and rates
    const float s = F(o, 0x275c);
    F(o, 0x1689984) = octavesMinus1(param(o, 15), s);
    F(o, 0x1689988) = octavesMinus1(param(o, 18), s);
    F(o, 0x168998c) = octavesMinus1(param(o, 21), s);
    F(o, 0x1689990) = octavesMinus1(param(o, 24), s);
    F(o, 0x1689944) = param(o, 16); F(o, 0x1689948) = param(o, 19);
    F(o, 0x168994c) = param(o, 22); F(o, 0x1689950) = param(o, 25);
    F(o, 0x1689954) = powF(2.0f, param(o, 17) * 10.0f) * 7.999999979801942e-06f;
    F(o, 0x1689958) = powF(2.0f, param(o, 20) * 10.0f) * 7.999999979801942e-06f;
    F(o, 0x168995c) = powF(2.0f, param(o, 23) * 10.0f) * 7.999999979801942e-06f;
    F(o, 0x1689960) = powF(2.0f, param(o, 26) * 10.0f) * 7.999999979801942e-06f;
    F(o, 0x18899ac) = param(o, 61) * 0.10000000149011612f;
    F(o, 0x18899b0) = param(o, 64) * 0.10000000149011612f;
    F(o, 0x18899b4) = 0.10000000149011612f * param(o, 67);
    if (param(o, 10) > 0.5f) { F(o, 0x18899ac) = 0; F(o, 0x18899b0) = 0; F(o, 0x18899b4) = 0; }
    constexpr float c = 0.1572864055633545f;
    const float a1 = param(o, 62) * 10.0f;
    const float p1 = powF(2.0f, a1) * c, p2 = powF(2.0f, a1 * 1.100000023841858f) * c;
    F(o, 0x18899b8) = p1; F(o, 0x18899c4) = p2;
    const float a2 = param(o, 65) * 10.0f;
    const float p3 = powF(2.0f, a2) * c, p4 = powF(2.0f, a2 * 1.100000023841858f) * c;
    F(o, 0x18899bc) = p3; F(o, 0x18899c8) = p4;
    const float a3 = 10.0f * param(o, 68);
    const float p5 = powF(2.0f, a3) * c, p6 = powF(2.0f, a3 * 0.8999999761581421f) * c;
    F(o, 0x18899c0) = p5; F(o, 0x18899cc) = p6;
    const float sw = param(o, 96);
    if (sw > 0.5f) {
        float m = sw * -0.5f;
        m = m + 1.0f;
        F(o, 0x18899b8) = p1 * m; F(o, 0x18899c4) = p2 * m; F(o, 0x18899bc) = p3 * m;
        F(o, 0x18899c8) = p4 * m; F(o, 0x18899c0) = p5 * m; F(o, 0x18899cc) = p6 * m;
    }
}

constexpr size_t kRev1 = 0x113ee0, kRev2 = 0x6b44a0, kRev3 = 0xc54a60;
constexpr size_t kWrite = 0x19c9b30;  // shared write index of the early/late, ensemble and pre-delay lines

inline float dbGain(float v) {  // 10^((60v-60)/20), 0 below 0.001 (tested in double)
    if (0.001 > double(v)) return 0.0f;
    float e = v * 60.0f;
    e = e + -60.0f;
    e = e / 20.0f;
    return powF(10.0f, e);
}

inline void delayControl(uint8_t* o) {  // 0x92c0: pre-delay taps, early and late reflections
    const float sr = F(o, 0x2750);
    const float ms = sr / 1000.0f;
    F(o, 0x275c) = ms; F(o, 0x2758) = ms;
    float pd = powF(2.0f, param(o, 2) * 12.0f);
    pd = pd + -1.0f;
    pd = pd * ms;
    I(o, 0x1688e20) = int32_t(pd);
    const double d0 = double(float(I(o, 0x1688e20)));
    const double x = double(param(o, 13));
    I(o, 0x1688e24) = int32_t((0.2 * x + 1.0) * d0);
    I(o, 0x1688e28) = int32_t((x * 0.1 + 1.0) * d0);
    float sz = clip01(param(o, 3) + F(o, 0x19c9b68));
    float e = (param(o, 98) > 0.5f ? 0.020999999716877937f : 0.05000000074505806f) * sz;
    if (param(o, 103) > 0.5f) e = 0.02500000037252903f * sz;
    if (param(o, 104) > 0.5f) e = sz * 0.02500000037252903f;
    e = e + 0.004999999888241291f;
    e = e * sr;
    float ln = param(o, 72) * 4.599999904632568f;
    ln = ln + -3.0f;
    ln = powF(2.0f, ln);
    ln = ln * e;
    const int32_t n1 = int32_t(ln) + 1;
    I(o, 0x2760) = n1;
    const int32_t n2 = int32_t(float(n1) * 1.5f) + 1;
    I(o, 0x2764) = n2;
    I(o, 0x2794) = 0x3f400000;
    F(o, 0x276c) = dbGain(clip01(param(o, 73) + F(o, 0x19c9b60)));
    const float a = param(o, 74) * 0.5f, b = param(o, 90) * 0.5f, c = 0.5f * param(o, 92);
    F(o, 0x2770) = a; F(o, 0x2778) = b; F(o, 0x2798) = c;
    float s = a + b;
    s = s + c;
    if (s > 0.9900000095367432f) {
        F(o, 0x2770) = a * 0.9900000095367432f / s;
        F(o, 0x2778) = b * 0.9900000095367432f / s;
        F(o, 0x2798) = 0.9900000095367432f * c / s;
    }
    auto damp = [&](float v) {
        float t = v * -0.25f;
        t = t + 1.0f;
        t = t * 10.0f;
        float p = powF(2.0f, t);
        p = p * 20.0f;
        p = p + p;
        p = p / sr;
        return (0.6000000238418579f < p) ? 1.0f : p;
    };
    const float A = damp(param(o, 75));
    F(o, 0x2774) = A;
    F(o, 0x277c) = 1.0f - A;
    I(o, 0x113eac) = n1; I(o, 0x113ea8) = n2;
    F(o, 0x113eb0) = a; F(o, 0x113eb4) = b; F(o, 0x279c) = c;
    if (s > 0.9900000095367432f) {
        F(o, 0x113eb0) = a * 0.9900000095367432f / s;
        F(o, 0x113eb4) = b * 0.9900000095367432f / s;
        F(o, 0x279c) = c * 0.9900000095367432f / s;
    }
    F(o, 0x113eb8) = A;
    F(o, 0x2780) = 1.0f - A;
    float lt = powF(2.0f, param(o, 85) * 12.0f);
    lt = lt + -1.0f;
    lt = lt * ms;
    I(o, 0x1689914) = int32_t(lt);
    F(o, 0x1689918) = dbGain(clip01(param(o, 86) + F(o, 0x19c9b64)));
    F(o, 0x168991c) = damp(param(o, 87));
}

inline void reverbControl(uint8_t* o) {  // 0x97b0: combs, tone, gains, distortion, enables, routing
    const float diff = param(o, 49) + F(o, 0x19c9b70);
    const bool ten = !(0.0f > diff) && (diff > 1.0f || diff > 0.5f);
    for (size_t R : {kRev1, kRev2, kRev3}) reverbSetCombs(P(o, R), ten ? 10 : 5, ten ? 0.02500000037252903f : 0.03500000014901161f);
    const float lp = param(o, 6), hp = param(o, 7);
    auto allpassGain = [&]() {
        const float dd = param(o, 49) + F(o, 0x19c9b70);
        if (0.0f > dd) return 0.10000000149011612f;
        float t = dd * 0.8999999761581421f;
        t = t + 0.10000000149011612f;
        return (1.0f < dd) ? 1.0f : t;
    };
    uint8_t* r1 = P(o, kRev1);
    reverbSetDamping(r1, clip01(param(o, 5) + F(o, 0x19c9b74)));
    reverbSetTone(r1, lp, hp, 0, 0);
    reverbSetOutputGain(r1, 1.0f);
    reverbSetAllpassGain(r1, 1.0f);
    uint8_t* r2 = P(o, kRev2);
    reverbSetDamping(r2, clip01(param(o, 5) + F(o, 0x19c9b74)));
    reverbSetTone(r2, lp, hp, (0.5f > param(o, 101)) ? 0.0f : param(o, 9) * 0.05000000074505806f, 0);
    reverbSetOutputGain(r2, dbGain(param(o, 11)));
    reverbSetAllpassGain(r2, allpassGain());
    uint8_t* r3 = P(o, kRev3);
    reverbSetDamping(r3, clip01(param(o, 5) + F(o, 0x19c9b74)));
    reverbSetTone(r3, lp, hp, 0, (0.5f > param(o, 102)) ? 0.0f : param(o, 9) * 0.05000000074505806f);
    reverbSetOutputGain(r3, dbGain(param(o, 14)));
    reverbSetAllpassGain(r3, allpassGain());
    // distortion: drive a = 2^(8x)-1 and makeup (1 - min(1.5x, 1)), floored at 0.2
    const float x = param(o, 53) + F(o, 0x19c9b5c);
    float m;
    if (0.0f > x) {
        F(o, 0x1688e2c) = 0;
        m = 0;
    } else {
        const float e = (x > 1.0f) ? 8.0f : 8.0f * x;
        float a = powF(2.0f, e);
        a = a + -1.0f;
        F(o, 0x1688e2c) = a;
        m = (1.0f < x) ? 1.5f : x * 1.5f;
    }
    const float mk = 1.0f - m;
    F(o, 0x1688e30) = (0.800000011920929f < m) ? 0.19999998807907104f : mk;
    if (0.5f > param(o, 50)) { F(o, 0x1688e2c) = 0; F(o, 0x1688e30) = 1.0f; }
    F(o, 0x19c9afc) = F(o, 0x19c9b00);
    F(o, 0x19c9b00) = param(o, 100);
    auto on = [&](int slot) -> uint8_t { return !(0.5f > param(o, slot)); };
    U8(o, 0x18899d8) = on(55); U8(o, 0x18899d9) = on(56); U8(o, 0x18899da) = on(57);
    if (0.5f > param(o, 100)) U8(o, 0x18899d9) = 0;  // HQ off: reverb 2 off
    if (0.5f > param(o, 54)) { U8(o, 0x18899d8) = 0; U8(o, 0x18899d9) = 0; U8(o, 0x18899da) = 0; }
    U8(o, 0x18899e8) = on(70); U8(o, 0x18899e9) = on(83); U8(o, 0x18899ea) = on(28);
    I(o, 0x18899e4) = int32_t(param(o, 29) * 1.9900000095367432f);
    I(o, 0x18899dc) = int32_t(param(o, 71) * 1.9900000095367432f);
    I(o, 0x18899e0) = int32_t(1.9900000095367432f * param(o, 84));
}

constexpr size_t kLfoTable = 0x1989ae4, kEnsTable = 0x1949ae4;
inline float lfoAt(uint8_t* o, float phase) { return F(o, kLfoTable + 4 * size_t(uint16_t(int32_t(phase)))); }

// 0x9ce0: envelope modes (see the RP-Verb map: the panel's buttons 1 and 2 are crossed and button 4
// does audio only; kept as in the original), comb modulation and size/length for the three reverbs
inline void envelopeRouting(uint8_t* o) {
    const int32_t mode = int32_t(param(o, 43) + 0.20000000298023224f);
    I(o, 0x16891ec) = mode;
    const float amt = param(o, 97);
    U8(o, 0x2768) = !(0.5f > param(o, 37));
    F(o, 0x16891f0) = -1.0f;
    float lenMul = 1.0f, sizeMul = 1.0f;
    if (!(0.5f > param(o, 37))) {
        const float env = F(o, kFollowerMono + 0x3c);
        if (mode == 0) {
            F(o, 0x16891f0) = amt;
        } else if (mode == 1) {
            float t = env * (0.5f * amt);
            lenMul = t + 1.0f;
        } else if (mode == 2) {
            float t = env * (0.5f * amt);
            sizeMul = t + 1.0f;
        } else if (mode == 3 || mode == 5) {
            const float h = amt * 0.5f;
            float t = env * h;
            sizeMul = t + 1.0f;
            t = env * h;
            lenMul = t + 1.0f;
        }
    }
    const float p0 = F(o, 0x18899b8) + F(o, 0x19c9ae4); F(o, 0x19c9ae4) = p0;
    const float p3 = F(o, 0x18899c4) + F(o, 0x19c9af0); F(o, 0x19c9af0) = p3;
    F(o, 0x19c9ae8) = F(o, 0x18899bc) + F(o, 0x19c9ae8);
    F(o, 0x19c9af4) = F(o, 0x18899c8) + F(o, 0x19c9af4);
    F(o, 0x19c9aec) = F(o, 0x18899c0) + F(o, 0x19c9aec);
    F(o, 0x19c9af8) = F(o, 0x18899cc) + F(o, 0x19c9af8);
    {
        const float dep = F(o, 0x18899ac);
        float a = lfoAt(o, p0) * dep; a = a + 1.0f;
        float b = dep * lfoAt(o, p3); b = b + 1.0f;
        reverbSetCombMod(P(o, kRev1), a, b);
    }
    const float x = param(o, 4) + F(o, 0x19c9b6c);
    float e;
    if (0.0f > x) e = -6.0f;
    else { float t = 6.0f * x; t = t + -6.0f; e = (1.0f < x) ? 0.0f : t; }
    const float lv0 = lenMul * powF(2.0f, e);
    float lv = (lv0 > 1.0f) ? 1.0f : ((0.015625f > lv0) ? 0.015625f : lv0);
    const float sz = clip01(param(o, 3) + F(o, 0x19c9b68));
    float fb = clip01(sizeMul * sz);
    if (param(o, 98) > 0.5f) fb = fb * 0.41999998688697815f;
    if (param(o, 103) > 0.5f) { fb = fb * 0.5f; lv = lv + lv; }
    if (param(o, 104) > 0.5f) { fb = fb * 0.5f; lv = lv * 16.0f; }
    reverbSetFeedback(P(o, kRev1), fb);
    reverbScaleLengths(P(o, kRev1), lv);
    {
        const float dep = F(o, 0x18899b0);
        float a = lfoAt(o, F(o, 0x19c9ae8)) * dep; a = a + 1.0f;
        float b = dep * lfoAt(o, F(o, 0x19c9af4)); b = b + 1.0f;
        reverbSetCombMod(P(o, kRev2), a, b);
    }
    float k = param(o, 9) * -0.375f;
    k = k + 1.0f;
    float lv2 = 0.8999999761581421f * lv;
    lv2 = lv2 * k;
    reverbSetFeedback(P(o, kRev2), fb * k);
    reverbScaleLengths(P(o, kRev2), lv2);
    {
        const float dep = F(o, 0x18899b4);
        float a = lfoAt(o, F(o, 0x19c9aec)) * dep; a = a + 1.0f;
        float b = dep * lfoAt(o, F(o, 0x19c9af8)); b = b + 1.0f;
        reverbSetCombMod(P(o, kRev3), a, b);
    }
    float fb3 = fb * 0.949999988079071f;
    float k3 = param(o, 9) * -0.25f;
    k3 = k3 + 1.0f;
    fb3 = fb3 * k3;
    float lv3 = lv * 0.949999988079071f;
    lv3 = lv3 * k3;
    reverbSetFeedback(P(o, kRev3), fb3);
    reverbScaleLengths(P(o, kRev3), lv3);
}

inline void modulatorLfos(uint8_t* o) {  // 0xa1f0: four triangle LFOs
    for (int k = 0; k < 4; k++) {
        float p = F(o, 0x1689964 + 4 * k) + F(o, 0x1689954 + 4 * k);
        p = p - float(int32_t(p));
        F(o, 0x1689964 + 4 * k) = p;
        const float sh = F(o, 0x1689944 + 4 * k);
        const float oms = 1.0f - sh;
        float t = p + p;
        t = t + -1.0f;
        t = std::fabs(t);
        float y = 1.0f - t;
        y = y * sh;
        y = y + oms;
        F(o, 0x1689974 + 4 * k) = y;
        y = y * F(o, 0x1689984 + 4 * k);
        F(o, 0x1889994 + 4 * k) = y + 1.0f;
    }
    if (0.5f > param(o, 99)) {
        reverbSetInputDelay(P(o, kRev1), F(o, 0x1889994), F(o, 0x1889998));
        reverbSetInputDelay(P(o, kRev2), F(o, 0x188999c), F(o, 0x18899a0));
        reverbSetInputDelay(P(o, kRev3), F(o, 0x188999c), F(o, 0x18899a0));
    } else {
        reverbSetInputDelay(P(o, kRev1), 1.0f, 1.0f);
        reverbSetInputDelay(P(o, kRev2), 1.0f, 1.0f);
        reverbSetInputDelay(P(o, kRev3), 1.0f, 1.0f);
    }
}

inline void followerDispatch(uint8_t* o) {  // 0xa460
    float a, b;
    if (U8(o, 0x19c9b78)) { a = F(o, 0x19c9b4c); b = F(o, 0x19c9b50); }
    else { a = F(o, 0x113ed4); b = F(o, 0x113ed8); }
    followerProcess(P(o, kFollowerL), a);
    followerProcess(P(o, kFollowerR), b);
    float m = std::fabs(b) + std::fabs(a);
    m = m * 0.5f;
    followerProcess(P(o, kFollowerMono), m);
    const int32_t mode = int32_t(param(o, 43) + 0.20000000298023224f);
    I(o, 0x16891ec) = mode;
    F(o, 0x18899a4) = 1.0f; F(o, 0x18899a8) = 1.0f;
    U8(o, 0x2768) = !(0.5f > param(o, 37));
    if (0.5f > param(o, 37)) return;
    if ((mode | 4) != 4) return;  // audio gain only in modes 0 and 4
    if (0.5f > param(o, 48)) {
        F(o, 0x18899a4) = F(o, kFollowerL + 0x3c);
        F(o, 0x18899a8) = F(o, kFollowerR + 0x3c);
    } else {
        F(o, 0x18899a4) = F(o, kFollowerMono + 0x3c);
        F(o, 0x18899a8) = F(o, kFollowerMono + 0x3c);
    }
}

inline void ensembleProcess(uint8_t* o, float inL, float inR, float* outL, float* outR) {  // 0xa5d0
    const float fbk = param(o, 36) * 0.9900000095367432f;
    float sumL = 0, sumR = 0;
    for (int v = 0; v < 6; v++) {
        const uint32_t ph = (uint32_t(I(o, 0x1889acc + 4 * v)) + uint32_t(I(o, 0x1889ab4 + 4 * v))) & 0xffffff;
        I(o, 0x1889ab4 + 4 * v) = int32_t(ph);
        const float cen = F(o, 0x1889a3c + 4 * v) + 1.0f;
        float dly = F(o, 0x1889a24 + 4 * v) * F(o, kEnsTable + 4 * size_t(ph >> 8));
        dly = dly + cen;
        const int32_t di = int32_t(dly);
        const uint32_t w = uint32_t(I(o, kWrite));
        const int32_t t = int32_t(w - uint32_t(di));
        const size_t bl = 0x1889ae4 + size_t(v) * 0x10000, br = 0x18e9ae4 + size_t(v) * 0x10000;
        float x0l = 0, x0r = 0, x1l = 0, x1r = 0;
        if (t >= 0) {
            x0l = F(o, bl + 4 * size_t(uint32_t(t) & 0x3fff));
            x0r = F(o, br + 4 * size_t(uint32_t(t) & 0x3fff));
            if (t > 0) {
                const uint32_t t1 = (w + 0x3fff - uint32_t(di)) & 0x3fff;
                x1l = F(o, bl + 4 * size_t(t1));
                x1r = F(o, br + 4 * size_t(t1));
            }
        }
        const float fr = dly - float(di);
        float yl = x1l - x0l; yl = yl * fr; yl = yl + x0l;
        float yr = x1r - x0r; yr = yr * fr; yr = yr + x0r;
        F(o, bl + 4 * size_t(w & 0x3fff)) = inL - fbk * yl;
        F(o, br + 4 * size_t(uint32_t(I(o, kWrite)) & 0x3fff)) = inR - fbk * yr;
        float gl = yl * F(o, 0x1889a54 + 4 * v); gl = gl + 1.0000000036274937e-15f;
        sumL = sumL + gl;
        float gr = yr * F(o, 0x1889a6c + 4 * v); gr = gr + 1.0000000036274937e-15f;
        sumR = sumR + gr;
    }
    *outL = sumL;
    *outR = sumR;
}

inline float tapOr0(uint8_t* o, size_t buf, int32_t w, int32_t len) {
    const int32_t t = int32_t(uint32_t(w) - uint32_t(len));
    return t >= 0 ? F(o, buf + 4 * size_t(uint16_t(t))) : 0.0f;
}
inline void earlyLate(uint8_t* o, float inL, float inR) {  // 0xa780: four cross-fed damped delays
    const int32_t w = I(o, kWrite);
    const float x5 = tapOr0(o, 0x27a8, w, I(o, 0x2760));
    const float x6 = tapOr0(o, 0x46d68, w, I(o, 0x2764));
    const float A = F(o, 0x2774), B = F(o, 0x277c);
    const float s1 = F(o, 0x2790) * B + x5 * A; F(o, 0x2790) = s1;
    const float s2 = F(o, 0x278c) * B + x6 * A; F(o, 0x278c) = s2;
    const float x3 = tapOr0(o, 0x8b328, w, I(o, 0x113ea8));
    const float x9 = tapOr0(o, 0xcf8e8, w, I(o, 0x113eac));
    const float A2 = F(o, 0x113eb8), B2 = F(o, 0x2780);
    const float s3 = F(o, 0x113ec8) * B2 + x3 * A2; F(o, 0x113ec8) = s3;
    const float s4 = B2 * F(o, 0x113ec4) + x9 * A2; F(o, 0x113ec4) = s4;
    const float e1 = F(o, 0x2788) * B + A * inL; F(o, 0x2788) = e1;
    const float e2 = B * F(o, 0x2784) + A * inR; F(o, 0x2784) = e2;
    constexpr float tiny = 1.0000000036274937e-15f;
    float y = s1 * F(o, 0x2770) + e1; y = y - s2 * F(o, 0x2778); y = s4 * F(o, 0x2798) + y; y = y + tiny;
    F(o, 0x27a8 + 4 * size_t(uint16_t(w))) = y;
    y = F(o, 0x278c) * F(o, 0x2770) + F(o, 0x2784); y = y - F(o, 0x2790) * F(o, 0x2778); y = F(o, 0x113ec8) * F(o, 0x2798) + y; y = y + tiny;
    F(o, 0x46d68 + 4 * size_t(uint16_t(I(o, kWrite)))) = y;
    const float A3 = F(o, 0x113eb8), B3 = F(o, 0x2780);
    const float s5 = F(o, 0x113ec0) * B3 + inL * A3; F(o, 0x113ec0) = s5;
    const float s6 = B3 * F(o, 0x113ebc) + A3 * inR; F(o, 0x113ebc) = s6;
    y = F(o, 0x113ec8) * F(o, 0x113eb0) + s5; y = y - F(o, 0x113ec4) * F(o, 0x113eb4); y = F(o, 0x278c) * F(o, 0x279c) + y; y = y + tiny;
    F(o, 0x8b328 + 4 * size_t(uint16_t(I(o, kWrite)))) = y;
    y = F(o, 0x113ec4) * F(o, 0x113eb0) + F(o, 0x113ebc); y = y - F(o, 0x113ec8) * F(o, 0x113eb4); y = F(o, 0x2790) * F(o, 0x279c) + y; y = y + tiny;
    F(o, 0xcf8e8 + 4 * size_t(uint16_t(I(o, kWrite)))) = y;
    const float c3 = F(o, 0x113ec8), c4 = F(o, 0x113ec4);
    float avg = c3 + c4; avg = avg * 0.5f;
    const float k = F(o, 0x2794);
    float om = 1.0f - k; om = om * avg;
    F(o, 0x113ec8) = c3 * k + om;
    F(o, 0x113ec4) = c4 * k + om;
}

constexpr size_t kPreL = 0x11f5020, kPreR = 0x143ef20;  // pre-delay lines, indexed & 0x7ffff
constexpr size_t kEarlyBufs[4] = {0x27a8, 0x46d68, 0x8b328, 0xcf8e8};

inline void engineClear(uint8_t* o) {  // 0x4df0
    I(o, 0x1689934) = -2; I(o, 0x1689930) = 0;
    for (size_t off = 0x19c9b0c; off <= 0x19c9b20; off += 4) I(o, off) = 0;
    I(o, kWrite) = 0;
    for (int i = 0; i < 100; i++) { F(o, kPreL + 4 * i) = 0; F(o, kPreR + 4 * i) = 0; }
    for (int i = 0; i < 100; i++) for (size_t b : kEarlyBufs) F(o, b + 4 * i) = 0;
    for (int i = 0; i < 100; i++) for (int v = 0; v < 12; v++) F(o, 0x1889ae4 + size_t(v) * 0x10000 + 4 * i) = 0;
    for (int k = 0; k < 4; k++) F(o, 0x1689964 + 4 * k) = 0;
    followerClear(P(o, kFollowerL));
    followerClear(P(o, kFollowerR));
    followerClear(P(o, kFollowerMono));
    reverbClear(P(o, kRev1)); reverbClear(P(o, kRev2)); reverbClear(P(o, kRev3));
    const float mix = clip01(param(o, 8) + F(o, 0x19c9b54));
    F(o, 0x18899ec) = mix; F(o, 0x18899f0) = mix;
    const float v = param(o, 93);
    F(o, 0x1689940) = v; F(o, 0x168993c) = v;
    const float ev = clip01(param(o, 30) + F(o, 0x19c9b58));
    F(o, 0x18899f4) = ev; F(o, 0x18899f8) = ev;
    float rv = 0;
    if (!(0.001 > double(v))) {
        float e = v * 70.0f; e = e + -60.0f; e = e / 20.0f;
        rv = powF(10.0f, e) * 0.3333300054073334f;
    }
    F(o, 0x1689938) = rv;
    const float eg = dbGain(ev);
    F(o, 0x18899fc) = eg;
    float n = 1.0f;
    if (U8(o, 0x18899ea)) { const float d = eg + 1.0f; n = n / d; }
    F(o, 0x1889a00) = n;
    eqControl(o);
    envelopeControl(o);
    modulatorControl(o);
    delayControl(o);
    reverbControl(o);
    envelopeRouting(o);
}

// comb tunings at 44.1 kHz: L uses Freeverb's eight plus 1680, 1046; R its own ten (0x5d60, copied)
constexpr float kCombL[10] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617, 1680, 1046};
constexpr float kCombR[10] = {1141, 1213, 1302, 1383, 1447, 1526, 1583, 1644, 1703, 1081};

inline void setSampleRate(uint8_t* o, float sr) {  // 0x5d60
    if (F(o, 0x2754) == sr) return;
    I(o, 0x19c9b34) = -32;
    I(o, 0x19c9b48) = int32_t(double(F(o, 0x2750) * 64.0f) / 44100.0);
    F(o, 0x2750) = sr; F(o, 0x2754) = sr;
    U8(o, 0x19c9b08) = 0;
    float x = sr;
    if (500.0f > sr) { F(o, 0x2750) = 500.0f; x = 500.0f; }
    else if (sr > 192000.0f) { F(o, 0x2750) = 192000.0f; x = 192000.0f; }
    static const float bands[10] = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    float xs[10];
    for (int k = 0; k < 10; k++) xs[k] = bands[k] / x;
    eqDesign(P(o, kEq), xs);
    const float fs = F(o, 0x2750);
    const float rs = 10000.0f / fs;
    followerReset(P(o, kFollowerL), fs, rs);
    followerReset(P(o, kFollowerR), F(o, 0x2750), rs);
    followerReset(P(o, kFollowerMono), F(o, 0x2750), rs);
    const float f2 = F(o, 0x2750);
    const float ms = f2 / 1000.0f;
    F(o, 0x275c) = ms; F(o, 0x2758) = ms;
    const float base = U8(o, 0x19c9b08) ? 0.5f * f2 : 44100.0f;
    const float q = base / f2;
    float len[20];
    for (int k = 0; k < 10; k++) { len[k] = kCombL[k] * q; len[10 + k] = kCombR[k] * q; }
    reverbSetLengths(P(o, kRev1), base, len);
    reverbSetLengths(P(o, kRev2), base, len);
    reverbSetLengths(P(o, kRev3), base, len);
    engineClear(o);
}

// 0x63d0 defaults for the 128 parameter slots, as float bits (slot, value)
constexpr uint32_t kSlotDefaults[][2] = {
    {0, 0x3f800000}, {2, 0x3f08f5c3}, {3, 0x3f2e147b}, {4, 0x3f800000}, {5, 0x3e75c28f}, {6, 0x3f770a3d},
    {7, 0x3e2e147b}, {11, 0x3f800000}, {14, 0x3f800000}, {29, 0x3f800000}, {30, 0x3f800000}, {31, 0x3f257ebb},
    {32, 0x3ea57eaa}, {33, 0x3f3a2e90}, {34, 0x3f5c28f6}, {35, 0x3efae148}, {36, 0x3d90ced5}, {40, 0x3f000000},
    {41, 0x3f000000}, {42, 0x3f000000}, {43, 0x3dcd35a8}, {45, 0x3ee147ae}, {46, 0x3f0f5c29}, {47, 0x3f800000},
    {48, 0x3f800000}, {49, 0x3f800000}, {54, 0x3f800000}, {55, 0x3f800000}, {56, 0x3f800000}, {57, 0x3f800000},
    {61, 0x3ca57eaa}, {62, 0x3ca3d70a}, {64, 0x3c23d70a}, {65, 0x3c23d70a}, {67, 0x3ca3d70a}, {68, 0x3cf5c28f},
    {72, 0x3f6e147b}, {73, 0x3f800000}, {74, 0x3e9eb852}, {75, 0x3e3d70a4}, {85, 0x3f51eb85}, {86, 0x3f451eb8},
    {87, 0x3eae147b}, {90, 0x3efd70a4}, {92, 0x3f07ae14}, {93, 0x3f5b6da8}, {94, 0x3b23d90e}, {95, 0x3b23d90e},
    {97, 0x3f800000}, {100, 0x3f800000}, {105, 0x3f800000}};

inline void engineInit(uint8_t* o) {  // 0x63d0
    I(o, 0x2750) = 0x472ca700;  // 44199: differs from 44100 so the first setSampleRate runs
    U8(o, 0x19c9b08) = 0;
    F(o, 0x19c9afc) = 0; F(o, 0x19c9b00) = 0;
    I(o, 0x2758) = 0x4230cbc7; I(o, 0x275c) = 0x4230cbc7;
    for (size_t off = 0x19c9b0c; off <= 0x19c9b20; off += 4) I(o, off) = 0;
    reverbInit(P(o, kRev1)); reverbInit(P(o, kRev2)); reverbInit(P(o, kRev3));
    I(o, 0x274c) = 0; I(o, kWrite) = 0;
    F(o, 0x113ed4) = 0; F(o, 0x113ed8) = 0; F(o, 0x113ecc) = 0; F(o, 0x113ed0) = 0;
    F(o, 0x18899ec) = 1.0f; F(o, 0x18899f0) = 1.0f;
    U8(o, 0x18899e8) = 0; U8(o, 0x18899e9) = 0; U8(o, 0x18899ea) = 0; U8(o, 0x2768) = 0;
    I(o, 0x19c9b04) = 0; I(o, 0x18899e4) = 0; I(o, 0x18899dc) = 0; I(o, 0x18899e0) = 0;
    U8(o, 0x18899d8) = 0; U8(o, 0x18899d9) = 0; U8(o, 0x18899da) = 0;
    I(o, 0x1689934) = -2; I(o, 0x1689930) = 0;
    memset(o + kPreL, 0, 600000 * 4);
    memset(o + kPreR, 0, 600000 * 4);
    for (size_t b : kEarlyBufs) memset(o + b, 0, 70000 * 4);
    for (int v = 0; v < 12; v++) memset(o + 0x1889ae4 + size_t(v) * 0x10000, 0, 0x10000);
    F(o, 0x1688e30) = 1.0f; F(o, 0x1688e2c) = 0;
    F(o, 0x1689938) = 1.0f; F(o, 0x168993c) = 1.0f; F(o, 0x1689940) = 1.0f;
    I(o, 0x2760) = 1; I(o, 0x2764) = 1;
    F(o, 0x276c) = 0; F(o, 0x2770) = 0; F(o, 0x2798) = 0; F(o, 0x2774) = 1.0f; F(o, 0x277c) = 0; F(o, 0x2778) = 0;
    I(o, 0x113eac) = 1; I(o, 0x113ea8) = 1;
    F(o, 0x113eb0) = 0; F(o, 0x113eb4) = 0; F(o, 0x279c) = 0; F(o, 0x113eb8) = 1.0f; F(o, 0x2780) = 0;
    I(o, 0x2794) = 0x3f400000;
    F(o, 0x2788) = 0; F(o, 0x2784) = 0; F(o, 0x2790) = 0; F(o, 0x278c) = 0;
    F(o, 0x113ec0) = 0; F(o, 0x113ebc) = 0; F(o, 0x113ec8) = 0; F(o, 0x113ec4) = 0;
    F(o, 0x1689924) = 0; F(o, 0x1689920) = 0; F(o, 0x168991c) = 1.0f; F(o, 0x1689918) = 0; I(o, 0x1689914) = 1;
    I(o, kWrite) = 0;
    F(o, 0x18899d0) = 0; F(o, 0x18899d4) = 1.0f;
    for (size_t off = 0x19c9b54; off <= 0x19c9b74; off += 4) F(o, off) = 0;
    U8(o, 0x19c9b78) = 0; F(o, 0x19c9b4c) = 0; F(o, 0x19c9b50) = 0; U8(o, 0x19c9b79) = 0;
    I(o, 0x16891ec) = 0; F(o, 0x16891f0) = -1.0f;
    F(o, 0x18899a4) = 1.0f; F(o, 0x18899a8) = 1.0f;
    F(o, 0x18899f4) = 1.0f; F(o, 0x18899f8) = 1.0f; F(o, 0x18899fc) = 1.0f;
    F(o, 0x1889a04) = 0.5f; F(o, 0x1889a00) = 1.0f;
    F(o, 0x27a4) = 1.0f;
    float w = F(o, 0x2750) * 0.05000000074505806f;
    w = w * 0.10000000149011612f;
    F(o, 0x27a0) = w + 1.0f;
    I(o, 0x1889a08) = 0;
    for (int v = 0; v < 6; v++) {
        const float fv = float(v);
        I(o, 0x1889ab4 + 4 * v) = 0;
        float e = F(o, 0x27a4) * -2.0f; e = e * fv; e = e / 5.0f;
        F(o, 0x1889a0c + 4 * v) = powF(2.0f, e) * F(o, 0x27a0);
        I(o, 0x1889acc + 4 * v) = 0;
        e = F(o, 0x27a4) * -2.0f; e = e * fv; e = e / 5.300000190734863f;
        F(o, 0x1889a84 + 4 * v) = powF(2.0f, e) * 112640.0f;
        e = F(o, 0x27a4) * -2.0f; e = e * fv; e = e / 8.300000190734863f;
        F(o, 0x1889a9c + 4 * v) = powF(2.0f, e) * 0.9900000095367432f;
        F(o, 0x1889a24 + 4 * v) = 1.0f; F(o, 0x1889a3c + 4 * v) = 1.0f;
        F(o, 0x1889a54 + 4 * v) = 1.0f; F(o, 0x1889a6c + 4 * v) = 1.0f;
    }
    for (int k = 0; k < 4; k++) {
        F(o, 0x1689964 + 4 * k) = 0; F(o, 0x1689974 + 4 * k) = 0; F(o, 0x1689984 + 4 * k) = 1.0f;
        F(o, 0x1689944 + 4 * k) = 0; F(o, 0x1689954 + 4 * k) = 0; F(o, 0x1889994 + 4 * k) = 0;
        memset(o + 0x1689994 + size_t(k) * 0x80000, 0, 0x80000);
    }
    for (size_t off = 0x19c9ae4; off <= 0x19c9af8; off += 4) F(o, off) = 0;
    for (size_t off : {0x18899ac, 0x18899b8, 0x18899c4, 0x18899b0, 0x18899bc, 0x18899c8, 0x18899b4, 0x18899c0, 0x18899cc}) F(o, off) = 0;
    F(o, 0x168992c) = 1.0f; F(o, 0x1689928) = 1.0f;
    I(o, 0x1688e28) = 0; I(o, 0x1688e20) = 0; I(o, 0x1688e24) = 0;
    for (int i = 0; i < 65536; i++) {  // sine tables; pi is 3.14159 in the original
        float x = float(i) * 1.52587890625e-05f;
        x = x + x;
        x = x * 3.141590118408203f;
        const float s = float(sinLib(double(x)));
        F(o, kLfoTable + 4 * size_t(i)) = s;
        float h = s * 0.5f;
        F(o, kEnsTable + 4 * size_t(i)) = h + 0.5f;
    }
    D(o, 0x19c9b38) = 0; D(o, 0x19c9b40) = 0;
    I(o, 0x19c9b34) = 0;
    memset(o + kParams, 0, 128 * 4);
    for (auto& d : kSlotDefaults) I(o, kParams + 4 * d[0]) = int32_t(d[1]);
    memcpy(o + 0x234c, o + kParams, 128 * 4);
    setSampleRate(o, 44100.0f);
}

// 0x5200: one sample
inline float softClip(float x, float a) {  // 0x5200 distortion: (x + ax) / (|ax| + 1), then clamp
    float t = a * x;
    t = t + 1.0000000036274937e-15f;
    const float u = x + t;
    float d = std::fabs(t);
    d = d + 1.0f;
    const float y = u / d;
    if (-1.0f > y) return -1.0f;
    const float y2 = y + 1.0000000036274937e-15f;
    return (1.0f < y) ? 1.0f : y2;
}

inline void reverbTriple(uint8_t* o, float a1L, float a1R, float a2L, float a2R, float a3L, float a3R, float r[6]) {
    // r = r1L, r1R, r2L, r2R, r3L, r3R
    for (int i = 0; i < 6; i++) r[i] = 0;
    if (U8(o, 0x18899d8)) reverbProcess(P(o, kRev1), a1L, a1R, &r[0], &r[1]);
    if (U8(o, 0x18899d9)) reverbProcess(P(o, kRev2), a2L, a2R, &r[2], &r[3]);
    if (U8(o, 0x18899da)) reverbProcess(P(o, kRev3), a3L, a3R, &r[4], &r[5]);
}

inline void processSample(uint8_t* o, double inL, double inR, double* outL, double* outR) {
    F(o, 0x113ed4) = float(inL);
    F(o, 0x113ed8) = float(inR);
    int32_t cnt = int32_t(uint32_t(I(o, 0x274c)) + 1);
    I(o, 0x274c) = cnt;
    if ((cnt & 0xfff) == 0) applyPresets(o);
    const float raw = param(o, 8) + F(o, 0x19c9b54);
    const float mt = clip01(raw);
    F(o, 0x18899f0) = mt;
    float s = mt * 0.009999999776482582f;
    s = s + F(o, 0x18899ec) * 0.9900000095367432f;
    F(o, 0x18899ec) = s;
    const float s2 = s + s;
    F(o, 0x18899d0) = clip01(2.0f - s2);
    F(o, 0x18899d4) = clip01(s2);
    if (!(0.0f > raw) && (raw > 1.0f || raw > 0.9900000095367432f)) { F(o, 0x18899d0) = 0; F(o, 0x18899d4) = 1.0f; }
    const float rvT = param(o, 93);
    F(o, 0x1689940) = rvT;
    float sm = rvT * 0.009999999776482582f;
    sm = sm + F(o, 0x168993c) * 0.9900000095367432f;
    F(o, 0x168993c) = sm;
    float rv = 0;
    if (!(0.001 > double(sm))) {
        float e = sm * 70.0f; e = e + -60.0f; e = e / 20.0f;
        rv = powF(10.0f, e) * 0.3333300054073334f;
    }
    F(o, 0x1689938) = rv;
    const float ev = clip01(param(o, 30) + F(o, 0x19c9b58));
    F(o, 0x18899f4) = ev;
    float es = ev * 0.009999999776482582f;
    es = es + F(o, 0x18899f8) * 0.9900000095367432f;
    F(o, 0x18899f8) = es;
    const float eg = dbGain(es);
    F(o, 0x18899fc) = eg;
    float n = 1.0f;
    if (U8(o, 0x18899ea)) { const float d = eg + 1.0f; n = n / d; }
    F(o, 0x1889a00) = n;
    cnt = I(o, 0x274c);
    if ((cnt & 0x3f) == 0) {
        eqControl(o); envelopeControl(o); modulatorControl(o); ensembleSetup(o); delayControl(o); reverbControl(o);
        cnt = I(o, 0x274c);
    }
    if (cnt % 7 == 0) { modulatorLfos(o); cnt = I(o, 0x274c); }
    if ((cnt & 7) == 0) envelopeRouting(o);
    followerDispatch(o);
    float xL = F(o, 0x113ed4), xR = F(o, 0x113ed8);
    float ensL = 0, ensR = 0, dirL = 0, dirR = 0;
    if (U8(o, 0x18899ea)) {
        ensembleProcess(o, xL, xR, &ensL, &ensR);
        dirL = ensL; dirR = ensR;
        const float g = F(o, 0x18899fc), nn = F(o, 0x1889a00);
        xL = g * ensL; xL = xL + F(o, 0x113ed4); xL = xL * nn;
        xR = g * ensR; xR = xR + F(o, 0x113ed8); xR = xR * nn;
    }
    earlyLate(o, xL, xR);
    const float a = F(o, 0x1688e2c), mk = F(o, 0x1688e30);
    const float dL = (0.009999999776482582f > a) ? xL : softClip(xL, a);
    const float dR = (0.009999999776482582f > a) ? xR : softClip(xR, a);
    float pL = mk * dL, pR = mk * dR;
    const bool eqOn = !(0.5f > param(o, 39));
    I(o, 0x19c9b04) = eqOn;
    if (eqOn && 0.5f > param(o, 38)) eqProcess(P(o, kEq), 10, pL, pR, &pL, &pR);
    const uint32_t w = uint32_t(I(o, kWrite));
    F(o, kPreL + 4 * size_t(w & 0x7ffff)) = pL;
    F(o, kPreR + 4 * size_t(uint32_t(I(o, kWrite)) & 0x7ffff)) = pR;
    auto tap = [&](int32_t len, float& l, float& r) {
        const int32_t t = int32_t(w - uint32_t(len));
        l = 0; r = 0;
        if (t >= 0) { l = F(o, kPreL + 4 * size_t(uint32_t(t) & 0x7ffff)); r = F(o, kPreR + 4 * size_t(uint32_t(t) & 0x7ffff)); }
    };
    float a1L, a1R, a2L, a2R, a3L, a3R, lL, lR;
    tap(I(o, 0x1688e20), a1L, a1R);
    tap(I(o, 0x1688e24), a2L, a2R);
    tap(I(o, 0x1688e28), a3L, a3R);
    tap(I(o, 0x1689914), lL, lR);
    const float k = F(o, 0x168991c);
    lL = lL * k;
    const float om = 1.0f - k;
    const float s7 = F(o, 0x1689920) * om + lL; F(o, 0x1689920) = s7;
    lR = lR * k;
    const float s8 = om * F(o, 0x1689924) + lR; F(o, 0x1689924) = s8;
    if (I(o, 0x18899dc) == 1 && U8(o, 0x18899e8)) {  // early reflections into the reverb
        float eL = F(o, 0x2790) + F(o, 0x113ec8); eL = eL * F(o, 0x276c);
        float eR = F(o, 0x278c) + F(o, 0x113ec4); eR = eR * F(o, 0x276c);
        a1L = a1L + eL; a1R = a1R + eR; a2L = a2L + eL; a2R = a2R + eR; a3L = a3L + eL; a3R = a3R + eR;
    }
    if (I(o, 0x18899e0) == 1 && U8(o, 0x18899e9)) {  // late reflection into the reverb
        const float lg = F(o, 0x1689918);
        const float l = s7 * lg, r = s8 * lg;
        a1L = a1L + l; a1R = a1R + r; a2L = a2L + l; a2R = a2R + r; a3L = a3L + l; a3R = a3R + r;
    }
    float r[6];
    if (U8(o, 0x19c9b08) == 0) {
        reverbTriple(o, a1L, a1R, a2L, a2R, a3L, a3R, r);
    } else if (I(o, 0x274c) & 1) {  // half-rate path: flag is never set by the original
        reverbTriple(o, a1L, a1R, a2L, a2R, a3L, a3R, r);
        F(o, 0x19c9b0c) = r[0]; F(o, 0x19c9b10) = r[2]; F(o, 0x19c9b14) = r[4];
        F(o, 0x19c9b18) = r[1]; F(o, 0x19c9b1c) = r[3]; F(o, 0x19c9b20) = r[5];
    } else {
        r[0] = F(o, 0x19c9b0c); r[2] = F(o, 0x19c9b10); r[4] = F(o, 0x19c9b14);
        r[1] = F(o, 0x19c9b18); r[3] = F(o, 0x19c9b1c); r[5] = F(o, 0x19c9b20);
    }
    const float rvg = F(o, 0x1689938);
    float wL = r[2] + r[0]; wL = wL + r[4]; wL = wL * rvg;
    float wR = r[3] + r[1]; wR = wR + r[5]; wR = wR * rvg;
    if (!U8(o, 0x18899d9)) { wL = wL * 1.5f; wR = wR * 1.5f; }
    float eL = 0, eR = 0;
    if (I(o, 0x18899dc) == 0 && U8(o, 0x18899e8)) {  // early reflections direct
        eL = F(o, 0x2790) + F(o, 0x113ec8); eL = eL * F(o, 0x276c); eL = eL + 0.0f;
        eR = F(o, 0x278c) + F(o, 0x113ec4); eR = eR * F(o, 0x276c); eR = eR + 0.0f;
    }
    if (I(o, 0x18899e0) == 0 && U8(o, 0x18899e9)) {  // late reflection direct
        const float lg = F(o, 0x1689918);
        eL = eL + F(o, 0x1689920) * lg;
        eR = eR + lg * F(o, 0x1689924);
    }
    wL = wL + eL;
    wR = wR + eR;
    if (I(o, 0x19c9b04) != 0 && param(o, 38) > 0.5f) eqProcess(P(o, kEq), 10, wL, wR, &wL, &wR);
    const float dry = F(o, 0x18899d0), wet = F(o, 0x18899d4);
    float oL = wL * wet; oL = oL * F(o, 0x18899a4); oL = oL + F(o, 0x113ed4) * dry;
    F(o, 0x113ecc) = oL;
    float oR = wR * wet; oR = oR * F(o, 0x18899a8); oR = oR + dry * F(o, 0x113ed8);
    F(o, 0x113ed0) = oR;
    if (param(o, 29) > 0.5f) {  // ensemble "direct + reverb"
        const float g = F(o, 0x18899fc), nn = F(o, 0x1889a00);
        oL = oL + dirL * g; oL = oL * nn; F(o, 0x113ecc) = oL;
        oR = oR + dirR * g; oR = oR * nn; F(o, 0x113ed0) = oR;
    }
    oL = oL * 1.149999976158142f;
    oL = (-1.0f > oL) ? -1.0f : oL;
    oL = (1.0f < oL) ? 1.0f : oL;
    F(o, 0x113ecc) = oL;
    oR = oR * 1.149999976158142f;
    oR = (-1.0f > oR) ? -1.0f : oR;
    oR = (1.0f < oR) ? 1.0f : oR;
    F(o, 0x113ed0) = oR;
    *outL = double(oL);
    *outR = double(oR);
    I(o, kWrite) = int32_t(uint32_t(I(o, kWrite)) + 1);
}

}  // namespace rpverb
