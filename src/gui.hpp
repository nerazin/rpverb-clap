// RP-Verb GUI interface. The plugin implements UiHost; the macOS view (gui_mac.mm) only talks to it.
// All gui:: functions run on the host's main thread.
#pragma once
#include <cstdint>

namespace rpverb_clap {

class UiHost {
public:
    virtual ~UiHost() = default;
    virtual double uiValue(int index) const = 0;   // parameter value (kParams index)
    virtual double uiMod(int index) const = 0;     // host modulation offset (drives the knob's CV input)
    virtual void uiBegin(int index) = 0;           // a drag: begin, values, end (one host gesture)
    virtual void uiSet(int index, double v) = 0;
    virtual void uiEnd(int index) = 0;
    // device state after the last batch: mute-and-clear counter, quiet batches, power mode
    virtual int uiResetCounter() const = 0;
    virtual int uiQuietBatches() const = 0;
};

namespace gui {
void* create(UiHost& host);
void destroy(void* g);
bool size(void* g, uint32_t* w, uint32_t* h);
bool setParent(void* g, const void* nsView);
bool show(void* g, bool shown);
}  // namespace gui

}  // namespace rpverb_clap
