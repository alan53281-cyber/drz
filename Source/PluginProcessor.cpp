#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace forge;

juce::String ForgeDrumProcessor::laneId (int lane)
{
    static const char* ids[] = { "kick", "clap", "snare", "chat", "ohat", "perc", "rim", "ride", "bass" };
    return ids[lane];
}

juce::AudioProcessorValueTreeState::ParameterLayout ForgeDrumProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + "%"; });

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style", StringArray { "Hypnotic", "Rolling", "Peak-time", "Hard" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "output", 1 }, "Output",
                                                   StringArray { "Drums", "Bass line", "Drums + bass (ch 2)" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "follow", 1 },    "Melody follow", NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "density", 1 },   "Density",       NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "accent", 1 },    "Accent",        NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "variation", 1 }, "Variation",     NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "evolve", 1 },    "Evolve",        NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "section", 1 }, "Evolve section", StringArray { "16 bars", "32 bars", "64 bars" }, 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "swing", 1 },     "Swing",         NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 },      "Bass gate",     NormalisableRange<float> (0.1f, 1.0f), 0.8f, pct));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "cresc", 1 }, "Crescendo", StringArray { "Off", "2 bars", "4 bars", "8 bars", "16 bars" }, 2));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "clave", 1 }, "Clave", StringArray { "Off", "Son 3-2", "Son 2-3", "Extended 3-2" }, 1));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "tresillo", 1 },  "Tresillo", true));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "polymeter", 1 }, "Polymeter", true));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { "lenHat", 1 },  "Hat loop",  3, 32, 16));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { "lenPerc", 1 }, "Perc loop", 3, 32, 12));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { "lenRim", 1 },  "Rim loop",  3, 32, 16));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { "lenRide", 1 }, "Ride loop", 3, 32, 10));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "bassStyle", 1 }, "Bass style",
                                                   StringArray { "Tresillo", "Rolling", "Clave", "Follow melody", "Off" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "bassKey", 1 }, "Bass key (no melody)",
                                                   StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 9));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { "bassOct", 1 }, "Bass octave", 0, 3, 1));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { "seed", 1 },    "Groove seed", 1, 999, 1));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "thru", 1 },    "Melody thru", false));

    for (int lane = 0; lane < NumLanes; ++lane)
    {
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "on_" + laneId (lane), 1 }, String (laneName (lane)) + " on", true));
        if (lane != Bass)
            l.add (std::make_unique<AudioParameterInt> (ParameterID { "note_" + laneId (lane), 1 }, String (laneName (lane)) + " note",
                                                        0, 127, defaultNote (lane)));
    }
    return l;
}

ForgeDrumProcessor::ForgeDrumProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FORGE_DRUM", createLayout())
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.addParameterListener (rp->getParameterID(), this);
    static const char* scalarIds[] = { "style", "follow", "density", "accent", "variation", "cresc", "clave", "tresillo",
                                       "polymeter", "lenHat", "lenPerc", "lenRim", "lenRide", "bassStyle", "bassKey",
                                       "bassOct", "gate", "seed", "output", "thru", "swing", "evolve", "section" };
    for (int i = 0; i < P_count; ++i)
    {
        scalarPtr[(size_t) i] = apvts.getRawParameterValue (scalarIds[i]);
        jassert (scalarPtr[(size_t) i] != nullptr);
    }
    for (int lane = 0; lane < NumLanes; ++lane)
    {
        laneOnPtr[(size_t) lane] = apvts.getRawParameterValue ("on_" + laneId (lane));
        notePtr[(size_t) lane]   = lane == Bass ? nullptr : apvts.getRawParameterValue ("note_" + laneId (lane));
    }
    pending.reserve (256);
}

ForgeDrumProcessor::~ForgeDrumProcessor()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.removeParameterListener (rp->getParameterID(), this);
}

Params ForgeDrumProcessor::readParams() const
{
    auto v  = [this] (int id) { return scalarPtr[(size_t) id]->load(); };
    auto iv = [&v] (int id) { return (int) std::lround (v (id)); };
    static const int crescTable[] = { 0, 2, 4, 8, 16 };

    Params p;
    p.style      = iv (P_style);
    p.follow     = v (P_follow);
    p.density    = v (P_density);
    p.accent     = v (P_accent);
    p.variation  = v (P_variation);
    p.crescBars  = crescTable[juce::jlimit (0, 4, iv (P_cresc))];
    p.clave      = iv (P_clave);
    p.tresillo   = v (P_tresillo) > 0.5f;
    p.polymeter  = v (P_polymeter) > 0.5f;
    p.lenHat     = iv (P_lenHat);
    p.lenPerc    = iv (P_lenPerc);
    p.lenRim     = iv (P_lenRim);
    p.lenRide    = iv (P_lenRide);
    p.bassStyle  = iv (P_bassStyle);
    p.bassRoot   = iv (P_bassKey);
    p.bassOctave = iv (P_bassOct);
    p.bassGate   = v (P_gate);
    p.seed       = (uint32_t) iv (P_seed);
    p.evolve     = v (P_evolve);
    p.sectionBars = 16 << juce::jlimit (0, 2, iv (P_section));
    for (int lane = 0; lane < NumLanes; ++lane)
        p.laneOn[(size_t) lane] = laneOnPtr[(size_t) lane]->load() > 0.5f;
    return p;
}

void ForgeDrumProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    pending.clear();
    currentBarIndex = INT64_MIN;
    wasPlaying = false;
}

void ForgeDrumProcessor::addNoteOn (juce::MidiBuffer& out, int sample, int channel, int note, int vel, double offPpq)
{
    // Same note still sounding? Close it first so the sampler retriggers cleanly.
    for (auto it = pending.begin(); it != pending.end();)
    {
        if (it->channel == channel && it->note == note)
        {
            out.addEvent (juce::MidiMessage::noteOff (channel, note), sample);
            it = pending.erase (it);
        }
        else ++it;
    }
    out.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) vel), sample);
    if (pending.size() < 256) pending.push_back ({ offPpq, channel, note });
}

void ForgeDrumProcessor::flushAllNotes (juce::MidiBuffer& out, int sample)
{
    for (auto& p : pending) out.addEvent (juce::MidiMessage::noteOff (p.channel, p.note), sample);
    pending.clear();
}

void ForgeDrumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();   // FORGE-DRUM makes no sound itself - it plays your drum sampler / synth

    const int numSamples = buffer.getNumSamples();
    const auto output    = (int) std::lround (scalarPtr[P_output]->load());
    const bool thru      = scalarPtr[P_thru]->load() > 0.5f;
    const float swing    = scalarPtr[P_swing]->load();
    juce::MidiBuffer out;

    // ---- Transport
    bool playing = false;
    double ppq0 = 0.0, bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (pos->getIsPlaying() && pos->getPpqPosition().hasValue())
            {
                playing = true;
                ppq0 = *pos->getPpqPosition();
                if (pos->getBpm().hasValue() && *pos->getBpm() > 1.0) bpm = *pos->getBpm();
            }

    const double samplesPerPpq = sampleRate * 60.0 / bpm;

    if (! playing)
    {
        if (wasPlaying) { flushAllNotes (out, 0); tracker.barChanged (INT64_MIN + 1); }
        wasPlaying = false;
        currentBarIndex = INT64_MIN;
        displayStep.store (-1);
        displayPlaying.store (false);
        if (thru) for (const auto m : midi) out.addEvent (m.getMessage(), m.samplePosition);
        midi.swapWith (out);
        return;
    }

    const double ppq1 = ppq0 + numSamples / samplesPerPpq;

    // Loop or jump in the timeline: close everything and rebuild the bar
    if (wasPlaying && std::abs (ppq0 - expectedPpq) > 0.01)
    {
        flushAllNotes (out, 0);
        currentBarIndex = INT64_MIN;
    }
    wasPlaying  = true;
    expectedPpq = ppq1;

    // ---- Listen to the melody first, so a bar line inside this block uses the full bar before it
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isNoteOn())
            tracker.noteOn (ppq0 + m.samplePosition / samplesPerPpq, msg.getNoteNumber(), msg.getVelocity());
        if (thru) out.addEvent (msg, m.samplePosition);
    }

    auto toSample = [&] (double ppq) { return juce::jlimit (0, numSamples - 1, (int) ((ppq - ppq0) * samplesPerPpq)); };

    // ---- Note-offs due in this block
    for (auto it = pending.begin(); it != pending.end();)
    {
        if (it->ppq < ppq1) { out.addEvent (juce::MidiMessage::noteOff (it->channel, it->note), toSample (it->ppq)); it = pending.erase (it); }
        else ++it;
    }

    // ---- Steps that land in this block
    constexpr double stepLen = 0.25;
    const double swingDelay  = swing * stepLen / 3.0;   // 100% = triplet shuffle
    const int bassChannel    = output == OutBoth ? 2 : 1;
    int lastStep = displayStep.load();

    for (int64_t n = (int64_t) std::floor (ppq0 / stepLen) - 1;; ++n)
    {
        const double t = (double) n * stepLen + ((n & 1) ? swingDelay : 0.0);
        if (t >= ppq1) break;
        if (t < ppq0) continue;

        const int64_t barIdx = (n >= 0 ? n : n - (kSteps - 1)) / kSteps;
        const int step = (int) (n - barIdx * kSteps);

        if (barIdx != currentBarIndex || paramsDirty.exchange (false))
        {
            if (barIdx != currentBarIndex) tracker.barChanged (barIdx);
            currentBarIndex = barIdx;
            currentBar = generateBar (barIdx, readParams(), tracker.get());
            const juce::SpinLock::ScopedTryLockType lock (displayLock);
            if (lock.isLocked()) { displayBar = currentBar; displayMelody = tracker.get(); }
        }

        const int sample = toSample (t);
        for (int lane = 0; lane < NumLanes; ++lane)
        {
            const auto& h = currentBar.hits[(size_t) lane][(size_t) step];
            if (! h.on()) continue;
            if (lane == Bass)
            {
                if (output == OutDrums) continue;
                addNoteOn (out, sample, bassChannel, juce::jlimit (0, 127, h.note), h.vel, t + h.len * stepLen);
            }
            else
            {
                if (output == OutBass) continue;
                const int note = (int) std::lround (notePtr[(size_t) lane]->load());
                addNoteOn (out, sample, 1, note, h.vel, t + std::min (h.len, 2.0f) * stepLen * 0.9);
            }
        }
        lastStep = step;
    }

    displayStep.store (lastStep);
    displayPlaying.store (true);
    midi.swapWith (out);
}

ForgeDrumProcessor::Display ForgeDrumProcessor::getDisplay()
{
    Display d;
    d.playing = displayPlaying.load();
    d.step    = displayStep.load();
    {
        const juce::SpinLock::ScopedLockType lock (displayLock);
        d.bar = displayBar;
        d.melody = displayMelody;
    }
    if (! d.playing)   // stopped: preview what the current settings give you
        d.bar = generateBar (d.bar.index, readParams(), d.melody);
    return d;
}

void ForgeDrumProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void ForgeDrumProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ForgeDrumProcessor::createEditor() { return new ForgeDrumEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ForgeDrumProcessor(); }
