// Renders the plugin's GUI to a PNG through the CLAP gui extension, for review without a host:
//   rpverb_gui_snapshot <plugin binary> out.png [routed]
// "routed" first sets a different routing (EQ post, early into reverb, late direct, ensemble
// direct, envelope on in "Space Size" mode, HQ off) to show the wires change.
#import <AppKit/AppKit.h>
#include <clap/clap.h>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <vector>

static const void* ext(const clap_host_t*, const char*) { return nullptr; }
static void noop(const clap_host_t*) {}
static clap_host_t kHost = {CLAP_VERSION_INIT, nullptr, "snapshot", "test", "", "1", ext, noop, noop, noop};

struct Events {
    std::vector<clap_event_param_value_t> v;
    static uint32_t size(const clap_input_events_t* l) { return uint32_t(static_cast<Events*>(l->ctx)->v.size()); }
    static const clap_event_header_t* get(const clap_input_events_t* l, uint32_t i) { return &static_cast<Events*>(l->ctx)->v[i].header; }
};

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <plugin binary> out.png [routed]\n", argv[0]); return 2; }
    @autoreleasepool {
        [NSApplication sharedApplication];
        void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 2; }
        auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(lib, "clap_entry"));
        entry->init(argv[1]);
        auto* fac = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
        const clap_plugin_t* p = fac->create_plugin(fac, &kHost, fac->get_plugin_descriptor(fac, 0)->id);
        if (!p || !p->init(p)) { fprintf(stderr, "init failed\n"); return 1; }
        auto* params = static_cast<const clap_plugin_params_t*>(p->get_extension(p, CLAP_EXT_PARAMS));
        auto* gui = static_cast<const clap_plugin_gui_t*>(p->get_extension(p, CLAP_EXT_GUI));
        if (!gui || !gui->is_api_supported(p, CLAP_WINDOW_API_COCOA, false)) { fprintf(stderr, "no cocoa gui\n"); return 1; }
        if (argc > 3 && !strcmp(argv[3], "routed")) {
            Events ev; clap_input_events_t in{&ev, Events::size, Events::get};
            const struct { clap_id id; double v; } set[] = {{38, 1}, {39, 1}, {70, 1}, {71, 1}, {83, 1}, {84, 0}, {28, 1}, {29, 1},
                                                           {37, 1}, {43, 1}, {100, 0}, {50, 1}, {53, 0.4}, {94, 14}, {95, 4}};
            for (auto& s : set) {
                clap_event_param_value_t e{}; e.header = {sizeof e, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
                e.param_id = s.id; e.note_id = -1; e.port_index = -1; e.channel = -1; e.key = -1; e.value = s.v; ev.v.push_back(e);
            }
            params->flush(p, &in, nullptr);
        }
        p->activate(p, 44100, 1, 512);
        float z[512] = {}, oL[512], oR[512];
        float* ins[2] = {z, z}; float* outs[2] = {oL, oR};
        clap_audio_buffer_t ib{ins, nullptr, 2, 0, 0}, ob{outs, nullptr, 2, 0, 0};
        clap_process_t pr{}; pr.steady_time = -1; pr.frames_count = 512; pr.audio_inputs = &ib; pr.audio_inputs_count = 1;
        pr.audio_outputs = &ob; pr.audio_outputs_count = 1;
        for (int k = 0; k < 40; k++) p->process(p, &pr);
        if (!gui->create(p, CLAP_WINDOW_API_COCOA, false)) { fprintf(stderr, "gui create failed\n"); return 1; }
        uint32_t w = 0, h = 0;
        gui->get_size(p, &w, &h);
        NSView* parent = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
        clap_window_t win{}; win.api = CLAP_WINDOW_API_COCOA; win.cocoa = parent;
        if (!gui->set_parent(p, &win) || !gui->show(p)) { fprintf(stderr, "set_parent/show failed\n"); return 1; }
        [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];  // one refresh tick
        NSBitmapImageRep* rep = [parent bitmapImageRepForCachingDisplayInRect:parent.bounds];
        [parent cacheDisplayInRect:parent.bounds toBitmapImageRep:rep];
        NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        const bool ok = [png writeToFile:[NSString stringWithUTF8String:argv[2]] atomically:YES];
        gui->destroy(p);
        [parent release];
        p->deactivate(p);
        p->destroy(p);
        entry->deinit();
        printf("%s %s (%ux%u)\n", ok ? "wrote" : "FAILED", argv[2], w, h);
        return ok ? 0 : 1;
    }
}
