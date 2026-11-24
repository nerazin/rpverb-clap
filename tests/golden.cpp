// Golden test: the port's output hash for a fixed stimulus, against the hash the lab measured on the
// original RPVerb 1.0.7 x86-64 image (run under Rosetta) with the same stimulus.
#include <cinttypes>
#include <cstdio>
#include "golden_stimulus.hpp"

int main() {
    using namespace rpverb_golden;
    // from the lab oracle (`oracle golden`), original image, x86-64 libm
    const uint64_t kExpected[2] = {0x3555b5d2f73a8a30ull, 0x658983f84dcdb7a9ull};
    int failed = 0;
    for (int seed = 1; seed <= 2; seed++) {
        rpverb::Device dev;
        if (!dev.ok()) { fprintf(stderr, "allocation failed\n"); return 2; }
        Stim st{uint64_t(seed)};
        std::vector<int> tags;
        std::vector<double> vals;
        uint64_t h = 0xcbf29ce484222325ull;
        for (long b = 0; b < kBatches; b++) {
            rpverb::BatchIO io;
            st.batch(b, io, tags, vals);
            dev.render(io, tags.data(), vals.data(), int(tags.size()));
            if (!io.outWritten) memset(io.out, 0, sizeof io.out);
            h = fnv(h, io.out, sizeof io.out);
        }
        const bool ok = h == kExpected[seed - 1];
        printf("seed %d: hash %016" PRIx64 " %s\n", seed, h, ok ? "ok" : "MISMATCH");
        failed += !ok;
    }
    return failed ? 1 : 0;
}
