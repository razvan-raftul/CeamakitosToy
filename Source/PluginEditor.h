#pragma once

#include "PluginProcessor.h"

class PlayPanel;
class BuildPanel;
class KeyboardMode;
class SettingsPanel;

class ToyEditor : public juce::AudioProcessorEditor
{
public:
    explicit ToyEditor (ToyProcessor&);
    ~ToyEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void focusTab (int index);
    void toggleKeyboard();

    ToyProcessor& proc;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    PlayPanel* play = nullptr;
    BuildPanel* build = nullptr;
    SettingsPanel* settings = nullptr;
    std::unique_ptr<KeyboardMode> keys;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToyEditor)
};
