// Prints generated bars and checks the maths. Build: g++ -std=c++17 -I../Source EngineTest.cpp
#include "PatternEngine.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace forge;
static int failures = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf ("FAIL: %s\n", msg); ++failures; } } while (0)

static void printBar (const Bar& b)
{
    std::printf ("bar %lld\n        ", (long long) b.index);
    for (int i = 0; i < kSteps; ++i) std::printf ("%c", i % 4 == 0 ? '|' : '.');
    std::printf ("\n");
    for (int l = 0; l < NumLanes; ++l)
    {
        std::printf ("%-7s ", laneName (l));
        for (int i = 0; i < kSteps; ++i)
        {
            const auto& h = b.hits[l][i];
            std::printf ("%c", ! h.on() ? '-' : h.vel >= 105 ? 'X' : h.vel >= 70 ? 'x' : 'o');
        }
        if (l == Bass)
        {
            std::printf ("   notes:");
            for (int i = 0; i < kSteps; ++i) if (b.hits[l][i].on()) std::printf (" %d", b.hits[l][i].note);
        }
        std::printf ("\n");
    }
    std::printf ("KNOTS   ");
    for (int i = 0; i < kSteps; ++i) std::printf ("%c", b.knot[i] ? '*' : ' ');
    std::printf ("\n\n");
}

int main()
{
    Params p;
    p.evolve = 0.0f;   // the maths checks run on a fixed groove; Evolve is tested on its own below
    MelodyInfo none;

    // 1. Kick four-to-the-floor at full velocity, bass never on the kick
    for (int64_t bar = 0; bar < 32; ++bar)
    {
        auto b = generateBar (bar, p, none);
        for (int i = 0; i < kSteps; ++i)
        {
            CHECK ((i % 4 == 0) == b.hits[Kick][i].on() || (b.hits[Kick][i].on() && i == 14), "kick 4/4");
            if (i % 4 == 0) { CHECK (b.hits[Kick][i].vel == 127, "kick full velocity"); CHECK (! b.hits[Bass][i].on(), "bass off kick"); }
        }
    }

    // 2. Tresillo: with polymeter off the PERC lane is E(3,8) = 3+3+2 twice per bar
    {
        Params t = p; t.polymeter = false; t.density = 0.0f; t.seed = 1;
        auto b = generateBar (0, t, none);
        int hits = 0; for (int i = 0; i < kSteps; ++i) hits += b.hits[Perc][i].on();
        CHECK (hits == 6, "tresillo = 6 hits per bar");
        // gaps between hits must be 3,3,2 (rotated)
        std::string gaps; int last = -1, first = -1;
        for (int i = 0; i < kSteps; ++i) if (b.hits[Perc][i].on()) { if (last >= 0) gaps += char ('0' + i - last); else first = i; last = i; }
        gaps += char ('0' + first + 16 - last);
        CHECK (gaps.find ("332") != std::string::npos || gaps.find ("323") != std::string::npos || gaps.find ("233") != std::string::npos, "tresillo 3+3+2 gaps");
        std::printf ("tresillo gaps: %s\n", gaps.c_str());
    }

    // 3. Son clave 3-2 one-bar positions, and the extended two-bar clave
    {
        Params c = p; c.clave = Clave32;
        auto b = generateBar (0, c, none);
        for (int i : { 0, 3, 6, 10, 12 }) CHECK (b.hits[Rim][i].on(), "3-2 clave positions");
        c.clave = ClaveExtended;
        auto a = generateBar (0, c, none), b2 = generateBar (1, c, none);
        for (int i : { 0, 6, 12 }) CHECK (a.hits[Rim][i].on(), "extended clave bar A");
        for (int i : { 4, 8 })     CHECK (b2.hits[Rim][i].on(), "extended clave bar B");
    }

    // 4. Polymeter: perc pattern (length 12) must not repeat bar-to-bar, but repeats every 3 bars
    {
        Params q = p; q.variation = 0.0f; q.density = 0.0f; q.follow = 0.0f;
        auto a = generateBar (0, q, none), b = generateBar (1, q, none), c = generateBar (3, q, none);
        bool diff = false, same = true;
        for (int i = 0; i < kSteps; ++i)
        {
            diff |= a.hits[Perc][i].on() != b.hits[Perc][i].on();
            same &= a.hits[Perc][i].on() == c.hits[Perc][i].on();
        }
        CHECK (diff, "polymeter shifts against the bar");
        CHECK (same, "12-step loop realigns after 3 bars");
    }

    // 5. Crescendo: average hat velocity rises through the 4-bar cycle
    {
        auto avg = [&] (int64_t bar) { auto b = generateBar (bar, p, none); float s = 0; int n = 0;
            for (int i = 0; i < kSteps; ++i) { if (b.hits[OHat][i].on()) { s += b.hits[OHat][i].vel; ++n; } }
            return n ? s / n : 0.0f; };
        std::printf ("o-hat avg vel bars 0..3: %.0f %.0f %.0f %.0f\n", avg (0), avg (1), avg (2), avg (3));
        CHECK (avg (0) < avg (1) && avg (1) < avg (2) && avg (2) < avg (3), "crescendo rises");
    }

    // 6. Melody follow: feed a melody, drums lean onto its rhythm, bass follows its notes
    MelodyTracker tr;
    const int melody[][2] = { { 1, 57 }, { 3, 60 }, { 6, 64 }, { 9, 55 }, { 11, 57 }, { 14, 62 } };   // 16th step, pitch
    for (auto& m : melody) tr.noteOn (m[0] * 0.25, m[1], 100);
    tr.barChanged (1);
    const auto& mel = tr.get();
    CHECK (mel.valid, "melody captured");
    CHECK (mel.beatRoot[0] == 57 && mel.beatRoot[2] == 55, "lowest note per beat");
    {
        Params fp = p; fp.follow = 1.0f; fp.bassStyle = BassFollow;
        auto b = generateBar (1, fp, mel);
        for (auto& m : melody)
        {
            const int i = m[0];
            CHECK (b.hits[CHat][i].on(), "hat locks to melody onset");
            if (i % 4 != 0) CHECK (b.hits[Bass][i].on(), "bass plays with melody");
        }
        CHECK (b.hits[Bass][1].note % 12 == 57 % 12, "bass root A on beat 1");
        CHECK (b.hits[Bass][9].note % 12 == 55 % 12, "bass root G on beat 3");
        std::printf ("--- follow = 1, bass = follow, melody on steps 1 3 6 9 11 14\n");
        printBar (b);
    }

    // 7. Determinism: same params = same bar (keeps two instances in sync); different seed differs
    {
        auto a = generateBar (5, p, none), b = generateBar (5, p, none);
        Params s = p; s.seed = 77; auto c = generateBar (5, s, none);
        bool eq = true, ne = false;
        for (int l = 0; l < NumLanes; ++l) for (int i = 0; i < kSteps; ++i)
        { eq &= a.hits[l][i].vel == b.hits[l][i].vel; ne |= a.hits[l][i].vel != c.hits[l][i].vel; }
        CHECK (eq, "deterministic"); CHECK (ne, "seed changes groove");
    }

    // 8. Muting a lane doesn't move the bass
    {
        Params m = p; m.laneOn[Perc] = false; m.laneOn[Rim] = false;
        auto a = generateBar (2, p, none), b = generateBar (2, m, none);
        for (int i = 0; i < kSteps; ++i) CHECK (a.hits[Bass][i].note == b.hits[Bass][i].note, "mute keeps bass");
    }

    // 9. Evolve: over 256 bars (8 minutes at 128) the groove keeps changing
    {
        Params e = p; e.evolve = 0.8f; e.sectionBars = 32;
        auto sig = [] (const Bar& b) { std::string s; for (int l = 0; l < NumLanes; ++l) for (int i = 0; i < kSteps; ++i) s += char ('a' + b.hits[l][i].vel / 8); return s; };
        // compare 16-bar blocks: none should be an exact copy of another
        std::vector<std::string> blocks;
        for (int blk = 0; blk < 16; ++blk) { std::string s; for (int k = 0; k < 16; ++k) s += sig (generateBar (blk * 16 + k, e, none)); blocks.push_back (s); }
        int repeats = 0; for (size_t a = 0; a < blocks.size(); ++a) for (size_t b = a + 1; b < blocks.size(); ++b) repeats += blocks[a] == blocks[b];
        std::printf ("evolve: 16-bar blocks repeated %d times over 256 bars\n", repeats);
        CHECK (repeats == 0, "evolve: no 16-bar block repeats");

        // with evolve off the old 16-bar cycle comes back (proves the knob is what does it)
        Params off = e; off.evolve = 0.0f; off.lenHat = 16; off.lenPerc = 8; off.lenRide = 16; off.lenRim = 16;
        std::string a, b; for (int k = 0; k < 16; ++k) { a += sig (generateBar (k, off, none)); b += sig (generateBar (16 + k, off, none)); }
        CHECK (a == b, "evolve off: loop repeats");

        // backbone: kick 4/4 at 127 everywhere, bass never on the kick
        int lenChanges = 0, claveFlips = 0, density0 = 0, density1 = 0;
        for (int64_t bar = 0; bar < 256; ++bar)
        {
            auto g = generateBar (bar, e, none);
            for (int i = 0; i < kSteps; i += 4) { CHECK (g.hits[Kick][i].vel == 127, "evolve keeps kick"); CHECK (! g.hits[Bass][i].on(), "evolve keeps bass off kick"); }
            if (bar % 32 == 0 && bar > 0)
            {
                auto prev = generateBar (bar - 32, e, none);
                bool rimDiff = false; for (int i = 0; i < kSteps; ++i) rimDiff |= prev.hits[Rim][i].on() != g.hits[Rim][i].on();
                claveFlips += rimDiff;
            }
            int n = 0; for (int i = 0; i < kSteps; ++i) n += g.hits[CHat][i].on() + g.hits[Perc][i].on();
            if (bar % 32 < 4) density0 += n; else if (bar % 32 >= 28) density1 += n;
        }
        // loop length changes are visible as a different perc pattern at the top of each section
        for (int s = 1; s < 8; ++s)
        {
            auto a0 = generateBar (s * 32 + 8, e, none), b0 = generateBar ((s - 1) * 32 + 8, e, none);
            bool d = false; for (int i = 0; i < kSteps; ++i) d |= a0.hits[Perc][i].on() != b0.hits[Perc][i].on() || a0.hits[Ride][i].on() != b0.hits[Ride][i].on();
            lenChanges += d;
        }
        std::printf ("evolve: %d/7 sections changed perc/ride, %d clave flips, section start vs end hits: %d -> %d\n", lenChanges, claveFlips, density0, density1);
        CHECK (lenChanges >= 4, "evolve: most sections change the loops");
        CHECK (claveFlips >= 1, "evolve: clave flips sometimes");
        CHECK (density1 > density0, "evolve: sections build up");
    }

    const char* styles[] = { "HYPNOTIC", "ROLLING", "PEAK-TIME", "HARD" };
    for (int st = 0; st < 4; ++st)
    {
        Params sp = p; sp.style = st; sp.bassStyle = st == Rolling ? BassRolling : BassTresillo;
        std::printf ("=== %s (no melody yet) ===\n", styles[st]);
        printBar (generateBar (0, sp, none));
        if (st == Hard) printBar (generateBar (7, sp, none));
    }

    std::printf (failures ? "\n%d CHECKS FAILED\n" : "\nALL CHECKS PASSED\n", failures);
    return failures ? 1 : 0;
}
