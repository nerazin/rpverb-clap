// Host smoke test: loads the built plugin through the CLAP C ABI like a host and checks descriptors,
// parameters, state round trip, latency, and that its output equals a direct rpverb::Device run
// (64-frame batches, 64 frames late, changes applied at the next batch) sample for sample.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <vector>
#include <clap/clap.h>
#include "rpverb_device.hpp"
#include "../src/params.hpp"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static const void* hostExt(const clap_host_t*, const char*) { return nullptr; }
static void hostNoop(const clap_host_t*) {}
static clap_host_t kHost = {CLAP_VERSION_INIT, nullptr, "smoke", "test", "", "1", hostExt, hostNoop, hostNoop, hostNoop};

struct Events {
    std::vector<clap_event_param_value_t> v;
    static uint32_t size(const clap_input_events_t* l) { return uint32_t(static_cast<Events*>(l->ctx)->v.size()); }
    static const clap_event_header_t* get(const clap_input_events_t* l, uint32_t i) { return &static_cast<Events*>(l->ctx)->v[i].header; }
};
struct Buf { std::vector<uint8_t> d; size_t pos = 0; };
static int64_t bufWrite(const clap_ostream_t* s, const void* p, uint64_t n) { auto* b = static_cast<Buf*>(s->ctx); b->d.insert(b->d.end(), (const uint8_t*)p, (const uint8_t*)p + n); return int64_t(n); }
static int64_t bufRead(const clap_istream_t* s, void* p, uint64_t n) { auto* b = static_cast<Buf*>(s->ctx); n = std::min<uint64_t>(n, b->d.size() - b->pos); memcpy(p, b->d.data() + b->pos, n); b->pos += n; return int64_t(n); }

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: rpverb_host_test <plugin binary>\n"); return 2; }
    void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 2; }
    auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(lib, "clap_entry"));
    CHECK(entry && entry->init(argv[1]));
    auto* fac = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    CHECK(fac && fac->get_plugin_count(fac) == 1);
    const clap_plugin_descriptor_t* desc = fac->get_plugin_descriptor(fac, 0);
    const clap_plugin_t* p = fac->create_plugin(fac, &kHost, desc->id);
    CHECK(p && p->init(p));
    auto* params = static_cast<const clap_plugin_params_t*>(p->get_extension(p, CLAP_EXT_PARAMS));
    auto* state = static_cast<const clap_plugin_state_t*>(p->get_extension(p, CLAP_EXT_STATE));
    auto* lat = static_cast<const clap_plugin_latency_t*>(p->get_extension(p, CLAP_EXT_LATENCY));
    CHECK(params && state && lat);
    CHECK(params->count(p) == uint32_t(rpverb_clap::kParamCount));
    for (uint32_t i = 0; i < params->count(p); i++) {
        clap_param_info_t info; CHECK(params->get_info(p, i, &info));
        double v; CHECK(params->get_value(p, info.id, &v) && v == info.default_value);
        char text[64]; CHECK(params->value_to_text(p, info.id, v, text, sizeof text));
        double back; CHECK(params->text_to_value(p, info.id, text, &back));
        if (info.flags & CLAP_PARAM_IS_STEPPED) CHECK(back == v);
    }
    CHECK(lat->get(p) == 64);

    // state round trip with changed values
    Events ev; clap_input_events_t in{&ev, Events::size, Events::get};
    auto push = [&](uint32_t t, clap_id id, double v) {
        clap_event_param_value_t e{}; e.header = {sizeof e, t, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
        e.param_id = id; e.note_id = -1; e.port_index = -1; e.channel = -1; e.key = -1; e.value = v; ev.v.push_back(e);
    };
    push(0, 94, 7); push(0, 3, 0.25); push(0, 43, 4);
    params->flush(p, &in, nullptr);
    ev.v.clear();
    Buf saved; clap_ostream_t os{&saved, bufWrite};
    CHECK(state->save(p, &os));
    push(0, 94, 0); push(0, 3, 0.9);
    params->flush(p, &in, nullptr);
    ev.v.clear();
    clap_istream_t is{&saved, bufRead};
    CHECK(state->load(p, &is));
    double v; params->get_value(p, 94, &v); CHECK(v == 7);
    params->get_value(p, 3, &v); CHECK(v == 0.25);
    params->get_value(p, 43, &v); CHECK(v == 4);

    // processing against a direct device run
    CHECK(p->activate(p, 44100, 1, 512) && p->start_processing(p));
    rpverb::Device ref;
    double cur[256]; bool pending = true;  // the plugin reports every property on its first batch
    for (int i = 0; i < rpverb_clap::kParamCount; i++) cur[rpverb_clap::kParams[i].id] = rpverb_clap::kParams[i].def;
    cur[94] = 7; cur[3] = 0.25; cur[43] = 4;
    double applied[256]; memcpy(applied, cur, sizeof cur);
    std::vector<float> inL, inR, outL, outR, refL, refR;
    uint64_t seed = 7;
    auto rnd = [&]() { seed = seed * 6364136223846793005ull + 1442695040888963407ull; return double(seed >> 11) / 9007199254740992.0; };
    const int total = 64 * 900;
    for (int i = 0; i < total; i++) { inL.push_back(float(rnd() * 2 - 1) * 0.5f); inR.push_back(float(rnd() * 2 - 1) * 0.5f); }
    outL.resize(total); outR.resize(total);
    std::vector<std::pair<int, std::pair<clap_id, double>>> moves;  // absolute frame, id, value
    for (int k = 0; k < 120; k++) {
        const int at = int(rnd() * total);
        const int pi = int(rnd() * rpverb_clap::kParamCount);
        const auto& d = rpverb_clap::kParams[pi];
        if (d.id == rpverb_clap::kIdPower || d.id == rpverb_clap::kIdReset || d.id == rpverb_clap::kIdSideInput) continue;
        moves.push_back({at, {d.id, d.steps ? double(int(rnd() * d.steps)) : rnd()}});
    }
    std::sort(moves.begin(), moves.end(), [](auto& a, auto& b) { return a.first < b.first; });
    size_t mi = 0;
    for (int pos = 0; pos < total;) {
        const int n = std::min(total - pos, 1 + int(rnd() * 700));
        ev.v.clear();
        while (mi < moves.size() && moves[mi].first < pos + n) { push(uint32_t(moves[mi].first - pos), moves[mi].second.first, moves[mi].second.second); mi++; }
        float* ins[2] = {&inL[pos], &inR[pos]}; float* outs[2] = {&outL[pos], &outR[pos]};
        clap_audio_buffer_t ib{ins, nullptr, 2, 0, 0}, ob{outs, nullptr, 2, 0, 0};
        clap_output_events_t oe{nullptr, [](const clap_output_events_t*, const clap_event_header_t*) { return true; }};
        clap_process_t pr{}; pr.steady_time = -1; pr.frames_count = uint32_t(n); pr.audio_inputs = &ib; pr.audio_inputs_count = 1;
        pr.audio_outputs = &ob; pr.audio_outputs_count = 1; pr.in_events = &in; pr.out_events = &oe;
        CHECK(p->process(p, &pr) == CLAP_PROCESS_CONTINUE);
        pos += n;
    }
    // reference: batch k sees every move at frames <= 64k+63
    mi = 0;
    refL.assign(64, 0.f); refR.assign(64, 0.f);
    for (int b = 0; b * 64 < total - 64; b++) {
        while (mi < moves.size() && moves[mi].first <= b * 64 + 63) { cur[moves[mi].second.first] = moves[mi].second.second; mi++; }
        rpverb::BatchIO io;
        memcpy(io.in[0], &inL[b * 64], 256); memcpy(io.in[1], &inR[b * 64], 256);
        io.conn[2] = io.conn[3] = false;
        io.space = cur[94]; io.colour = cur[95]; io.envelopeMode = cur[43];
        std::vector<int> tags; std::vector<double> vals; int changes = 0;
        for (int i = 0; i < rpverb_clap::kParamCount; i++) {
            const uint32_t id = rpverb_clap::kParams[i].id;
            if (id == rpverb_clap::kIdSideInput) continue;
            if (!pending && cur[id] == applied[id]) continue;
            applied[id] = cur[id]; changes++;
            if (id < 128) { tags.push_back(int(id)); vals.push_back(cur[id]); }
        }
        pending = false;
        io.changes = changes;
        ref.render(io, tags.data(), vals.data(), int(tags.size()));
        if (!io.outWritten) memset(io.out, 0, sizeof io.out);
        refL.insert(refL.end(), io.out[0], io.out[0] + 64); refR.insert(refR.end(), io.out[1], io.out[1] + 64);
    }
    long differ = 0; double energy = 0;
    for (int i = 0; i < total; i++) {
        if (memcmp(&outL[i], &refL[i], 4) || memcmp(&outR[i], &refR[i], 4)) differ++;
        energy += double(outL[i]) * outL[i];
    }
    printf("process: %d frames, %zu moves, %ld samples differ from the direct device, output energy %.2f\n", total, moves.size(), differ, energy);
    CHECK(differ == 0 && energy > 1.0);
    p->stop_processing(p); p->deactivate(p); p->destroy(p); entry->deinit();
    printf("%s\n", fails ? "host smoke FAILED" : "host smoke ok");
    return fails ? 1 : 0;
}
