# FORGE-DRUM

Techno drum and bass-line pattern engine. It listens to your melody and writes drums,
percussion and a bass line around it. It makes no sound itself — it plays whatever
drum kit you load in Waveform's drum sampler (and whatever synth you put on the bass),
so the sound is only as good as the kit.

## Build (same as MasterForge / Forge-Stepper)

1. Create a new private GitHub repo called `Forge-Drum`.
2. Upload everything in this folder (keep the `.github` folder — it's hidden on Windows,
   so turn on "Hidden items" in Explorer before dragging).
3. Go to **Actions** → the build runs by itself (about 10–15 min the first time).
4. Download **FORGE-DRUM-VST3-Windows**, unzip, and put `FORGE-DRUM.vst3` in
   `C:\Program Files\Common Files\VST3`.
5. In Waveform: Settings → Plugins → scan.

## Setting it up in Waveform

**Drums track**
1. New track. Copy your melody MIDI clip onto it (Ctrl+drag).
2. Plugin chain: **FORGE-DRUM** first, then Waveform's drum sampler (Micro Drum Sampler) with your kit.
3. FORGE-DRUM: Output = **Drums**. Leave MELODY THRU off (otherwise the melody triggers pads).
4. If a drum lands on the wrong pad, change its note number next to the lane name
   (defaults are the General MIDI drum map: kick 36, clap 39, snare 38, closed hat 42,
   open hat 46, perc 43, rim 37, ride 51).

**Bass track**
1. Another track with the same melody clip copied on.
2. Plugin chain: **FORGE-DRUM** then your bass synth (4OSC, an 808 sampler, etc.).
3. Output = **Bass line**. Use the same Style / Seed / Maths settings as the drums
   instance so the knots line up (save a preset and load it in both).

The pattern follows the melody from the bar before, so give it one bar to lock on.

**Evolve** works from the song position, so the groove at bar 97 is different from bar 1.
Each section can give the loops new lengths and start points (the loop-length knobs are the
lengths it starts from), flip the clave between 3-2 and 2-3, build the density towards the end
of the section, drop the perc and ride out and bring them back in turn, and move the variation on
so nothing comes round exactly the same. The kick never drops out - breakdowns are yours to
arrange in Waveform.

## Controls

| Control | What it does |
|---|---|
| Style | Hypnotic, Rolling, Peak-time, Hard |
| Melody follow | How hard hats, perc and bass lock to the melody's rhythm |
| Density | Extra ghost notes |
| Accent | Gap between ghost notes and accents (the accent trick) |
| Variation | How much each 4-bar block changes (repeats every 16 bars) |
| Swing | 0 = straight, 100% = triplet shuffle |
| Tresillo | Perc lane uses the 3+3+2 cell |
| Polymeter | Hat / Perc / Rim / Ride loop on their own lengths and drift against the kick |
| Son clave | 3-2, 2-3, or the extended two-bar clave on the rim |
| Crescendo | Velocity builds over 2–16 bars; snare roll on the last bar of each 8 |
| Knots (◆) | Where clave meets tresillo/ride or loops restart together: accent + bass octave jump |
| Bass rhythm | Tresillo, Rolling, Clave, Follow melody, Off. Bass never plays on the kick |
| Evolve | How much the groove changes from section to section (0 = a fixed loop) |
| Evolve every | Section length: 16, 32 or 64 bars. The top of the pattern shows which section and bar you're in |
| Groove seed / New groove | Different variations of the same maths |
| Lane buttons | Click to mute a lane (the bass and knots don't change) |

## Getting the hard-techno master shape

FORGE-DRUM keeps every kick at velocity 127 so the transient stays the same every hit,
and keeps the bass off the kick. The rest happens in the mix:

- Kick track peaking around −6 dB.
- Rumble: send the kick to a 100% wet dark reverb, low-pass at 150 Hz, saturate,
  then sidechain it hard to the kick.
- Sidechain the bass to the kick too.
- Light limiting on the master (MasterForge) so kick transients poke through the top.
