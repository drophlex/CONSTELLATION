// CONSTELLATION — editor
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Lang.h"

namespace cst
{
struct Theme
{
    static juce::Colour bg()     { return juce::Colour (0xff05060f); }
    static juce::Colour panel()  { return juce::Colour (0xff0b0f24); }
    static juce::Colour line()   { return juce::Colour (0xff1c2550); }
    static juce::Colour cyan()   { return juce::Colour (0xff58e1ff); }
    static juce::Colour pink()   { return juce::Colour (0xffff4fd8); }
    static juce::Colour gold()   { return juce::Colour (0xffffd166); }
    static juce::Colour text()   { return juce::Colour (0xffe8ecff); }
    static juce::Colour dim()    { return juce::Colour (0xff7c86b8); }
    static juce::Colour danger() { return juce::Colour (0xffff5a4e); }
};

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f, juce::Font::bold)); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::Font (juce::FontOptions (juce::jmin (13.0f, h * 0.5f), juce::Font::bold)); }
};

class SkyView : public juce::Component
{
public:
    explicit SkyView (ConstellationProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;

    enum View { v3D = 0, vTop, vFront };
    void setView (int v);
    int  view = v3D;
    int  selected = -1;
    std::function<void (int)> onSelect;
    float time = 0;

private:
    struct P { float x, y, depth, scale; };
    P project (float wx, float wy, float wz) const;
    void fit();
    float listenerZ() const;
    float fitS = 300.0f, fitX = 0.0f, fitY = 0.0f;
    static float toWZ (float z) { return -1.0f + 2.0f * z; }
    int hitTest (juce::Point<float>) const;
    void drawBackground (juce::Graphics&);
    void drawGrid (juce::Graphics&);
    void drawStar (juce::Graphics&, int slot, const P& pp, bool fresh);

    ConstellationProcessor& proc;
    float yaw = -0.55f, pitch = 0.42f, yawTarget = -0.55f, pitchTarget = 0.42f;
    int dragging = -1, hover = -1;
    bool orbiting = false;
    juce::Point<float> lastMouse;
    std::vector<std::pair<int, P>> drawn;
    std::vector<juce::Rectangle<int>> labelRects;
    struct Bg { float x, y, s, ph; };
    std::vector<Bg> bgStars;
};

class BandMeter : public juce::Component
{
public:
    int slot = -1;
    void paint (juce::Graphics&) override;
};

class MasterMeter : public juce::Component
{
public:
    explicit MasterMeter (ConstellationProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    ConstellationProcessor& proc;
};
} // namespace cst

class ConstellationEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ConstellationEditor (ConstellationProcessor&);
    ~ConstellationEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshTexts();
    void refreshPanel();
    void select (int slot);
    void knob (juce::Slider&, juce::Label&, int param, double lo, double hi, double def);

    ConstellationProcessor& proc;
    cst::LookAndFeel lnf;
    cst::SkyView sky;
    cst::BandMeter bands;
    cst::MasterMeter meter;

    juce::TextButton modeStar, modeSky, view3D, viewTop, viewFront, phones, langBtn, helpBtn, donate;
    juce::ComboBox room, roleBox, styleBox;
    juce::TextButton applyStyle, untangleBtn, monoBtn, muteBtn, soloBtn, storeBtn;
    juce::TextButton sceneBtn[4];
    juce::Slider wrapSlider, morphSlider, prioSlider;
    juce::Label wrapLabel, morphLabel, prioLabel, nameLabel, hintLabel, scenesLabel, autoLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAtt;

    struct K { juce::Slider s; juce::Label l; int param; };
    std::array<K, 9> knobs;
    std::unique_ptr<juce::Component> help;
    bool updating = false, showHelp = false, storeArmed = false;
    int selected = -1, lastLang = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConstellationEditor)
};
