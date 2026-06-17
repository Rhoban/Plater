// Prototype benchmark: bit-packed Bitmap::overlaps vs the scalar reference.
// Builds representative plate/part bitmaps, validates the two paths agree, then
// times them on a worst-case (full miss) and a typical (mixed) plate.
#include <cstdio>
#include <cstdint>
#include <chrono>
#include "Bitmap.h"

using namespace Plater;
using namespace std::chrono;

// Small deterministic PRNG (xorshift) so runs are reproducible.
static uint64_t s = 88172645463325252ULL;
static uint64_t rnd() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }

int main()
{
    const int PW = 600, PH = 600;   // plate: 300mm @ 0.5mm precision
    const int QW = 160, QH = 160;   // part: 80mm

    // Part: a filled disk (typical-ish footprint).
    Bitmap part(QW, QH);
    int cx = QW / 2, cy = QH / 2, r = QW / 2 - 2;
    for (int x = 0; x < QW; x++)
        for (int y = 0; y < QH; y++)
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) < r * r)
                part.setPoint(x, y, 2);

    // A plate scattered with ~40 placed blocks (~30% fill), for validation
    // and the "mixed" timing scenario.
    Bitmap plateFilled(PW, PH);
    for (int b = 0; b < 40; b++) {
        int bw = 20 + rnd() % 60, bh = 20 + rnd() % 60;
        int ox = rnd() % (PW - bw), oy = rnd() % (PH - bh);
        for (int x = 0; x < bw; x++)
            for (int y = 0; y < bh; y++)
                plateFilled.setPoint(ox + x, oy + y, 2);
    }

    // ---- Correctness: masked path must equal the scalar reference ----
    long checks = 0, mismatches = 0;
    for (int i = 0; i < 50000; i++) {
        int offx = rnd() % (PW - QW), offy = rnd() % (PH - QH);
        bool a = part.overlapsScalar(&plateFilled, offx, offy);
        bool c = part.overlaps(&plateFilled, offx, offy);
        checks++;
        if (a != c) mismatches++;
    }
    printf("validation: %ld checks, %ld mismatches -> %s\n\n",
           checks, mismatches, mismatches ? "FAIL" : "OK");

    auto bench = [&](const char *label, Bitmap &plate, int iters) {
        part.ensureMask();
        plate.ensureMask();
        volatile int sink = 0;
        auto t0 = high_resolution_clock::now();
        for (int i = 0; i < iters; i++) {
            int offx = i % (PW - QW), offy = (i * 37) % (PH - QH);
            sink ^= part.overlapsScalar(&plate, offx, offy);
        }
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < iters; i++) {
            int offx = i % (PW - QW), offy = (i * 37) % (PH - QH);
            sink ^= part.overlaps(&plate, offx, offy);
        }
        auto t2 = high_resolution_clock::now();
        double sca = duration_cast<nanoseconds>(t1 - t0).count() / (double)iters;
        double msk = duration_cast<nanoseconds>(t2 - t1).count() / (double)iters;
        printf("%-22s scalar=%8.1f ns   masked=%8.1f ns   speedup=%5.1fx\n",
               label, sca, msk, sca / msk);
        (void)sink;
    };

    Bitmap plateEmpty(PW, PH);  // worst case: every test is a full-scan miss
    bench("empty plate (miss)", plateEmpty, 200000);
    bench("filled plate (mixed)", plateFilled, 200000);

    return 0;
}
