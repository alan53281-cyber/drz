// Runs the real FORGE-DRUM processor headlessly with a fake host transport,
// feeds it a melody, and checks the MIDI that comes out. Also renders the UI to a PNG.
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <map>

struct FakeHost : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 128.0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setIsPlaying (playing); p.setPpqPosition (ppq); p.setBpm (bpm);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

struct Event { double ppq; int ch, note, vel; bool on; };
static int failures = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf ("FAIL: %s\n", msg); ++failures; } } while (0)

static std::vector<Event> run (ForgeDrumProcessor& proc, FakeHost& host, double fromPpq, double toPpq, bool sendMelody)
{
    const double sr = 44100.0; const int block = 512;
    const double spp = sr * 60.0 / host.bpm;
    juce::AudioBuffer<float> buf (2, block);
    std::vector<Event> out;
    // melody: steps 1,3,6,9,11,14 of every bar (A, C, E, G, A, D)
    const int mSteps[] = { 1, 3, 6, 9, 11, 14 }, mNotes[] = { 57, 60, 64, 55, 57, 62 };
    host.ppq = fromPpq;
    while (host.ppq < toPpq)
    {
        juce::MidiBuffer midi;
        const double end = host.ppq + block / spp;
        if (sendMelody)
            for (double bar = std::floor (host.ppq / 4.0); bar * 4.0 < end; ++bar)
                for (int k = 0; k < 6; ++k)
                {
                    const double t = bar * 4.0 + mSteps[k] * 0.25;
                    if (t >= host.ppq && t < end) midi.addEvent (juce::MidiMessage::noteOn (1, mNotes[k], (juce::uint8) 100), (int) ((t - host.ppq) * spp));
                }
        proc.processBlock (buf, midi);
        for (const auto m : midi)
        {
            const auto msg = m.getMessage();
            if (msg.isNoteOnOrOff())
                out.push_back ({ host.ppq + m.samplePosition / spp, msg.getChannel(), msg.getNoteNumber(), msg.getVelocity(), msg.isNoteOn() });
        }
        host.ppq = end;
    }
    return out;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    ForgeDrumProcessor proc;
    FakeHost host;
    proc.setPlayHead (&host);
    proc.prepareToPlay (44100.0, 512);
    proc.apvts.getParameter ("output")->setValueNotifyingHost (1.0f);   // drums + bass on ch 2

    // 8 bars with melody
    auto ev = run (proc, host, 0.0, 32.0, true);

    int kicks = 0; std::map<std::pair<int,int>, int> open; int maxOpen = 0, bassOn = 0, bassOnKick = 0;
    for (auto& e : ev)
    {
        auto key = std::make_pair (e.ch, e.note);
        if (e.on)
        {
            open[key]++; maxOpen = std::max (maxOpen, open[key]);
            if (e.ch == 1 && e.note == 36) { ++kicks; if (std::getenv ("SHOWKICKS")) std::printf ("kick %.3f\n", e.ppq); CHECK (std::abs (e.ppq - std::round (e.ppq)) < 0.002 || std::abs (std::fmod (e.ppq, 4.0) - 3.5) < 0.01, "kick on the beat"); }
            if (e.ch == 2) { ++bassOn; if (std::abs (e.ppq - std::round (e.ppq)) < 0.002) ++bassOnKick; }
        }
        else open[key]--;
    }
    std::printf ("8 bars: %zu events, %d kicks, %d bass notes\n", ev.size(), kicks, bassOn);
    CHECK (kicks >= 32, "32 kicks in 8 bars");
    CHECK (bassOn > 16, "bass line playing on ch 2");
    CHECK (bassOnKick == 0, "bass never on the kick");
    CHECK (maxOpen <= 1, "no overlapping same notes");

    // Stop transport: everything must be closed
    host.playing = false;
    {
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
        proc.processBlock (buf, midi);
        for (const auto m : midi) if (m.getMessage().isNoteOff()) open[{ m.getMessage().getChannel(), m.getMessage().getNoteNumber() }]--;
    }
    int stuck = 0; for (auto& [k, v] : open) stuck += v;
    CHECK (stuck == 0, "no stuck notes after stop");

    // Loop jump: play bars 4..6, jump back to 4 - again no stuck notes
    host.playing = true;
    open.clear();
    auto a = run (proc, host, 16.0, 23.9, true);
    auto b = run (proc, host, 16.0, 20.0, true);
    a.insert (a.end(), b.begin(), b.end());
    host.playing = false;
    { juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi; proc.processBlock (buf, midi);
      for (const auto m : midi) if (m.getMessage().isNoteOnOrOff()) a.push_back ({ 0, m.getMessage().getChannel(), m.getMessage().getNoteNumber(), 0, m.getMessage().isNoteOn() }); }
    for (auto& e : a) open[{ e.ch, e.note }] += e.on ? 1 : -1;
    stuck = 0; for (auto& [k, v] : open) stuck += v;
    CHECK (stuck == 0, "no stuck notes after loop jump");

    // Drums-only output must not send bass, bass-only must not send drums
    proc.apvts.getParameter ("output")->setValueNotifyingHost (0.0f);
    host.playing = true;
    auto d = run (proc, host, 0.0, 8.0, true);
    bool anyBass = false; for (auto& e : d) anyBass |= (e.note < 36 && e.on);
    CHECK (! anyBass, "drums-only sends no bass");
    proc.apvts.getParameter ("output")->setValueNotifyingHost (0.5f);
    auto bl = run (proc, host, 0.0, 8.0, true);
    bool anyDrum = false; for (auto& e : bl) anyDrum |= (e.on && std::abs (e.ppq - std::round (e.ppq)) < 0.002);   // drums always hit the beat, bass never does
    CHECK (! anyDrum, "bass-only sends no drums");

    // Render the UI
    host.playing = false;
    { juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi; proc.processBlock (buf, midi); }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    if (auto* fe = dynamic_cast<ForgeDrumEditor*> (ed.get()))
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        auto img = fe->createComponentSnapshot (fe->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile ("ui.png"));
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("UI rendered to %s\n", f.getFullPathName().toRawUTF8());
    }

    std::printf (failures ? "%d CHECKS FAILED\n" : "ALL PROCESSOR CHECKS PASSED\n", failures);
    return failures ? 1 : 0;
}
