#include <utils/sbs_rand.h>

#include <array>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {

void check(bool ok, const char* message) {
    if (!ok) {
        std::cerr << "RNG isolation: " << message << '\n';
        std::abort();
    }
}

}  // namespace

int main() {
    // Fixed Park–Miller values are an independent golden oracle.
    constexpr int golden[] = {16807, 282475249, 1622650073, 984943658, 1144108930};
    SbsRng boundary(1);
    for (int value : golden) {
        check(boundary.next() == value, "MINSTD golden sequence");
    }
    boundary.seed(0);
    check(boundary.state() == 123459876ULL, "zero seed resets state");
    boundary.seed(2147483647U);
    check(boundary.next() == 0 && boundary.state() == 123459876ULL,
          "modulus hit returns zero and resets state");

    // Two explicit streams can be interleaved with the non-battle legacy stream.
    sbs_srand(7);
    SbsRng a(1), b(1);
    check(a.next() == golden[0] && a.next() == golden[1], "first stream advances twice");
    check(sbs_rand() == 117649, "explicit draws leave the legacy stream unchanged");
    check(b.next() == golden[0], "second stream starts at its own seed");
    check(a.next() == golden[2] && b.next() == golden[1], "interleaved streams keep their positions");
    b.seed(2);
    check(b.next() == 33614 && a.next() == golden[3], "reseeding one stream leaves the other unchanged");
    check(sbs_rand_state() == 117649, "explicit reseed leaves the legacy stream unchanged");

    std::array<int, 10000> expected;
    SbsRng reference(1234);
    for (int& draw : expected) {
        draw = reference.next();
    }
    std::array<std::thread, 2> threads;
    for (unsigned int n = 0; n < threads.size(); ++n) {
        threads[n] = std::thread([&, n] {
            const unsigned int legacy_seed = 9 + n;
            sbs_srand(legacy_seed);
            SbsRng rng(1234);
            for (int value : expected) {
                check(rng.next() == value, "explicit stream is reproducible across threads");
            }
            check(sbs_rand_state() == legacy_seed, "legacy seeding is thread-local");
            check(sbs_rand() == static_cast<int>(16807 * legacy_seed), "legacy draw is thread-local");
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    check(sbs_rand_state() == 117649, "worker draws leave the caller's legacy stream unchanged");
}
