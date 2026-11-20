// CONSTELLATION — by Polaris Sound Lab (Mérida, Yucatán). Design: Dark Funk Durcal. GPL-3.0
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Registry.h"
#include "DSP.h"
#include "Styles.h"

class ConstellationProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    ConstellationProcessor();
    ~ConstellationProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Constellation"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    void updateTrackProperties (const TrackProperties&) override;

    // ── API for editors (message thread) ──
    juce::AudioProcessorValueTreeState apvts;
    static juce::StringArray starParamIds();
    int  getSlot() const noexcept { return slotIndex; }
    bool isSkyMode() const noexcept { return modeParam->load() > 0.5f; }
    void setSkyMode (bool sky);

    // Sets a star parameter on any star (own → host param, other → command queue)
    static void setStarParam (int slot, int param, float realValue);
    void applyOwnParam (int param, float realValue);

    void applyStyle (int style);
    void untangle();
    void storeScene (int i);
    void recallScene (int i);
    bool sceneStored (int i) const { return scenes[(size_t) i].isValid() && scenes[(size_t) i].getNumChildren() > 0; }
    int  effectiveRole (int slot) const;

    // Sky analysis
    std::atomic<float> correlation { 1.0f }, masterL { -100.0f }, masterR { -100.0f };

    static float pairMasking (int a, int b);   // 0..1, used by editor and untangle

private:
    void timerCallback() override;
    void applyMorph (float m);
    void publishParams();
    void updateFilters (bool force);
    void processSky (juce::AudioBuffer<float>&);
    void processStar (juce::AudioBuffer<float>&);
    void computeUnmask (int numSamples);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::array<std::atomic<float>*, cst::kNumStarParams> p {};
    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* morphParam = nullptr;

    int slotIndex = -1;
    juce::String uuid;
    juce::SpinLock trackLock;
    juce::String pendingName; juce::Colour pendingColour; bool trackDirty = false;
    juce::String trackName { "Track" };
    uint32_t trackColour = 0;

    std::array<juce::ValueTree, 4> scenes;
    float lastMorph = -1.0f;
    int   lastSceneTouched = -1;

    // ── DSP state ──
    double sr = 48000.0;
    int    maxBlock = 512;
    juce::AudioBuffer<float> scratch;
    std::vector<float> outGains, outGainsPrev, revGains;
    cst::SpeakerLayout layout;
    int layoutChannels = 0;

    cst::Biquad hpf[2], hpf2[2], lowSh[2], highSh[2], hPeak[2], hShelf[2], air[2];
    cst::Biquad det[cst::kBands];
    cst::Biquad cut[2][cst::kBands];
    float detEnv[cst::kBands] {};
    float cutDb[cst::kBands] {}, cutApplied[cst::kBands] {};
    cst::RoomReverb reverb;
    cst::BinauralSource bin[2];

    juce::SmoothedValue<float> gainSm, muteSm, widthSm, directSm, sendSm;
    float panGL[2] { 1, 0 }, panGR[2] { 0, 1 };        // stereo: [src][out]
    float panTGL[2] { 1, 0 }, panTGR[2] { 0, 1 };
    float lastVals[cst::kNumStarParams] {};
    int   lastRoom = -1; float lastWrap = -1; int lastBin = -1;
    float peakHold = -100.0f;

    // sky meters
    float corrNum = 0, corrDen1 = 1e-9f, corrDen2 = 1e-9f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConstellationProcessor)
};
