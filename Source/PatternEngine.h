// FORGE-DRUM pattern engine
// Pure C++ (no JUCE) so it can be tested on its own.
//
// Generates one bar (16 x 16th steps) at a time for 9 lanes. Every decision is a
// deterministic hash of (seed, lane, step, phrase), so the same settings always give
// the same groove - two plugin instances with the same settings stay in sync.
//
// The maths:
//   Tresillo        3+3+2 Euclidean cell (E(3,8)) on the PERC lane and the bass line
//   Son clave       3-2 / 2-3 one-bar clave, or the extended two-bar son clave on RIM
//   Polymeter       HAT / PERC / RIM / RIDE each loop over their own length, so they
//                   shift against the 4/4 kick ("loop shifting")
//   Knots           steps where the clave meets the tresillo or ride, or where two
//                   lane cycles restart together -> accented, bass jumps an octave
//   Crescendos      velocity rises over N bars and through each lane cycle
//                   (ghost notes low, accents high = "the accent trick")
//   Evolve          the song is split into sections (16/32/64 bars). Each section can give
//                   the loops new lengths and start points (so the drift changes speed and
//                   direction), flip the clave, build density across the section, bring
//                   perc and ride in one after the other, and walk the variation so it
//                   never repeats exactly. Kick stays the backbone.
//   Melody follow   percussion, hats and bass lock onto the rhythm the melody played
//                   in the bar before; the bass follows the melody's lowest note per beat
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace forge
{

enum Lane { Kick = 0, Clap, Snare, CHat, OHat, Perc, Rim, Ride, Bass, NumLanes };
constexpr int kSteps = 16;

inline const char* laneName (int lane)
{
    static const char* names[] = { "KICK", "CLAP", "SNARE", "C-HAT", "O-HAT", "PERC", "RIM", "RIDE", "BASS" };
    return names[lane];
}

// Default notes: General MIDI drum map (works with Waveform's Drum Sampler and most kits)
inline int defaultNote (int lane)
{
    static const int notes[] = { 36, 39, 38, 42, 46, 43, 37, 51, 0 };
    return notes[lane];
}

enum Style      { Hypnotic = 0, Rolling, PeakTime, Hard };
enum ClaveMode  { ClaveOff = 0, Clave32, Clave23, ClaveExtended };
enum BassStyle  { BassTresillo = 0, BassRolling, BassClave, BassFollow, BassOff };

struct Hit
{
    uint8_t vel  = 0;     // 0 = no hit
    int     note = -1;    // absolute pitch (bass only); drums use the lane's note
    float   len  = 1.0f;  // length in 16th steps
    bool on() const { return vel > 0; }
};

struct Bar
{
    std::array<std::array<Hit, kSteps>, NumLanes> hits {};
    std::array<bool, kSteps> knot {};
    int64_t index = 0;
};

struct Params
{
    int   style      = Hypnotic;
    float follow     = 0.6f;   // 0..1  how strongly drums lock to the melody
    float density    = 0.5f;   // 0..1  extra ghost notes
    float accent     = 0.6f;   // 0..1  accent vs ghost contrast
    float variation  = 0.3f;   // 0..1  how much each 4-bar block differs
    int   crescBars  = 4;      // 0 = off, else 2/4/8/16
    int   clave      = Clave32;
    bool  tresillo   = true;
    bool  polymeter  = true;
    int   lenHat = 16, lenPerc = 12, lenRim = 16, lenRide = 10;
    int   bassStyle  = BassTresillo;
    int   bassRoot   = 9;      // fallback pitch class (9 = A) when no melody heard yet
    int   bassOctave = 1;      // 1 -> notes 24..35 (C1..B1)
    float bassGate   = 0.8f;
    uint32_t seed    = 1;
    float evolve     = 0.5f;   // 0 = off. How much the groove changes from section to section
    int   sectionBars = 32;    // length of an Evolve section in bars
    std::array<bool, NumLanes> laneOn { true, true, true, true, true, true, true, true, true };
};

//==============================================================================
// What the melody did. Filled by MelodyTracker, read by the engine.
struct MelodyInfo
{
    std::array<float, kSteps> onset {};             // 0..1 strength per step
    std::array<int, 4> beatRoot { -1, -1, -1, -1 }; // lowest pitch per beat
    int  root  = -1;
    bool valid = false;
};

class MelodyTracker
{
public:
    // ppq = position in quarter notes where the melody note started
    void noteOn (double ppq, int pitch, int velocity)
    {
        const auto bar = (int64_t) std::floor (ppq / 4.0);
        barChanged (bar);

        const int step = std::clamp ((int) std::lround ((ppq - (double) bar * 4.0) / 0.25), 0, kSteps - 1);
        cur[(size_t) step] = std::max (cur[(size_t) step], (float) velocity / 127.0f);

        const int beat = step / 4;
        if (curBeatLow[(size_t) beat] < 0 || pitch < curBeatLow[(size_t) beat])
            curBeatLow[(size_t) beat] = pitch;
        ++count;
    }

    // Call at every bar line (and on any jump in the timeline)
    void barChanged (int64_t bar)
    {
        if (bar == captureBar) return;
        commit();
        captureBar = bar;
    }

    void reset()  { info = {}; clearCurrent(); captureBar = INT64_MIN; }
    const MelodyInfo& get() const { return info; }

private:
    void commit()
    {
        if (count > 0)
        {
            for (size_t i = 0; i < kSteps; ++i)
                info.onset[i] = info.valid ? 0.65f * cur[i] + 0.35f * info.onset[i] : cur[i];

            int carry = info.beatRoot[3];
            int lowest = -1;
            for (size_t b = 0; b < 4; ++b)
            {
                if (curBeatLow[b] >= 0) carry = curBeatLow[b];   // held note carries to next beat
                info.beatRoot[b] = carry;
                if (curBeatLow[b] >= 0 && (lowest < 0 || curBeatLow[b] < lowest)) lowest = curBeatLow[b];
            }
            for (auto& r : info.beatRoot) if (r < 0) r = lowest;
            info.root  = lowest;
            info.valid = true;
        }
        else if (info.valid)
        {
            for (auto& o : info.onset) o *= 0.5f;   // melody rested: let it fade out
        }
        clearCurrent();
    }

    void clearCurrent() { cur.fill (0.0f); curBeatLow.fill (-1); count = 0; }

    MelodyInfo info;
    std::array<float, kSteps> cur {};
    std::array<int, 4> curBeatLow { -1, -1, -1, -1 };
    int count = 0;
    int64_t captureBar = INT64_MIN;
};

//==============================================================================
namespace detail
{
    inline uint32_t mix (uint32_t h)
    {
        h ^= h >> 16; h *= 0x7feb352dU;
        h ^= h >> 15; h *= 0x846ca68bU;
        h ^= h >> 16; return h;
    }
    inline float rnd (uint32_t seed, uint32_t lane, uint32_t step, uint32_t salt)
    {
        const uint32_t h = mix (seed * 0x9E3779B1U ^ mix (lane * 0x85EBCA77U ^ mix (step * 0xC2B2AE3DU ^ salt)));
        return (float) (h >> 8) * (1.0f / 16777216.0f);
    }
    // Euclidean rhythm: k hits spread as evenly as possible over n steps.
    // euclid(3,8) = x..x..x. = the tresillo.
    inline bool euclid (int k, int n, int pos, int rot)
    {
        if (n <= 0 || k <= 0) return false;
        pos = ((pos + rot) % n + n) % n;
        return (pos * k) % n < k;
    }
    inline int posMod (int64_t s, int n) { return (int) (((s % n) + n) % n); }
    inline uint8_t clampVel (float v) { return (uint8_t) std::clamp ((int) std::lround (v), 1, 127); }

    inline bool claveHit (int mode, int64_t bar, int step)
    {
        switch (mode)
        {
            case Clave32: { static const bool c[16] = {1,0,0,1, 0,0,1,0, 0,0,1,0, 1,0,0,0}; return c[step]; }
            case Clave23: { static const bool c[16] = {0,0,1,0, 1,0,0,0, 1,0,0,1, 0,0,1,0}; return c[step]; }
            case ClaveExtended:
                // Two-bar son clave: bar A = the "3 side" stretched over the bar (a bar-long
                // tresillo: steps 0,6,12), bar B = the "2 side" on beats 2 and 3.
                if ((bar & 1) == 0) return step == 0 || step == 6 || step == 12;
                return step == 4 || step == 8;
            default: return false;
        }
    }
}

//==============================================================================
inline Bar generateBar (int64_t barIndex, const Params& p, const MelodyInfo& mel)
{
    using namespace detail;
    Bar bar;
    bar.index = barIndex;

    const uint32_t seed   = p.seed;
    const uint32_t phrase = (uint32_t) ((barIndex / 4) % 4);       // 4-bar blocks, repeating every 16 bars
    const bool phraseEnd  = posMod (barIndex, 8) == 7;              // last bar of an 8-bar phrase
    const float f         = mel.valid ? p.follow : 0.0f;
    const float acc       = p.accent;
    const float accVel    = 100.0f + 27.0f * acc;
    const float ghostVel  = 90.0f - 55.0f * acc;
    const float midVel    = 0.5f * (accVel + ghostVel);

    //--- EVOLVE: where are we in the song?
    const bool  evolveOn   = p.evolve > 0.001f;
    const int   secBars    = std::max (4, p.sectionBars);
    const int64_t sec      = evolveOn ? (barIndex >= 0 ? barIndex / secBars : (barIndex - secBars + 1) / secBars) : 0;
    const float secT       = evolveOn ? (float) posMod (barIndex, secBars) / (float) (secBars - 1) : 1.0f;   // 0..1 through the section
    const uint32_t secSalt = (uint32_t) (sec & 0xFFFF);
    auto secRnd = [&] (uint32_t what) { return rnd (seed, what, secSalt, 0xE70Eu); };   // one roll per section

    // density builds through each section
    const float density = std::clamp (p.density + (evolveOn ? p.evolve * (0.4f * secT - 0.2f) : 0.0f), 0.0f, 1.0f);

    // the clave flips 3-2 <-> 2-3 (extended clave swaps its sides) on some sections
    int clave = p.clave;
    int64_t claveBar = barIndex;
    if (evolveOn && sec > 0 && secRnd (1) < p.evolve * 0.7f)
    {
        if (clave == Clave32) clave = Clave23;
        else if (clave == Clave23) clave = Clave32;
        else if (clave == ClaveExtended) claveBar = barIndex + 1;
    }

    auto decide = [&] (int lane, int step)
    {
        const float base = rnd (seed, (uint32_t) lane, (uint32_t) step, 0xBEEFu);
        // with Evolve on, the variation also changes every section, so it never comes round the same
        const float var  = rnd (seed, (uint32_t) lane, (uint32_t) step, 0x1000u + phrase + 0x100u * secSalt);
        return base + (var - base) * std::max (p.variation, evolveOn ? 0.35f * p.evolve : 0.0f);
    };

    // Crescendo across N bars: starts lower, peaks at the end of the cycle
    auto cresc = [&] (int step)
    {
        if (p.crescBars <= 0) return 1.0f;
        const float t = (float) (posMod (barIndex, p.crescBars) * kSteps + step) / (float) (p.crescBars * kSteps - 1);
        return 1.0f - acc * 0.45f * (1.0f - t);
    };
    // Accent trick inside each polymetric loop: leans into the end of the cycle
    auto ramp = [] (int pos, int len) { return len > 1 ? 0.85f + 0.15f * (float) pos / (float) (len - 1) : 1.0f; };

    auto onsetAt = [&] (int i) { return mel.valid ? mel.onset[(size_t) i] : 0.0f; };
    auto set = [&] (int lane, int i, float vel, float len = 1.0f, int note = -1)
    {
        auto& h = bar.hits[(size_t) lane][(size_t) i];
        h.vel = clampVel (vel); h.len = len; h.note = note;
    };
    auto isOn = [&] (int lane, int i) { return bar.hits[(size_t) lane][(size_t) i].on(); };

    // Evolve: each section can pick new loop lengths. Loops restart together at the top of
    // the section, then drift apart at the new speeds.
    auto evolveLen = [&] (int base, uint32_t what, std::initializer_list<int> pool)
    {
        if (! evolveOn || sec == 0 || secRnd (what) >= p.evolve) return base;
        const auto idx = mix (seed ^ (what * 0x2545F491u) ^ (secSalt * 0x9E3779B9u)) % (uint32_t) pool.size();
        return *(pool.begin() + idx);
    };
    const int64_t s0     = (barIndex - (evolveOn ? sec * secBars : 0)) * kSteps;   // steps since the section began
    const int lenHat     = p.polymeter ? std::max (3, evolveLen (p.lenHat,  11, { 16, 12, 14, 10, 15 }))      : 16;
    const int lenPerc    = p.polymeter ? std::max (3, evolveLen (p.lenPerc, 12, { 12, 10, 14, 9, 11, 7, 16 })) : 8;
    const int lenRim     = p.polymeter ? std::max (3, evolveLen (p.lenRim,  13, { 16, 10, 12, 14 }))          : 16;
    const int lenRide    = p.polymeter ? std::max (3, evolveLen (p.lenRide, 14, { 10, 14, 7, 12, 11, 9 }))    : 16;
    const uint32_t rotSalt = evolveOn ? secSalt * 0x632BE5ABu : 0u;
    const int rotPerc    = (int) (mix (seed ^ 0x51u ^ rotSalt) % (uint32_t) lenPerc);
    const int rotRim     = (int) (mix (seed ^ 0x77u ^ rotSalt) % (uint32_t) lenRim);
    const int rotRide    = evolveOn && sec > 0 ? (int) (mix (seed ^ 0x99u ^ rotSalt) % (uint32_t) lenRide) : 2;

    //--- KICK: four to the floor, full velocity every time so the transient stays consistent
    for (int i = 0; i < kSteps; i += 4) set (Kick, i, 127.0f);
    if (p.style == Hard && phraseEnd && decide (Kick, 14) < 0.5f + 0.5f * density)
        set (Kick, 14, 112.0f);                                     // pickup into the next phrase

    //--- CLAP: backbeat, plus ghosts that answer the melody
    for (int i : { 4, 12 })
        set (Clap, i, (p.style == Hypnotic ? 96.0f : 112.0f) * (0.85f + 0.15f * cresc (i)));
    for (int i : { 7, 15 })
    {
        const bool styleGhost = (p.style == Rolling || p.style == Hard) && i == 15 && decide (Clap, i) < density * 0.6f;
        if (styleGhost || f * onsetAt (i) > 0.5f) set (Clap, i, ghostVel + 10.0f);
    }

    //--- SNARE: crescendo roll at the end of each 8-bar phrase
    if (phraseEnd && density > 0.2f && (p.crescBars > 0 || p.style == Hard))
    {
        const int start = p.style == Hard ? 8 : (p.style == PeakTime ? 10 : 12);
        for (int i = start; i < kSteps; ++i)
        {
            const float t = (float) (i - start) / (float) (kSteps - 1 - start);
            set (Snare, i, 45.0f + 82.0f * t);
        }
    }

    //--- PERC: tresillo family (Euclidean 3-in-8, stretched to the lane length)
    int percPos[kSteps];
    for (int i = 0; i < kSteps; ++i)
    {
        const int pos = posMod (s0 + i, lenPerc);
        percPos[i] = pos;
        const int k = p.tresillo ? std::max (2, (int) std::lround (lenPerc * 3.0 / 8.0))
                                 : std::max (2, (int) std::lround (lenPerc * (0.2 + 0.4 * density)));
        const bool hit   = euclid (k, lenPerc, pos, rotPerc);
        const bool first = posMod (pos + rotPerc, lenPerc) == 0;    // the "1" of the 3+3+2
        if (hit)
        {
            // melody rests here and follow is high -> occasionally drop a non-accent hit
            if (! first && f > 0.6f && onsetAt (i) < 0.05f && decide (Perc, i) < (f - 0.6f) * 0.75f) continue;
            set (Perc, i, (first ? accVel : midVel) * cresc (i) * ramp (pos, lenPerc));
        }
        else if (f * onsetAt (i) > 0.45f)
            set (Perc, i, (midVel + 12.0f * onsetAt (i)) * cresc (i));
        else if (decide (Perc, i) < density * 0.22f)
            set (Perc, i, ghostVel * cresc (i));
    }

    //--- RIM: son clave (anchored to the bar - it's the skeleton), or a Euclidean loop
    int rimFirst = -1;
    for (int i = 0; i < kSteps; ++i)
    {
        bool hit;
        if (clave != ClaveOff) hit = claveHit (clave, claveBar, i);
        else
        {
            const int pos = posMod (s0 + i, lenRim);
            hit = euclid (std::max (2, (int) std::lround (lenRim * 5.0 / 16.0)), lenRim, pos, rotRim);
        }
        if (! hit) continue;
        const bool first = rimFirst < 0;
        if (first) rimFirst = i;
        set (Rim, i, ((first ? accVel : midVel) + 15.0f * f * onsetAt (i)) * cresc (i));
    }

    //--- RIDE: quarter-offbeat Euclidean loop on its own length (drifts against the bar)
    const bool rideActive = p.style == PeakTime || p.style == Hard || density > 0.55f;
    int ridePos[kSteps];
    for (int i = 0; i < kSteps; ++i)
    {
        const int pos = posMod (s0 + i, lenRide);
        ridePos[i] = pos;
        if (! rideActive) continue;
        const int k = std::max (2, (int) std::lround (lenRide / 4.0));
        if (euclid (k, lenRide, pos, rotRide))
            set (Ride, i, (pos == 0 ? accVel : midVel - 5.0f) * cresc (i) * ramp (pos, lenRide));
    }

    //--- C-HAT: 16ths over its own loop length, accents on the loop's offbeats
    int hatPos[kSteps];
    for (int i = 0; i < kSteps; ++i)
    {
        const int pos = posMod (s0 + i, lenHat);
        hatPos[i] = pos;
        const int sub = pos % 4;
        const bool openTakesIt = (p.style == PeakTime) && (i % 4 == 2) && p.laneOn[OHat];
        if (openTakesIt) continue;

        float vel = 0.0f;
        if (sub == 2)                                              vel = accVel - 8.0f;
        else if (p.style == Hard && sub == 0)                      vel = midVel;
        else if (decide (CHat, i) < density + (p.style == Hard ? 0.5f : 0.25f)) vel = ghostVel;

        const float o = onsetAt (i);
        if (f * o > 0.35f) vel = std::max (vel, midVel) + 15.0f * o;           // lock to the melody
        else if (vel > 0.0f && sub != 2 && f > 0.5f && o < 0.05f && decide (CHat, i + 32) < (f - 0.5f) * 0.5f)
            vel = 0.0f;                                                        // thin out where melody rests

        if (vel > 0.0f) set (CHat, i, vel * cresc (i) * ramp (pos, lenHat));
    }

    //--- O-HAT: the offbeat anchor (bar-locked)
    if (density > 0.15f)
        for (int i : { 2, 6, 10, 14 })
        {
            const float base = (p.style == Hypnotic) ? midVel : accVel - 4.0f;
            set (OHat, i, base * cresc (i), 2.0f);
        }

    //--- KNOTS: where the structures tie together
    for (int i = 0; i < kSteps; ++i)
    {
        const bool rim  = isOn (Rim, i);
        const bool tied = rim && (isOn (Perc, i) || isOn (Ride, i));
        const bool cyclesMeet = percPos[i] == 0 && (ridePos[i] == 0 || hatPos[i] == 0) && i != 0;
        bar.knot[(size_t) i] = tied || cyclesMeet;
        if (! bar.knot[(size_t) i]) continue;
        for (int lane : { Perc, Rim, Ride, CHat })
        {
            if (lane == CHat && i % 4 == 0) continue;   // keep hats off the kick's transient
            auto& h = bar.hits[(size_t) lane][(size_t) i];
            if (h.on()) h.vel = clampVel (std::max ((float) h.vel, accVel) + 10.0f * acc);
        }
    }

    //--- BASS: rhythm from the tresillo/clave/melody, pitch from the melody's lowest note per beat
    auto rootFor = [&] (int i)
    {
        int pc = p.bassRoot;
        if (mel.valid && mel.beatRoot[(size_t) (i / 4)] >= 0) pc = mel.beatRoot[(size_t) (i / 4)];
        return ((pc % 12) + 12) % 12 + 12 * (p.bassOctave + 1);
    };

    std::array<float, kSteps> bassVel {};
    for (int i = 0; i < kSteps; ++i)
    {
        if (i % 4 == 0) continue;                    // never on the kick: keeps the low end clean for sidechain
        const int half = i % 8;
        float v = 0.0f;
        switch (p.bassStyle)
        {
            case BassTresillo:
                if (half == 3 || half == 6) v = (half == 3 ? accVel : midVel);
                else if ((half == 7 || half == 1) && decide (Bass, i) < density - 0.35f) v = ghostVel;
                break;
            case BassRolling:
                if (i % 4 == 2) v = accVel;
                else if (decide (Bass, i) < density + 0.35f) v = ghostVel + 10.0f;
                break;
            case BassClave:
                if (detail::claveHit (clave == ClaveOff ? Clave32 : clave, claveBar, i)) v = accVel;
                else if (i % 4 == 2 && decide (Bass, i) < density) v = ghostVel;
                break;
            case BassFollow:
                if (onsetAt (i) > 0.25f) v = midVel + 20.0f * onsetAt (i);
                break;
            default: break;
        }
        if (p.bassStyle != BassOff && p.bassStyle != BassFollow && v <= 0.0f && f * onsetAt (i) > 0.55f)
            v = midVel;                              // melody pulls extra bass notes in
        bassVel[(size_t) i] = v;
    }
    if (p.bassStyle == BassFollow && std::all_of (bassVel.begin(), bassVel.end(), [] (float v) { return v <= 0.0f; }))
        for (int i : { 3, 6, 11, 14 }) bassVel[(size_t) i] = midVel;          // nothing heard yet: tresillo

    for (int i = 0; i < kSteps; ++i)
    {
        if (bassVel[(size_t) i] <= 0.0f) continue;
        int next = i + 1;
        while (next < kSteps && bassVel[(size_t) next] <= 0.0f && next % 4 != 0) ++next;
        const float len = std::min (4.0f, (float) (next - i)) * p.bassGate;
        int note = rootFor (i);
        if (bar.knot[(size_t) i] && decide (Bass, i + 64) < 0.35f + 0.6f * acc) note += 12;   // knot: octave jump
        set (Bass, i, bassVel[(size_t) i] * cresc (i), std::max (0.25f, len), note);
    }

    //--- Evolve layering: on some "build" sections perc drops out and comes back in a quarter of
    // the way through and ride half way, so the groove fills up in turn. Applied after the knots
    // so the bass doesn't change.
    if (evolveOn && sec > 0 && secRnd (2) < p.evolve * 0.6f)
    {
        if (secT < 0.25f) bar.hits[Perc].fill (Hit {});
        if (secT < 0.5f)  bar.hits[Ride].fill (Hit {});
    }

    //--- Mutes are applied last so the knots (and so the bass) don't change when you mute a lane
    for (int l = 0; l < NumLanes; ++l)
        if (! p.laneOn[(size_t) l]) bar.hits[(size_t) l].fill (Hit {});

    return bar;
}

} // namespace forge
