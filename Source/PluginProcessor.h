#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <memory>
#include <vector>

inline juce::String ro (const char* s) { return juce::String::fromUTF8 (s); }

struct Zone
{
    int lo = 0, hi = 127, root = 60, vlo = 0, vhi = 127, group = 0;
    double sampleRate = 44100.0;
    juce::AudioBuffer<float> data;
};

struct Instrument
{
    juce::String name;
    std::vector<Zone> zones;
    int id = 0;
};

class ToyProcessor : public juce::AudioProcessor,
                     private juce::AudioProcessorValueTreeState::Listener,
                     private juce::Thread,
                     private juce::AsyncUpdater
{
public:
    static constexpr int maxInstruments = 200;

    ToyProcessor();
    ~ToyProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // folderul cu instrumente si lista lor
    juce::File getRoot() const;
    void setRoot (const juce::File& folder);      // din fereastra pluginului
    juce::StringArray getInstrumentNames() const;
    int getInstrumentCount() const;
    int getCurrentIndex() const;
    void selectInstrument (int index);
    void stepInstrument (int delta);
    juce::String getStatus() const;
    juce::String instrumentName (int index) const;

    juce::ChangeBroadcaster changes;

    // pentru fereastra de construit instrumente
    struct MapLine { int lo = 36, hi = 36, root = 36, vlo = 0, vhi = 127, grp = 0; juce::String file; };
    static std::vector<MapLine> readMap (const juce::File& dir);
    static bool writeMap (const juce::File& dir, const std::vector<MapLine>& lines);
    static juce::String noteName (int note);
    juce::File instrumentFolder (int index) const;
    void reloadCurrent() { requestLoad(); }
    void rescanFolders() { rescan(); }

    juce::MidiKeyboardState keyboardState;
    std::atomic<int> lastNote { -1 };
    std::atomic<int> noteCount { 0 };

private:
    // datele de aici trebuie sa existe inainte de apvts (le foloseste textul parametrului Instrument)
    mutable juce::CriticalSection listLock;
    juce::Array<juce::File> folders;
    juce::File root;
    juce::String status;

public:
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout (ToyProcessor&);
    void parameterChanged (const juce::String& id, float value) override;
    void run() override;
    void handleAsyncUpdate() override;

    static juce::File globalFile();
    static juce::File defaultRoot();
    juce::File readGlobalRoot() const;
    void rescan();
    void requestLoad();
    std::shared_ptr<Instrument> build (const juce::File& dir);
    void setStatus (const juce::String& s);

    juce::AudioFormatManager formats;
    std::atomic<bool> pendingLoad { false };
    int nextId = 0;

    juce::SpinLock instLock;
    std::shared_ptr<Instrument> current;   // scris de firul de incarcare
    std::shared_ptr<Instrument> playing;   // folosit de firul audio

    struct Voice
    {
        bool active = false, fading = false;
        int zone = 0, note = 0, group = 0;
        double pos = 0.0, step = 1.0;
        float gain = 1.0f, level = 1.0f;
        juce::uint32 age = 0;
    };
    static constexpr int numVoices = 64;
    Voice voices[numVoices];
    juce::uint32 ageCounter = 0, roundRobin = 0;
    double hostRate = 44100.0;
    float fadeStep = 0.003f;

    std::atomic<float>* volumeParam = nullptr;
    std::atomic<float>* sensParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;

    void noteOn (int note, int velocity);
    void noteOff (int note);
    void fadeAll();
    void render (juce::AudioBuffer<float>& out, int start, int end);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToyProcessor)
};
