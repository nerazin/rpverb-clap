// RP-Verb Reconstruction: CLAP wrapper around the ported RPVerb device.
// The device runs in 64-frame batches like the Reason host, so the plugin reports 64 frames of
// latency. Knob changes and modulation are applied at the next batch boundary.
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

#include <clap/clap.h>

#include "gui.hpp"
#include "params.hpp"
#include "rpverb_device.hpp"

using namespace rpverb_clap;

namespace {

constexpr uint32_t kLatency = rpverb::kBatch;
constexpr char kStateMagic[8] = {'R', 'P', 'V', 'R', 'C', 'L', 'P', '1'};

const char* const kFeatures[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, CLAP_PLUGIN_FEATURE_REVERB,
                                 CLAP_PLUGIN_FEATURE_STEREO, nullptr};
const clap_plugin_descriptor_t kDescriptor = {
    CLAP_VERSION_INIT, "com.rpverb-reconstruction.rpverb", "RPVerb Reconstruction", "RPVerb Reconstruction Project",
    "", "", "", "0.1.0", "Reverb reconstructed from the RPVerb 1.0.7 Rack Extension", kFeatures};

// GUI -> host: gestures and values, one producer (main thread), one consumer (process or flush)
struct UiEvent { uint16_t type; int index; double value; };
class UiQueue {
public:
    bool push(const UiEvent& e) {
        const uint32_t w = write_.load(std::memory_order_relaxed);
        if (w - read_.load(std::memory_order_acquire) >= kSize) return false;
        items_[w % kSize] = e;
        write_.store(w + 1, std::memory_order_release);
        return true;
    }
    bool pop(UiEvent& e) {
        const uint32_t r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire)) return false;
        e = items_[r % kSize];
        read_.store(r + 1, std::memory_order_release);
        return true;
    }
private:
    static constexpr uint32_t kSize = 1024;
    UiEvent items_[kSize]{};
    std::atomic<uint32_t> write_{0}, read_{0};
};

struct Plugin final : UiHost {
    clap_plugin_t clap{};
    const clap_host_t* host = nullptr;
    const clap_host_params_t* hostParams = nullptr;
    std::unique_ptr<rpverb::Device> dev;

    std::atomic<double> value[kParamCount];  // current values (main and audio thread)
    std::atomic<bool> sendAll{true};         // next batch reports every property as changed (creation, state load)
    std::atomic<bool> resetRequest{false};   // CLAP reset(): one batch with the Reset button down
    std::atomic<double> mod[kParamCount];    // host modulation offsets (audio thread writes, GUI reads)
    std::atomic<int> resetCounter{20000}, quiet{2000};
    UiQueue uiEvents;
    void* gui = nullptr;

    // audio thread
    double applied[kParamCount]{};
    float carry[4][rpverb::kBatch]{};
    float prevOut[2][rpverb::kBatch]{};
    int fill = 0;
    double rate = 44100;
    bool playing = false;

    Plugin() {
        for (int i = 0; i < kParamCount; i++) {
            value[i].store(kParams[i].def, std::memory_order_relaxed);
            mod[i].store(0, std::memory_order_relaxed);
        }
    }

    // ---- UiHost (main thread)
    double uiValue(int i) const override { return value[i].load(std::memory_order_relaxed); }
    double uiMod(int i) const override { return mod[i].load(std::memory_order_relaxed); }
    void requestFlush() { if (hostParams && hostParams->request_flush) hostParams->request_flush(host); }
    void uiBegin(int i) override { uiEvents.push({CLAP_EVENT_PARAM_GESTURE_BEGIN, i, 0}); requestFlush(); }
    void uiSet(int i, double v) override {
        v = clampValue(i, v);
        value[i].store(v, std::memory_order_relaxed);  // the device takes it at the next batch
        uiEvents.push({CLAP_EVENT_PARAM_VALUE, i, v});
        requestFlush();
    }
    void uiEnd(int i) override { uiEvents.push({CLAP_EVENT_PARAM_GESTURE_END, i, 0}); requestFlush(); }
    int uiResetCounter() const override { return resetCounter.load(std::memory_order_relaxed); }
    int uiQuietBatches() const override { return quiet.load(std::memory_order_relaxed); }

    void emitUiEvents(const clap_output_events_t* out) {
        UiEvent e;
        while (uiEvents.pop(e)) {
            if (!out) continue;
            const clap_id id = kParams[e.index].id;
            if (e.type == CLAP_EVENT_PARAM_VALUE) {
                clap_event_param_value_t ev{};
                ev.header = {sizeof ev, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
                ev.param_id = id; ev.note_id = -1; ev.port_index = -1; ev.channel = -1; ev.key = -1; ev.value = e.value;
                out->try_push(out, &ev.header);
            } else {
                clap_event_param_gesture_t ev{};
                ev.header = {sizeof ev, 0, CLAP_CORE_EVENT_SPACE_ID, e.type, 0};
                ev.param_id = id;
                out->try_push(out, &ev.header);
            }
        }
    }

    void handleEvent(const clap_event_header_t* h) {
        if (h->space_id != CLAP_CORE_EVENT_SPACE_ID) return;
        if (h->type == CLAP_EVENT_PARAM_VALUE) {
            const auto* e = reinterpret_cast<const clap_event_param_value_t*>(h);
            const int i = paramIndex(e->param_id);
            if (i >= 0) value[i].store(clampValue(i, e->value), std::memory_order_relaxed);
        } else if (h->type == CLAP_EVENT_PARAM_MOD) {
            const auto* e = reinterpret_cast<const clap_event_param_mod_t*>(h);
            const int i = paramIndex(e->param_id);
            if (i >= 0 && kParams[i].cv >= 0 && std::isfinite(e->amount)) mod[i].store(e->amount, std::memory_order_relaxed);
        }
    }
    static double clampValue(int i, double v) {
        if (!std::isfinite(v)) return kParams[i].def;
        const double hi = kParams[i].steps ? kParams[i].steps - 1 : 1.0;
        v = v < 0 ? 0 : (v > hi ? hi : v);
        return kParams[i].steps ? std::floor(v + 0.5) : v;
    }
    double get(uint32_t id) const { return value[paramIndex(id)].load(std::memory_order_relaxed); }

    void runBatch() {
        rpverb::BatchIO io;
        memcpy(io.in, carry, sizeof carry);
        const bool side = get(kIdSideInput) >= 0.5;
        io.conn[0] = io.conn[1] = true;
        io.conn[2] = io.conn[3] = side;
        io.playing = playing;
        io.sampleRate = rate;
        io.powerMode = int(get(kIdPower));
        io.resetButton = get(kIdReset) >= 0.5 || resetRequest.exchange(false);
        io.space = get(94);
        io.colour = get(95);
        io.envelopeMode = get(43);
        int tags[kParamCount];
        double vals[kParamCount];
        int n = 0, changes = 0;
        const bool all = sendAll.exchange(false);
        for (int i = 0; i < kParamCount; i++) {
            const double v = value[i].load(std::memory_order_relaxed);
            if (kParams[i].cv >= 0) {
                const double m = mod[i].load(std::memory_order_relaxed);
                io.cv[kParams[i].cv] = m;
                io.cvConn[kParams[i].cv] = m != 0;
            }
            if (kParams[i].id == kIdSideInput) { applied[i] = v; continue; }
            if (!all && v == applied[i]) continue;
            applied[i] = v;
            changes++;
            if (kParams[i].id < 128) { tags[n] = int(kParams[i].id); vals[n] = v; n++; }
        }
        // Reason sends a burst of changes when its transport stops, and "Reset on Stop" works off it
        // (measured in Reason: with it on the tail is cut at stop, with it off the tail rings on)
        if (rpverb::U8(dev->state(), 0x1b39) && !playing) changes += 11;
        io.changes = changes;
        dev->render(io, tags, vals, n);
        resetCounter.store(rpverb::I(dev->state(), 0x19c9b34), std::memory_order_relaxed);
        quiet.store(rpverb::I(dev->state(), 0x1f48), std::memory_order_relaxed);
        if (io.outWritten) memcpy(prevOut, io.out, sizeof prevOut);
        else memset(prevOut, 0, sizeof prevOut);  // the original leaves the host buffer silent
    }

    // One batch on silence, then more until the reset it started has cleared the engine, so no input
    // falls into the original's mute. Reason does the same before a device's first audio.
    void settleBatch() {
        memset(carry, 0, sizeof carry);
        runBatch();
        for (int k = 0; k < 1000 && dev->resetPending(); k++) runBatch();
        fill = 0;
        memset(prevOut, 0, sizeof prevOut);
    }

    void process(const clap_process_t* p) {
        const uint32_t frames = p->frames_count;
        emitUiEvents(p->out_events);
        if (p->transport) playing = (p->transport->flags & CLAP_TRANSPORT_IS_PLAYING) != 0;
        const float* in[4] = {nullptr, nullptr, nullptr, nullptr};
        if (p->audio_inputs_count > 0 && p->audio_inputs[0].data32) {
            const auto& b = p->audio_inputs[0];
            in[0] = b.data32[0];
            in[1] = b.channel_count > 1 ? b.data32[1] : b.data32[0];
        }
        if (p->audio_inputs_count > 1 && p->audio_inputs[1].data32) {
            const auto& b = p->audio_inputs[1];
            in[2] = b.data32[0];
            in[3] = b.channel_count > 1 ? b.data32[1] : b.data32[0];
        }
        float* out[2] = {nullptr, nullptr};
        if (p->audio_outputs_count > 0 && p->audio_outputs[0].data32) {
            const auto& b = p->audio_outputs[0];
            out[0] = b.data32[0];
            out[1] = b.channel_count > 1 ? b.data32[1] : nullptr;
        }
        const uint32_t nev = p->in_events ? p->in_events->size(p->in_events) : 0;
        uint32_t ev = 0;
        for (uint32_t i = 0; i < frames; i++) {
            while (ev < nev) {
                const clap_event_header_t* h = p->in_events->get(p->in_events, ev);
                if (h->time > i) break;
                handleEvent(h);
                ev++;
            }
            float x[4];
            for (int c = 0; c < 4; c++) x[c] = in[c] ? in[c][i] : 0.0f;
            if (out[0]) out[0][i] = prevOut[0][fill];
            if (out[1]) out[1][i] = prevOut[1][fill];
            for (int c = 0; c < 4; c++) carry[c][fill] = x[c];
            if (++fill == rpverb::kBatch) { runBatch(); fill = 0; }
        }
        for (; ev < nev; ev++) handleEvent(p->in_events->get(p->in_events, ev));
    }
};

Plugin* self(const clap_plugin_t* p) { return static_cast<Plugin*>(p->plugin_data); }

// ---------------------------------------------------------------- audio ports
uint32_t portsCount(const clap_plugin_t*, bool isInput) { return isInput ? 2 : 1; }
bool portsGet(const clap_plugin_t*, uint32_t index, bool isInput, clap_audio_port_info_t* info) {
    if (index >= (isInput ? 2u : 1u)) return false;
    memset(info, 0, sizeof *info);
    info->id = index;
    snprintf(info->name, sizeof info->name, "%s", index == 0 ? (isInput ? "In" : "Out") : "Side");
    info->flags = index == 0 ? CLAP_AUDIO_PORT_IS_MAIN : 0;
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = index == 0 ? 0 : CLAP_INVALID_ID;
    return true;
}
const clap_plugin_audio_ports_t kPorts = {portsCount, portsGet};

// ---------------------------------------------------------------- params
uint32_t paramsCount(const clap_plugin_t*) { return kParamCount; }
bool paramsInfo(const clap_plugin_t*, uint32_t index, clap_param_info_t* info) {
    if (index >= uint32_t(kParamCount)) return false;
    const ParamDef& d = kParams[index];
    memset(info, 0, sizeof *info);
    info->id = d.id;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    if (d.steps) info->flags |= CLAP_PARAM_IS_STEPPED;
    if (d.steps > 2 || d.labels == kRouting || d.labels == kEnsembleRouting) info->flags |= CLAP_PARAM_IS_ENUM;
    if (d.cv >= 0) info->flags |= CLAP_PARAM_IS_MODULATABLE;
    snprintf(info->name, sizeof info->name, "%s", d.name);
    snprintf(info->module, sizeof info->module, "%s", d.module);
    info->min_value = 0;
    info->max_value = d.steps ? d.steps - 1 : 1;
    info->default_value = d.def;
    return true;
}
bool paramsValue(const clap_plugin_t* p, clap_id id, double* v) {
    const int i = paramIndex(id);
    if (i < 0) return false;
    *v = self(p)->value[i].load(std::memory_order_relaxed);
    return true;
}
bool paramsToText(const clap_plugin_t*, clap_id id, double v, char* out, uint32_t size) {
    return formatValue(paramIndex(id), v, out, size);
}
bool paramsFromText(const clap_plugin_t*, clap_id id, const char* text, double* v) {
    const int i = paramIndex(id);
    if (i < 0 || !text) return false;
    const ParamDef& d = kParams[i];
    if (d.steps) {
        for (int k = 0; k < d.steps; k++) if (!strcmp(text, d.labels[k])) { *v = k; return true; }
        return false;
    }
    char* end = nullptr;
    const double x = strtod(text, &end);
    if (end == text || !std::isfinite(x)) return false;
    *v = Plugin::clampValue(i, x / 100.0);
    return true;
}
void paramsFlush(const clap_plugin_t* p, const clap_input_events_t* in, const clap_output_events_t* out) {
    Plugin* s = self(p);
    s->emitUiEvents(out);
    const uint32_t n = in ? in->size(in) : 0;
    for (uint32_t k = 0; k < n; k++) s->handleEvent(in->get(in, k));
}
const clap_plugin_params_t kParamsExt = {paramsCount, paramsInfo, paramsValue, paramsToText, paramsFromText, paramsFlush};

// ---------------------------------------------------------------- state
bool writeAll(const clap_ostream_t* s, const void* d, size_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(d);
    while (n) {
        const int64_t w = s->write(s, b, n);
        if (w <= 0) return false;
        b += w; n -= size_t(w);
    }
    return true;
}
bool stateSave(const clap_plugin_t* p, const clap_ostream_t* s) {
    if (!writeAll(s, kStateMagic, 8)) return false;
    const uint32_t count = kParamCount;
    if (!writeAll(s, &count, 4)) return false;
    for (int i = 0; i < kParamCount; i++) {
        const uint32_t id = kParams[i].id;
        const double v = self(p)->value[i].load(std::memory_order_relaxed);
        if (!writeAll(s, &id, 4) || !writeAll(s, &v, 8)) return false;
    }
    return true;
}
bool stateLoad(const clap_plugin_t* p, const clap_istream_t* s) {
    std::vector<uint8_t> data;
    uint8_t buf[4096];
    for (;;) {
        const int64_t r = s->read(s, buf, sizeof buf);
        if (r < 0) return false;
        if (r == 0) break;
        data.insert(data.end(), buf, buf + r);
        if (data.size() > (1u << 20)) return false;
    }
    if (data.size() < 12 || memcmp(data.data(), kStateMagic, 8)) return false;
    uint32_t count;
    memcpy(&count, data.data() + 8, 4);
    if (count > 1024 || data.size() < 12 + size_t(count) * 12) return false;
    Plugin* self_ = self(p);
    for (int i = 0; i < kParamCount; i++) self_->value[i].store(kParams[i].def, std::memory_order_relaxed);
    for (uint32_t k = 0; k < count; k++) {
        uint32_t id; double v;
        memcpy(&id, data.data() + 12 + k * 12, 4);
        memcpy(&v, data.data() + 16 + k * 12, 8);
        const int i = paramIndex(id);
        if (i >= 0) self_->value[i].store(Plugin::clampValue(i, v), std::memory_order_relaxed);
    }
    self_->sendAll.store(true);  // like a patch load in Reason: every property arrives as a change
    if (self_->hostParams) self_->hostParams->rescan(self_->host, CLAP_PARAM_RESCAN_VALUES);
    return true;
}
const clap_plugin_state_t kState = {stateSave, stateLoad};

// ---------------------------------------------------------------- remote controls
uint32_t remoteCount(const clap_plugin_t*) { return kRemotePageCount; }
bool remoteGet(const clap_plugin_t*, uint32_t index, clap_remote_controls_page_t* page) {
    if (!page || index >= uint32_t(kRemotePageCount)) return false;
    memset(page, 0, sizeof *page);
    snprintf(page->section_name, sizeof page->section_name, "%s", "RPVerb");
    page->page_id = index;
    snprintf(page->page_name, sizeof page->page_name, "%s", kRemotePages[index].name);
    for (uint32_t i = 0; i < CLAP_REMOTE_CONTROLS_COUNT; i++)
        page->param_ids[i] = kRemotePages[index].ids[i] == kNone ? CLAP_INVALID_ID : kRemotePages[index].ids[i];
    page->is_for_preset = false;
    return true;
}
const clap_plugin_remote_controls_t kRemote = {remoteCount, remoteGet};

// ---------------------------------------------------------------- gui (macOS)
#ifdef __APPLE__
bool guiSupported(const clap_plugin_t*, const char* api, bool floating) { return !floating && api && !strcmp(api, CLAP_WINDOW_API_COCOA); }
bool guiPreferred(const clap_plugin_t*, const char** api, bool* floating) { *api = CLAP_WINDOW_API_COCOA; *floating = false; return true; }
bool guiCreate(const clap_plugin_t* p, const char* api, bool floating) {
    if (!guiSupported(p, api, floating)) return false;
    Plugin* s = self(p);
    if (s->gui) gui::destroy(s->gui);
    s->gui = gui::create(*s);
    return s->gui != nullptr;
}
void guiDestroy(const clap_plugin_t* p) { Plugin* s = self(p); if (s->gui) gui::destroy(s->gui); s->gui = nullptr; }
bool guiSetScale(const clap_plugin_t*, double) { return false; }
bool guiGetSize(const clap_plugin_t* p, uint32_t* w, uint32_t* h) { return gui::size(self(p)->gui, w, h); }
bool guiCanResize(const clap_plugin_t*) { return false; }
bool guiHints(const clap_plugin_t*, clap_gui_resize_hints_t*) { return false; }
bool guiAdjust(const clap_plugin_t* p, uint32_t* w, uint32_t* h) { return gui::size(self(p)->gui, w, h); }
bool guiSetSize(const clap_plugin_t* p, uint32_t w, uint32_t h) {
    uint32_t cw, ch;
    return gui::size(self(p)->gui, &cw, &ch) && cw == w && ch == h;
}
bool guiSetParent(const clap_plugin_t* p, const clap_window_t* w) { return w && gui::setParent(self(p)->gui, w->cocoa); }
bool guiSetTransient(const clap_plugin_t*, const clap_window_t*) { return false; }
void guiSuggestTitle(const clap_plugin_t*, const char*) {}
bool guiShow(const clap_plugin_t* p) { return gui::show(self(p)->gui, true); }
bool guiHide(const clap_plugin_t* p) { return gui::show(self(p)->gui, false); }
const clap_plugin_gui_t kGui = {guiSupported, guiPreferred, guiCreate, guiDestroy, guiSetScale, guiGetSize,
                                guiCanResize, guiHints, guiAdjust, guiSetSize, guiSetParent, guiSetTransient,
                                guiSuggestTitle, guiShow, guiHide};
#endif

// ---------------------------------------------------------------- latency, tail
uint32_t latencyGet(const clap_plugin_t*) { return kLatency; }
const clap_plugin_latency_t kLatencyExt = {latencyGet};
uint32_t tailGet(const clap_plugin_t* p) { return uint32_t(self(p)->rate * 30.0); }
const clap_plugin_tail_t kTail = {tailGet};

// ---------------------------------------------------------------- plugin
bool pInit(const clap_plugin_t* p) {
    Plugin* s = self(p);
    s->hostParams = static_cast<const clap_host_params_t*>(s->host->get_extension(s->host, CLAP_EXT_PARAMS));
    return true;
}
void pDestroy(const clap_plugin_t* p) {
#ifdef __APPLE__
    guiDestroy(p);
#endif
    delete self(p);
}
bool pActivate(const clap_plugin_t* p, double rate, uint32_t, uint32_t) {
    if (!std::isfinite(rate) || rate < 1000 || rate > 768000) return false;
    Plugin* s = self(p);
    s->dev.reset();
    s->dev.reset(new (std::nothrow) rpverb::Device());
    if (!s->dev || !s->dev->ok()) { s->dev.reset(); return false; }
    s->rate = rate;
    s->playing = false;
    // A fresh device, started as Reason starts one: every property arrives at creation and again from
    // the patch, and both resets finish before the first audio (a Reason export matches this to 24 bits).
    for (int pass = 0; pass < 2; pass++) {
        s->sendAll.store(true);
        s->settleBatch();
    }
    return true;
}
void pDeactivate(const clap_plugin_t*) {}
bool pStartProcessing(const clap_plugin_t*) { return true; }
void pStopProcessing(const clap_plugin_t*) {}
void pReset(const clap_plugin_t* p) {
    Plugin* s = self(p);
    s->resetRequest.store(true);  // the panel's Reset, finished at once
    s->settleBatch();
}
clap_process_status pProcess(const clap_plugin_t* p, const clap_process_t* pr) {
    self(p)->process(pr);
    return CLAP_PROCESS_CONTINUE;
}
const void* pExtension(const clap_plugin_t*, const char* id) {
    if (!strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &kPorts;
    if (!strcmp(id, CLAP_EXT_PARAMS)) return &kParamsExt;
    if (!strcmp(id, CLAP_EXT_STATE)) return &kState;
    if (!strcmp(id, CLAP_EXT_LATENCY)) return &kLatencyExt;
    if (!strcmp(id, CLAP_EXT_TAIL)) return &kTail;
#ifdef __APPLE__
    if (!strcmp(id, CLAP_EXT_GUI)) return &kGui;
#endif
    if (!strcmp(id, CLAP_EXT_REMOTE_CONTROLS) || !strcmp(id, CLAP_EXT_REMOTE_CONTROLS_COMPAT)) return &kRemote;
    return nullptr;
}
void pMainThread(const clap_plugin_t*) {}

const clap_plugin_t* create(const clap_host_t* host) {
    Plugin* s = new (std::nothrow) Plugin();
    if (!s) return nullptr;
    s->host = host;
    s->clap = {&kDescriptor, s, pInit, pDestroy, pActivate, pDeactivate, pStartProcessing, pStopProcessing,
               pReset, pProcess, pExtension, pMainThread};
    return &s->clap;
}

// ---------------------------------------------------------------- factory, entry
uint32_t fCount(const clap_plugin_factory_t*) { return 1; }
const clap_plugin_descriptor_t* fDescribe(const clap_plugin_factory_t*, uint32_t i) { return i == 0 ? &kDescriptor : nullptr; }
const clap_plugin_t* fCreate(const clap_plugin_factory_t*, const clap_host_t* host, const char* id) {
    if (!host || !id || strcmp(id, kDescriptor.id) || !clap_version_is_compatible(host->clap_version)) return nullptr;
    return create(host);
}
const clap_plugin_factory_t kFactory = {fCount, fDescribe, fCreate};

bool eInit(const char*) { return true; }
void eDeinit() {}
const void* eFactory(const char* id) { return !strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? &kFactory : nullptr; }

}  // namespace

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {CLAP_VERSION_INIT, eInit, eDeinit, eFactory};
