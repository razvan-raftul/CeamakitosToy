#include "PluginEditor.h"

using namespace juce;

ToyEditor::ToyEditor (ToyProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    title.setText ("Ceamakito's toy", dontSendNotification);
    title.setFont (FontOptions (22.0f, Font::bold));
    title.setJustificationType (Justification::centred);
    addAndMakeVisible (title);

    chooseButton.setButtonText (ro ("Alege folderul cu instrumente (Alt 3)"));
    chooseButton.onClick = [this] { chooseFolder(); };
    addAndMakeVisible (chooseButton);

    folderBox.setReadOnly (true);
    folderBox.setCaretVisible (true);
    folderBox.setTitle (ro ("Folderul cu instrumente"));
    addAndMakeVisible (folderBox);

    instrumentLabel.setText ("Instrument", dontSendNotification);
    instrumentLabel.attachToComponent (&instrumentBox, true);
    instrumentBox.setTitle ("Instrument");
    instrumentBox.onChange = [this]
    {
        const int i = instrumentBox.getSelectedItemIndex();
        if (i >= 0 && i != proc.getCurrentIndex())
            proc.selectInstrument (i);
    };
    addAndMakeVisible (instrumentBox);

    prevButton.setButtonText (ro ("Instrumentul anterior (Alt 1)"));
    prevButton.onClick = [this] { announceNext = true; proc.stepInstrument (-1); };
    addAndMakeVisible (prevButton);

    nextButton.setButtonText (ro ("Instrumentul următor (Alt 2)"));
    nextButton.onClick = [this] { announceNext = true; proc.stepInstrument (1); };
    addAndMakeVisible (nextButton);

    statusBox.setReadOnly (true);
    statusBox.setCaretVisible (true);
    statusBox.setTitle (ro ("Ce e încărcat"));
    addAndMakeVisible (statusBox);

    volumeLabel.setText ("Volum general (dB)", dontSendNotification);
    volumeLabel.attachToComponent (&volumeSlider, true);
    volumeSlider.setTitle ("Volum general");
    volumeSlider.setSliderStyle (Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (Slider::TextBoxRight, false, 70, 24);
    addAndMakeVisible (volumeSlider);
    volumeAtt = std::make_unique<SA> (proc.apvts, "volum", volumeSlider);

    sensLabel.setText (ro ("Cât contează cât de tare apeși"), dontSendNotification);
    sensLabel.attachToComponent (&sensSlider, true);
    sensSlider.setTitle (ro ("Cât contează cât de tare apeși clapa"));
    sensSlider.setSliderStyle (Slider::LinearHorizontal);
    sensSlider.setTextBoxStyle (Slider::TextBoxRight, false, 70, 24);
    addAndMakeVisible (sensSlider);
    sensAtt = std::make_unique<SA> (proc.apvts, "sensibilitate", sensSlider);

    releaseLabel.setText ("La ridicarea clapei", dontSendNotification);
    releaseLabel.attachToComponent (&releaseBox, true);
    releaseBox.setTitle ("La ridicarea clapei");
    releaseBox.addItemList ({ ro ("Sunetul continuă până la capăt"), ro ("Sunetul se oprește") }, 1);
    addAndMakeVisible (releaseBox);
    releaseAtt = std::make_unique<CA> (proc.apvts, "ridicare", releaseBox);

    chooseButton.setExplicitFocusOrder (1);
    folderBox.setExplicitFocusOrder (2);
    instrumentBox.setExplicitFocusOrder (3);
    prevButton.setExplicitFocusOrder (4);
    nextButton.setExplicitFocusOrder (5);
    statusBox.setExplicitFocusOrder (6);
    volumeSlider.setExplicitFocusOrder (7);
    sensSlider.setExplicitFocusOrder (8);
    releaseBox.setExplicitFocusOrder (9);

    setTitle ("Ceamakito's toy");
    setWantsKeyboardFocus (false);
    setSize (560, 360);

    proc.changes.addChangeListener (this);
    refresh();

    SafePointer<ToyEditor> safe (this);
    Timer::callAfterDelay (400, [safe]
    {
        if (safe != nullptr && safe->isShowing())
            safe->instrumentBox.grabKeyboardFocus();
    });
}

ToyEditor::~ToyEditor()
{
    proc.changes.removeChangeListener (this);
}

void ToyEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff20242c));
}

void ToyEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    title.setBounds (r.removeFromTop (34));
    r.removeFromTop (6);
    chooseButton.setBounds (r.removeFromTop (30));
    r.removeFromTop (6);
    folderBox.setBounds (r.removeFromTop (26));
    r.removeFromTop (10);

    const int labelW = 210;
    auto row = [&r, labelW] (int h) { auto x = r.removeFromTop (h); r.removeFromTop (8); return x.withTrimmedLeft (labelW); };

    instrumentBox.setBounds (row (28));
    auto buttons = r.removeFromTop (30);
    r.removeFromTop (8);
    prevButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (2, 0));
    nextButton.setBounds (buttons.reduced (2, 0));
    statusBox.setBounds (r.removeFromTop (26));
    r.removeFromTop (10);
    volumeSlider.setBounds (row (26));
    sensSlider.setBounds (row (26));
    releaseBox.setBounds (row (28));
}

bool ToyEditor::keyPressed (const KeyPress& k)
{
    if (k.getModifiers().isAltDown() && ! k.getModifiers().isCtrlDown())
    {
        const auto c = k.getKeyCode();
        if (c == '1') { announceNext = true; proc.stepInstrument (-1); return true; }
        if (c == '2') { announceNext = true; proc.stepInstrument (1); return true; }
        if (c == '3') { chooseFolder(); return true; }
    }
    return false;
}

void ToyEditor::changeListenerCallback (ChangeBroadcaster*)
{
    refresh();
}

void ToyEditor::refresh()
{
    const auto names = proc.getInstrumentNames();
    if (names != shownNames)
    {
        shownNames = names;
        instrumentBox.clear (dontSendNotification);
        instrumentBox.addItemList (names, 1);
    }
    instrumentBox.setSelectedItemIndex (proc.getCurrentIndex(), dontSendNotification);

    folderBox.setText (proc.getRoot().getFullPathName(), dontSendNotification);

    const auto st = proc.getStatus();
    if (st != lastStatus)
    {
        lastStatus = st;
        statusBox.setText (st, dontSendNotification);
        if (announceNext)
        {
            announceNext = false;
            AccessibilityHandler::postAnnouncement (st, AccessibilityHandler::AnnouncementPriority::high);
        }
    }
}

void ToyEditor::chooseFolder()
{
    auto start = proc.getRoot();
    chooser = std::make_unique<FileChooser> (ro ("Alege folderul cu instrumente"), start);
    SafePointer<ToyEditor> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                          [safe] (const FileChooser& fc)
                          {
                              if (safe == nullptr)
                                  return;
                              auto f = fc.getResult();
                              if (f.isDirectory())
                              {
                                  safe->announceNext = true;
                                  safe->proc.setRoot (f);
                              }
                              safe->instrumentBox.grabKeyboardFocus();
                          });
}
