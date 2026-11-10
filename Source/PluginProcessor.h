#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PatternEngine.h"

class ForgeDrumProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    ForgeDrumProcessor();
    ~ForgeDrumProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FORGE-DRUM"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    enum Output { OutDrums = 0, OutBass, OutBoth };

    // For the editor
    struct Display { forge::Bar bar; forge::MelodyInfo melody; int step = -1; bool playing = false; };
    Display getDisplay();
    forge::Params readParams() const;

    static juce::String laneId (int lane);

private:
    void parameterChanged (const juce::String&, float) override { paramsDirty.store (true); }
    void addNoteOn  (juce::MidiBuffer&, int sample, int channel, int note, int vel, double offPpq);
    void flushAllNotes (juce::MidiBuffer&, int sample);

    // Parameter pointers cached once so the audio thread never builds strings or searches
    std::array<std::atomic<float>*, forge::NumLanes> laneOnPtr {}, notePtr {};
    enum ScalarParam { P_style, P_follow, P_density, P_accent, P_variation, P_cresc, P_clave, P_tresillo, P_polymeter,
                       P_lenHat, P_lenPerc, P_lenRim, P_lenRide, P_bassStyle, P_bassKey, P_bassOct, P_gate, P_seed,
                       P_output, P_thru, P_swing, P_evolve, P_section, P_count };
    std::array<std::atomic<float>*, P_count> scalarPtr {};

    struct PendingOff { double ppq; int channel; int note; };
    std::vector<PendingOff> pending;

    forge::MelodyTracker tracker;
    forge::Bar currentBar;
    int64_t currentBarIndex = INT64_MIN;
    double sampleRate = 44100.0;
    double expectedPpq = -1.0;
    bool wasPlaying = false;
    std::atomic<bool> paramsDirty { true };

    juce::SpinLock displayLock;
    forge::Bar displayBar;
    forge::MelodyInfo displayMelody;
    std::atomic<int> displayStep { -1 };
    std::atomic<bool> displayPlaying { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForgeDrumProcessor)
};
