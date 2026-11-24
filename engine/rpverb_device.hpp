// RP-Verb device: the original's constructor and per-batch render (0x12c0, 0x25d0) without the
// Reason host. A batch is 64 frames. The host supplies inputs, connections, transport, sample rate,
// CV and the properties that changed; the device fills the outputs (or leaves them untouched,
// as the original does when off, bypassed on silence, or asleep).
#pragma once
#include "rpverb_core.hpp"
#include <cstdlib>

namespace rpverb {

constexpr int kBatch = 64;

struct BatchIO {
    float in[4][kBatch]{};           // in L, in R, side L, side R (what the host buffers hold)
    bool conn[4]{true, true, false, false};
    bool playing = false;
    double sampleRate = 44100;
    double cv[10]{};                 // WetDry, Ensemble, Distort, Early, Late, Space, Length, Diffusion, Damping, Gate
    bool cvConn[10]{};
    int powerMode = 1;               // builtin_onoffbypass: 0 off, 1 on, 2 bypass
    bool resetButton = false;
    double space = 0, colour = 0, envelopeMode = 0;
    int changes = 0;                 // number of property diffs the host delivered this batch
    float out[2][kBatch]{};
    bool outWritten = false;
};

// property kinds the constructor registers (0 number, 1 bool); other tags are ignored in diffs
inline int propertyKind(int tag) {
    switch (tag) {
    case 105: case 100: case 28: case 50: case 70: case 54: case 83: case 37: case 38: case 48: case 47: case 39: return 1;
    case 8: case 30: case 29: case 53: case 73: case 72: case 75: case 74: case 90: case 92: case 71: case 2: case 3:
    case 4: case 5: case 9: case 93: case 13: case 49: case 6: case 7: case 86: case 85: case 87: case 84: case 44:
    case 45: case 46: case 97: case 40: case 41: case 42: return 0;
    default: return -1;
    }
}

class Device {
public:
    Device() {
        o_ = static_cast<uint8_t*>(std::aligned_alloc(64, kObjectSize));
        if (!o_) return;
        memset(o_, 0, kObjectSize);
        construct();
    }
    ~Device() { std::free(o_); }
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    bool ok() const { return o_ != nullptr; }
    uint8_t* state() { return o_; }

    // property value as the host stores it; numbers arrive as double and are stored as float
    void setProperty(int tag, double v) {
        const int k = (tag >= 1 && tag <= 127) ? propertyKind(tag) : -1;
        if (k == 0) setSlot(o_, tag, float(v));
        else if (k == 1) setSlot(o_, tag, v != 0 ? 1.0f : 0.0f);
    }

    void render(BatchIO& io, const int* changedTags, const double* changedValues, int nChanged);

private:
    void construct();
    uint8_t* o_ = nullptr;
};

inline void Device::construct() {  // 0x12c0 without the host bindings
    uint8_t* o = o_;
    for (size_t R : {kRev1, kRev2, kRev3}) {
        for (size_t bank : {kBankA, kBankB}) {
            uint8_t* c = o + R + bank;
            I(c, 0x1d8) = 0;
            combBankInit(c);
            st(c, 0x170, splat(1.0f)); st(c, 0x180, splat(0)); st(c, 0x190, splat(0)); st(c, 0x1a0, splat(1.0f));
            memset(c + 0x230, 0, 0x280000);
        }
        memset(o + R + 0x24, 0, 0x40000);
        memset(o + R + 0x40024, 0, 0x40000);
    }
    const float x[10] = {0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f};
    eqDesign(P(o, kEq), x);
    for (size_t f : {kFollowerMono, kFollowerL, kFollowerR}) followerReset(P(o, f), 44100.0f, 0.022675737738609314f);
    for (int t = 0; t < 128; t++) I(o, 0x1b48 + 4 * t) = propertyKind(t);
    engineInit(o);
    I(o, 0x1f48) = 2000;
    I(o, 0x19c9b34) = 20000;
    D(o, 0x19c9b38) = 0; D(o, 0x19c9b40) = 0;
    memset(o + 0x1b38, 0, 11);
}

inline bool anyAbove(const float* x, float thr) {
    for (int i = 0; i < kBatch; i++) {
        float v = x[i];
        if (thr > v) v = -v;
        if (v > thr) return true;
    }
    return false;
}

inline void Device::render(BatchIO& io, const int* changedTags, const double* changedValues, int nChanged) {
    uint8_t* o = o_;
    io.outWritten = false;
    memcpy(o + 0x000, io.in[0], 256);
    memcpy(o + 0x100, io.in[1], 256);
    memcpy(o + 0x400, io.in[2], 256);
    memcpy(o + 0x500, io.in[3], 256);
    U8(o, 0x1b38) = U8(o, 0x1b39);
    U8(o, 0x1b39) = io.playing;
    int stopped = 0;  // transport just stopped with "Reset on Stop" off: no diff-count reset this batch
    if (U8(o, 0x1b38) == 1 && !io.playing && 0.9 > double(param(o, 105))) stopped = 1;
    setSidechainConnected(o, io.conn[2] || io.conn[3]);
    if (io.conn[2] && !io.conn[3]) memcpy(o + 0x500, o + 0x400, 256);
    if (!io.conn[2] && io.conn[3]) memcpy(o + 0x400, o + 0x500, 256);
    if (io.conn[0] && !io.conn[1]) memcpy(o + 0x100, o + 0x000, 256);
    if (!io.conn[0] && io.conn[1]) memcpy(o + 0x000, o + 0x100, 256);
    const int mode = io.powerMode;
    if (io.resetButton) I(o, 0x19c9b34) = -32;
    const double gate = io.cv[9];
    envelopeGate(o, io.cvConn[9] && double(float(gate)) > 0.007874015748031496 ? 1 : 0);
    if (!stopped && unsigned(io.changes) >= 11) I(o, 0x19c9b34) = -32;
    for (int c = 0; c < 9; c++) U8(o, 0x1b3a + c) = io.cvConn[c];
    setSampleRate(o, float(io.sampleRate));
    const int32_t c0 = I(o, 0x19c9b34);
    I(o, 0x19c9b34) = c0 + 1;
    if (c0 >= I(o, 0x19c9b48) && c0 + 1 <= 9998) { engineClear(o); I(o, 0x19c9b34) = 20000; }
    setSlot(o, 94, float(int32_t(io.space)));
    setSlot(o, 95, float(int32_t(io.colour)));
    setSlot(o, 43, float(int32_t(io.envelopeMode)));
    for (int c = 0; c < 9; c++) F(o, 0x19c9b54 + 4 * c) = U8(o, 0x1b3a + c) ? float(io.cv[c]) : 0.0f;
    for (int k = 0; k < nChanged; k++) setProperty(changedTags[k], changedValues[k]);

    bool silentIn = !anyAbove(reinterpret_cast<float*>(o + 0x000), 1.999999987845058e-08f) &&
                    !anyAbove(reinterpret_cast<float*>(o + 0x100), 1.999999987845058e-08f);
    if (!silentIn) {
        I(o, 0x1f48) = 0;
        if (mode == 0) {
            memset(o + 0x200, 0, 512);
            memcpy(io.out[0], o + 0x200, 256); memcpy(io.out[1], o + 0x300, 256); io.outWritten = true;
        } else if (mode == 2) {
            memcpy(o + 0x200, o + 0x000, 256); memcpy(o + 0x300, o + 0x100, 256);
            memcpy(io.out[0], o + 0x200, 256); memcpy(io.out[1], o + 0x300, 256); io.outWritten = true;
        }
    }
    if (mode == 1 && !(silentIn && I(o, 0x1f48) > 1999)) {
        for (int i = 0; i < kBatch; i++) {
            double l, r;
            if (I(o, 0x19c9b34) > 9998) {
                processSample(o, double(F(o, 4 * i)), double(F(o, 0x100 + 4 * i)), &l, &r);
                setSidechain(o, F(o, 0x400 + 4 * i), F(o, 0x500 + 4 * i));
                D(o, 0x19c9b38) = l; D(o, 0x19c9b40) = r;
            } else {
                l = D(o, 0x19c9b38) * 0.99; D(o, 0x19c9b38) = l;
                r = D(o, 0x19c9b40) * 0.99; D(o, 0x19c9b40) = r;
            }
            F(o, 0x200 + 4 * i) = float(l);
            F(o, 0x300 + 4 * i) = float(r);
        }
        if (anyAbove(reinterpret_cast<float*>(o + 0x200), 9.999999974752427e-07f) ||
            anyAbove(reinterpret_cast<float*>(o + 0x300), 9.999999974752427e-07f))
            I(o, 0x1f48) = 0;
        else
            I(o, 0x1f48) = I(o, 0x1f48) + 1;
        memcpy(io.out[0], o + 0x200, 256); memcpy(io.out[1], o + 0x300, 256); io.outWritten = true;
    }
    I(o, 0x1b44) = I(o, 0x1b44) + 1;
}

}  // namespace rpverb
