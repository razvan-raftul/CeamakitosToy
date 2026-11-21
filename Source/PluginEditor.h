#pragma once

#include "PluginProcessor.h"

class ToyEditor : public juce::AudioProcessorEditor,
                  private juce::ChangeListener
{
public:
    explicit ToyEditor (ToyProcessor&);
    ~ToyEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refresh();
    void chooseFolder();

    ToyProcessor& proc;

    juce::Label title;
    juce::TextButton chooseButton;
    juce::TextEditor folderBox;
    juce::Label instrumentLabel;
    juce::ComboBox instrumentBox;
    juce::TextButton prevButton, nextButton;
    juce::TextEditor statusBox;
    juce::Label volumeLabel, sensLabel, releaseLabel;
    juce::Slider volumeSlider, sensSlider;
    juce::ComboBox releaseBox;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SA> volumeAtt, sensAtt;
    std::unique_ptr<CA> releaseAtt;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::StringArray shownNames;
    juce::String lastStatus;
    bool announceNext = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToyEditor)
};
