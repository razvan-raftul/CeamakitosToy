#pragma once

#include "PluginProcessor.h"

class PlayPanel;
class BuildPanel;
class KeyboardMode;
class SettingsPanel;

// Rândul cu file, ca un TabControl din Windows: o singură oprire la Tab, săgeată stânga/dreapta schimbă fila
class ToyTabs : public juce::TabbedComponent
{
public:
    ToyTabs() : juce::TabbedComponent (juce::TabbedButtonBar::TabsAtTop) {}
    void currentTabChanged (int newIndex, const juce::String& name) override;
    juce::TabBarButton* createTabButton (const juce::String& name, int index) override;
    void updateTabFocus();
    void moveTab (int delta, bool absolute = false);
    std::function<void (int)> onTabChanged;
};

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
    ToyTabs tabs;
    PlayPanel* play = nullptr;
    BuildPanel* build = nullptr;
    SettingsPanel* settings = nullptr;
    std::unique_ptr<KeyboardMode> keys;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToyEditor)
};
