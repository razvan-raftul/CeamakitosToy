#include "PluginEditor.h"
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif
#include "Updater.h"
#include "Speech.h"

using namespace juce;

static void announce (const String& s) { speak (s); }

static void setupLabel (Label& l, const String& text, Component& target)
{
    l.setText (text, dontSendNotification);
    l.attachToComponent (&target, true);
    target.setTitle (text);
}

//==============================================================================
// Fila "Cântă" (Control 1)
class PlayPanel : public Component, private ChangeListener
{
public:
    explicit PlayPanel (ToyProcessor& p) : proc (p)
    {

        setupLabel (instrumentLabel, "Instrument", instrumentBox);
        instrumentBox.onChange = [this]
        {
            const int i = instrumentBox.getSelectedItemIndex();
            if (i >= 0 && i != proc.getCurrentIndex())
                proc.selectInstrument (i);
        };
        addAndMakeVisible (instrumentBox);

        prevButton.setButtonText (ro ("Instrumentul anterior (Alt 1)"));
        prevButton.onClick = [this] { step (-1); };
        addAndMakeVisible (prevButton);

        nextButton.setButtonText (ro ("Instrumentul următor (Alt 2)"));
        nextButton.onClick = [this] { step (1); };
        addAndMakeVisible (nextButton);

        statusBox.setReadOnly (true);
        statusBox.setCaretVisible (true);
        statusBox.setTitle (ro ("Ce e încărcat"));
        addAndMakeVisible (statusBox);

        int order = 1;
        for (Component* c : std::initializer_list<Component*> { &instrumentBox, &prevButton, &nextButton, &statusBox })
            c->setExplicitFocusOrder (order++);

        proc.changes.addChangeListener (this);
        refresh();
    }

    ~PlayPanel() override { proc.changes.removeChangeListener (this); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        instrumentBox.setBounds (r.removeFromTop (28).withTrimmedLeft (250));
        r.removeFromTop (8);
        auto buttons = r.removeFromTop (30);
        r.removeFromTop (8);
        prevButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (2, 0));
        nextButton.setBounds (buttons.reduced (2, 0));
        statusBox.setBounds (r.removeFromTop (26));
    }

    void step (int d) { announceNext = true; proc.stepInstrument (d); }
    void announceAfterChange() { announceNext = true; }

    ComboBox instrumentBox;

private:
    void changeListenerCallback (ChangeBroadcaster*) override { refresh(); }

    void refresh()
    {
        const auto names = proc.getInstrumentNames();
        if (names != shownNames)
        {
            shownNames = names;
            instrumentBox.clear (dontSendNotification);
            instrumentBox.addItemList (names, 1);
        }
        if (instrumentBox.getSelectedItemIndex() != proc.getCurrentIndex())
            instrumentBox.setSelectedItemIndex (proc.getCurrentIndex(), dontSendNotification);

        const auto st = proc.getStatus();
        if (st != lastStatus)
        {
            lastStatus = st;
            statusBox.setText (st, dontSendNotification);
            if (announceNext)
            {
                announceNext = false;
                announce (st);
            }
        }
    }

    ToyProcessor& proc;
    TextButton prevButton, nextButton;
    TextEditor statusBox;
    Label instrumentLabel;
    StringArray shownNames;
    String lastStatus;
    bool announceNext = false;
};

//==============================================================================
// Fila "Setări" (Control 3, ultima)
class SettingsPanel : public Component, private ChangeListener
{
public:
    SettingsPanel (ToyProcessor& p, std::function<void()> chosen) : proc (p), onFolderChosen (std::move (chosen))
    {

        chooseButton.setButtonText (ro ("Alege folderul cu instrumente (Alt 3)"));
        chooseButton.onClick = [this] { chooseFolder(); };
        addAndMakeVisible (chooseButton);

        folderBox.setReadOnly (true);
        folderBox.setCaretVisible (true);
        folderBox.setTitle (ro ("Folderul cu instrumente"));
        addAndMakeVisible (folderBox);

        setupLabel (volumeLabel, "Volum general (dB)", volumeSlider);
        volumeSlider.setSliderStyle (Slider::LinearHorizontal);
        volumeSlider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
        volumeSlider.setTextValueSuffix (" dB");
        addAndMakeVisible (volumeSlider);
        volumeAtt = std::make_unique<SA> (proc.apvts, "volum", volumeSlider);

        setupLabel (sensLabel, ro ("Cât contează cât de tare apeși clapa"), sensSlider);
        sensSlider.setSliderStyle (Slider::LinearHorizontal);
        sensSlider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (sensSlider);
        sensAtt = std::make_unique<SA> (proc.apvts, "sensibilitate", sensSlider);

        setupLabel (releaseLabel, "La ridicarea clapei", releaseBox);
        releaseBox.addItemList ({ ro ("Sunetul continuă până la capăt"), ro ("Sunetul se oprește") }, 1);
        addAndMakeVisible (releaseBox);
        releaseAtt = std::make_unique<CA> (proc.apvts, "ridicare", releaseBox);

        versionBox.setReadOnly (true);
        versionBox.setCaretVisible (true);
        versionBox.setTitle ("Versiunea");
        versionBox.setText (ro ("Versiunea ") + TOY_VERSION_STRING, dontSendNotification);
        addAndMakeVisible (versionBox);

        const bool standalone = p.wrapperType == AudioProcessor::wrapperType_Standalone;
        updateButton.setButtonText (ro ("Caută actualizări acum"));
        updateButton.onClick = [] { checkForUpdatesNow(); };
        addChildComponent (updateButton);
        updateButton.setVisible (standalone);

        int order = 1;
        for (Component* c : std::initializer_list<Component*> { &chooseButton, &folderBox, &volumeSlider, &sensSlider,
                                                               &releaseBox, &versionBox, &updateButton })
            c->setExplicitFocusOrder (order++);

        proc.changes.addChangeListener (this);
        refresh();
    }

    ~SettingsPanel() override { proc.changes.removeChangeListener (this); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        chooseButton.setBounds (r.removeFromTop (30));
        r.removeFromTop (6);
        folderBox.setBounds (r.removeFromTop (26));
        r.removeFromTop (12);
        auto row = [&r] (int h) { auto x = r.removeFromTop (h); r.removeFromTop (8); return x.withTrimmedLeft (250); };
        volumeSlider.setBounds (row (26));
        sensSlider.setBounds (row (26));
        releaseBox.setBounds (row (28));
        versionBox.setBounds (r.removeFromTop (26));
        r.removeFromTop (8);
        updateButton.setBounds (r.removeFromTop (30));
    }

    void chooseFolder()
    {
        chooser = std::make_unique<FileChooser> (ro ("Alege folderul cu instrumente"), proc.getRoot());
        SafePointer<SettingsPanel> safe (this);
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                              [safe] (const FileChooser& fc)
                              {
                                  if (safe == nullptr)
                                      return;
                                  auto f = fc.getResult();
                                  if (f.isDirectory())
                                  {
                                      if (safe->onFolderChosen)
                                          safe->onFolderChosen();
                                      safe->proc.setRoot (f);
                                  }
                                  safe->chooseButton.grabKeyboardFocus();
                              });
    }

    TextButton chooseButton;

private:
    void changeListenerCallback (ChangeBroadcaster*) override { refresh(); }

    void refresh()
    {
        const auto folder = proc.getRoot().getFullPathName();
        if (folderBox.getText() != folder)
            folderBox.setText (folder, dontSendNotification);
    }

    ToyProcessor& proc;
    std::function<void()> onFolderChosen;
    TextButton updateButton;
    TextEditor folderBox, versionBox;
    Label volumeLabel, sensLabel, releaseLabel;
    Slider volumeSlider, sensSlider;
    ComboBox releaseBox;

    using SA = AudioProcessorValueTreeState::SliderAttachment;
    using CA = AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SA> volumeAtt, sensAtt;
    std::unique_ptr<CA> releaseAtt;
    std::unique_ptr<FileChooser> chooser;
};

//==============================================================================
// Fila "Construiește instrumente"
class BuildPanel : public Component, private ChangeListener, private ListBoxModel, private Timer
{
public:
    explicit BuildPanel (ToyProcessor& p) : proc (p)
    {
        setupLabel (newLabel, "Numele instrumentului nou", newName);
        addAndMakeVisible (newName);
        newName.onReturnKey = [this] { createInstrument(); };
        createButton.setButtonText (ro ("Creează instrumentul"));
        createButton.onClick = [this] { createInstrument(); };
        addAndMakeVisible (createButton);

        setupLabel (instLabel, ro ("Instrumentul pe care îl construiești"), instBox);
        instBox.onChange = [this]
        {
            const int i = instBox.getSelectedItemIndex();
            if (i >= 0 && i != proc.getCurrentIndex())
                proc.selectInstrument (i);
            loadEntries();
        };
        addAndMakeVisible (instBox);

        addButton.setButtonText (ro ("Adaugă sunete (WAV)"));
        addButton.onClick = [this] { addSounds(); };
        addAndMakeVisible (addButton);

        list.setModel (this);
        list.setTitle ("Sunetele instrumentului");
        list.setRowHeight (24);
        addAndMakeVisible (list);

        listenButton.setButtonText (ro ("Ascultă sunetul"));
        listenButton.onClick = [this] { listen(); };
        addAndMakeVisible (listenButton);

        learnButton.setButtonText (ro ("Pune sunetul pe clapa pe care o apeși"));
        learnButton.onClick = [this] { startLearning(); };
        addAndMakeVisible (learnButton);

        removeButton.setButtonText (ro ("Scoate sunetul din instrument"));
        removeButton.onClick = [this] { removeSound(); };
        addAndMakeVisible (removeButton);

        StringArray notes;
        for (int i = 0; i < 128; ++i)
            notes.add (ToyProcessor::noteName (i));
        StringArray levels;
        for (int i = 0; i < 128; ++i)
            levels.add (String (i));
        StringArray groups { ro ("Fără grupă") };
        for (int i = 1; i <= 16; ++i)
            groups.add ("Grupa " + String (i));

        setupCombo (loBox, loLabel, "Clapa de jos", notes);
        setupCombo (hiBox, hiLabel, "Clapa de sus", notes);
        setupCombo (rootBox, rootLabel, ro ("Clapa pe care sună neschimbat"), notes);
        setupCombo (grpBox, grpLabel, ro ("Grupa (se taie între ele)"), groups);
        setupCombo (vloBox, vloLabel, ro ("Cel mai încet la care sună"), levels);
        setupCombo (vhiBox, vhiLabel, ro ("Cel mai tare la care sună"), levels);

        int order = 1;
        for (Component* c : std::initializer_list<Component*> { &newName, &createButton, &instBox, &addButton, &list, &listenButton,
                                                               &learnButton, &removeButton, &loBox, &hiBox, &rootBox, &grpBox, &vloBox, &vhiBox })
            c->setExplicitFocusOrder (order++);

        proc.changes.addChangeListener (this);
        refreshNames();
        loadEntries();
    }

    ~BuildPanel() override { proc.changes.removeChangeListener (this); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        const int labelW = 250;
        auto row = [&r, labelW] (int h) { auto x = r.removeFromTop (h); r.removeFromTop (6); return x.withTrimmedLeft (labelW); };

        auto top = row (26);
        createButton.setBounds (top.removeFromRight (180));
        newName.setBounds (top.withTrimmedRight (6));
        instBox.setBounds (row (26));
        addButton.setBounds (r.removeFromTop (28));
        r.removeFromTop (6);
        list.setBounds (r.removeFromTop (130));
        r.removeFromTop (6);
        auto b = r.removeFromTop (28);
        r.removeFromTop (6);
        const int w = b.getWidth() / 3;
        listenButton.setBounds (b.removeFromLeft (w).reduced (2, 0));
        learnButton.setBounds (b.removeFromLeft (w).reduced (2, 0));
        removeButton.setBounds (b.reduced (2, 0));
        for (auto* c : { &loBox, &hiBox, &rootBox, &grpBox, &vloBox, &vhiBox })
            c->setBounds (row (24));
    }

private:
    void setupCombo (ComboBox& box, Label& label, const String& text, const StringArray& items)
    {
        setupLabel (label, text, box);
        box.addItemList (items, 1);
        box.onChange = [this] { fieldsChanged(); };
        addAndMakeVisible (box);
    }

    // --- lista de sunete
    int getNumRows() override { return (int) entries.size(); }

    String getNameForRow (int row) override
    {
        if (! isPositiveAndBelow (row, (int) entries.size()))
            return {};
        const auto& e = entries[(size_t) row];
        String s = e.file + ", ";
        if (e.lo == e.hi)
            s << ToyProcessor::noteName (e.lo);
        else
            s << ro ("de la ") << ToyProcessor::noteName (e.lo) << ro (" până la ") << ToyProcessor::noteName (e.hi)
              << ro (", sună neschimbat pe ") << ToyProcessor::noteName (e.root);
        if (e.grp > 0)
            s << ", grupa " << e.grp;
        if (e.vlo > 0 || e.vhi < 127)
            s << ro (", când apeși între ") << e.vlo << " și " << e.vhi;
        return s;
    }

    void paintListBoxItem (int row, Graphics& g, int w, int h, bool selected) override
    {
        if (selected)
            g.fillAll (Colour (0xff3a5a8a));
        g.setColour (Colours::white);
        g.setFont (FontOptions (15.0f));
        g.drawText (getNameForRow (row), 6, 0, w - 12, h, Justification::centredLeft, true);
    }

    void selectedRowsChanged (int) override { showFields(); }

    // --- date
    File currentDir() const { return proc.instrumentFolder (proc.getCurrentIndex()); }

    void refreshNames()
    {
        const auto names = proc.getInstrumentNames();
        if (names != shownNames)
        {
            shownNames = names;
            instBox.clear (dontSendNotification);
            instBox.addItemList (names, 1);
        }
        if (instBox.getSelectedItemIndex() != proc.getCurrentIndex())
            instBox.setSelectedItemIndex (proc.getCurrentIndex(), dontSendNotification);
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        refreshNames();
        if (currentDir() != shownDir)
            loadEntries();
    }

    void loadEntries()
    {
        shownDir = currentDir();
        entries = shownDir == File() ? std::vector<ToyProcessor::MapLine>() : ToyProcessor::readMap (shownDir);
        list.updateContent();
        list.repaint();
        if (! entries.empty() && list.getSelectedRow() < 0)
            list.selectRow (0);
        showFields();
    }

    void save (int selectRow)
    {
        if (shownDir == File())
            return;
        ToyProcessor::writeMap (shownDir, entries);
        list.updateContent();
        list.repaint();
        if (isPositiveAndBelow (selectRow, (int) entries.size()))
            list.selectRow (selectRow);
        proc.reloadCurrent();
    }

    void showFields()
    {
        const int r = list.getSelectedRow();
        const bool ok = isPositiveAndBelow (r, (int) entries.size());
        for (auto* c : { &loBox, &hiBox, &rootBox, &grpBox, &vloBox, &vhiBox })
            c->setEnabled (ok);
        listenButton.setEnabled (ok);
        learnButton.setEnabled (ok);
        removeButton.setEnabled (ok);
        if (! ok)
            return;
        const auto& e = entries[(size_t) r];
        loBox.setSelectedItemIndex (e.lo, dontSendNotification);
        hiBox.setSelectedItemIndex (e.hi, dontSendNotification);
        rootBox.setSelectedItemIndex (e.root, dontSendNotification);
        grpBox.setSelectedItemIndex (jlimit (0, 16, e.grp), dontSendNotification);
        vloBox.setSelectedItemIndex (e.vlo, dontSendNotification);
        vhiBox.setSelectedItemIndex (e.vhi, dontSendNotification);
    }

    void fieldsChanged()
    {
        const int r = list.getSelectedRow();
        if (! isPositiveAndBelow (r, (int) entries.size()))
            return;
        auto& e = entries[(size_t) r];
        e.lo   = jmax (0, loBox.getSelectedItemIndex());
        e.hi   = jmax (e.lo, hiBox.getSelectedItemIndex());
        e.root = jmax (0, rootBox.getSelectedItemIndex());
        e.grp  = jmax (0, grpBox.getSelectedItemIndex());
        e.vlo  = jmax (0, vloBox.getSelectedItemIndex());
        e.vhi  = jmax (e.vlo, vhiBox.getSelectedItemIndex());
        save (r);
        showFields();
    }

    // --- actiuni
    void createInstrument()
    {
        auto name = File::createLegalFileName (newName.getText().trim());
        if (name.isEmpty())
        {
            announce (ro ("Scrie întâi numele instrumentului."));
            newName.grabKeyboardFocus();
            return;
        }
        auto root = proc.getRoot();
        if (root == File())
        {
            root = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Ceamakito's toy").getChildFile ("Instrumente");
            root.createDirectory();
            proc.setRoot (root);
        }
        auto dir = root.getChildFile (name);
        if (dir.isDirectory())
        {
            announce (ro ("Există deja un instrument cu numele ") + name);
            return;
        }
        dir.createDirectory();
        ToyProcessor::writeMap (dir, {});
        proc.rescanFolders();
        const int idx = proc.getInstrumentNames().indexOf (name);
        if (idx >= 0)
            proc.selectInstrument (idx);
        newName.clear();
        loadEntries();
        refreshNames();
        announce (ro ("Am creat instrumentul ") + name + ro (". Acum adaugă sunete."));
        addButton.grabKeyboardFocus();
    }

    void addSounds()
    {
        if (shownDir == File())
        {
            announce (ro ("Alege sau creează întâi un instrument."));
            return;
        }
        chooser = std::make_unique<FileChooser> (ro ("Alege sunetele"), File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        SafePointer<BuildPanel> safe (this);
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectMultipleItems,
                              [safe] (const FileChooser& fc)
                              {
                                  if (safe != nullptr)
                                      safe->addFiles (fc.getResults());
                              });
    }

    void addFiles (const Array<File>& files)
    {
        if (files.isEmpty())
            return;
        int next = 36;
        for (auto& e : entries)
            next = jmax (next, e.hi + 1);

        int added = 0;
        for (auto f : files)
        {
            if (! f.existsAsFile())
                continue;
            auto dest = shownDir.getChildFile (f.getFileName());
            if (f.getParentDirectory() != shownDir)
            {
                if (dest.existsAsFile())
                    dest = shownDir.getNonexistentChildFile (f.getFileNameWithoutExtension(), f.getFileExtension(), false);
                if (! f.copyFileTo (dest))
                    continue;
            }
            ToyProcessor::MapLine l;
            l.lo = l.hi = l.root = jmin (127, next++);
            l.file = dest.getFileName();
            entries.push_back (l);
            ++added;
        }
        save ((int) entries.size() - 1);
        announce (ro ("Am adăugat ") + String (added) + (added == 1 ? ro (" sunet") : ro (" sunete"))
                  + ro (". Fiecare e pe clapa următoare, le poți muta din listă."));
        list.grabKeyboardFocus();
    }

    void listen()
    {
        const int r = list.getSelectedRow();
        if (! isPositiveAndBelow (r, (int) entries.size()))
            return;
        const auto& e = entries[(size_t) r];
        const int note = (e.root >= e.lo && e.root <= e.hi) ? e.root : e.lo;
        proc.keyboardState.noteOn (1, note, 0.8f);
        auto& ks = proc.keyboardState;
        Timer::callAfterDelay (1500, [&ks, note] { ks.noteOff (1, note, 0.0f); });
    }

    void startLearning()
    {
        if (! isPositiveAndBelow (list.getSelectedRow(), (int) entries.size()))
            return;
        learningRow = list.getSelectedRow();
        learnCount = proc.noteCount.load();
        announce (ro ("Apasă clapa pe care vrei sunetul."));
        startTimer (40);
    }

    void timerCallback() override
    {
        if (proc.noteCount.load() == learnCount)
            return;
        stopTimer();
        const int note = proc.lastNote.load();
        if (! isPositiveAndBelow (learningRow, (int) entries.size()) || note < 0)
            return;
        auto& e = entries[(size_t) learningRow];
        e.lo = e.hi = e.root = note;
        save (learningRow);
        showFields();
        announce (e.file + ro (" e pe ") + ToyProcessor::noteName (note));
    }

    void removeSound()
    {
        const int r = list.getSelectedRow();
        if (! isPositiveAndBelow (r, (int) entries.size()))
            return;
        const auto name = entries[(size_t) r].file;
        entries.erase (entries.begin() + r);
        save (jmin (r, (int) entries.size() - 1));
        showFields();
        announce (ro ("Am scos ") + name + ro (" din instrument. Fișierul a rămas în folder."));
    }

    ToyProcessor& proc;
    Label newLabel, instLabel, loLabel, hiLabel, rootLabel, grpLabel, vloLabel, vhiLabel;
    TextEditor newName;
    TextButton createButton, addButton, listenButton, learnButton, removeButton;
    ComboBox instBox, loBox, hiBox, rootBox, grpBox, vloBox, vhiBox;
    ListBox list;

    std::vector<ToyProcessor::MapLine> entries;
    File shownDir;
    StringArray shownNames;
    std::unique_ptr<FileChooser> chooser;
    int learningRow = -1, learnCount = 0;
};

//==============================================================================
// Claviatura cu tastele calculatorului, pornita cu Control B (ca in REAPER)
class KeyboardMode : public Component
{
public:
    explicit KeyboardMode (ToyProcessor& p) : proc (p)
    {
        setWantsKeyboardFocus (true);
        setTitle (ro ("Claviatură"));
        setDescription (ro ("Cânți cu tastele Z S X D C pentru octava de jos și Q 2 W 3 E pentru cea de sus. Săgeată sus și jos te transpun cu câte un semiton, săgeată stânga și dreapta schimbă octava. Control B sau Escape o închide."));
    }

    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff2c3340));
        g.setColour (Colours::white);
        g.setFont (FontOptions (16.0f));
        g.drawText (ro ("Claviatură pornită, octava ") + String (base / 12 - 1) + ro (", transpus ") + String (transpose), getLocalBounds(), Justification::centred);
    }

    bool keyPressed (const KeyPress& k) override
    {
        const auto mods = k.getModifiers();
        if (mods.isCtrlDown() || mods.isAltDown() || mods.isCommandDown())
            return false;
        const int kc = k.getKeyCode();
        if (kc == KeyPress::leftKey || kc == KeyPress::rightKey)
        {
            allOff();
            base = jlimit (12, 96, base + (kc == KeyPress::rightKey ? 12 : -12));
            speak (ro ("Octava ") + String (base / 12 - 1));
            repaint();
            return true;
        }
        if (kc == KeyPress::upKey || kc == KeyPress::downKey)
        {
            allOff();
            transpose = jlimit (-12, 12, transpose + (kc == KeyPress::upKey ? 1 : -1));
            speak (transpose == 0 ? ro ("Netranspus")
                                  : ro ("Transpus ") + (transpose > 0 ? "+" : "") + String (transpose));
            repaint();
            return true;
        }
        const int off = offsetFor (CharacterFunctions::toLowerCase (k.getTextCharacter()));
        if (off < 0)
            return false;
        const int code = k.getKeyCode();
        for (auto& h : held)
            if (h.code == code)
                return true;   // repetare automata
        const int note = jlimit (0, 127, base + off + transpose);
        held.add ({ code, note });
        proc.keyboardState.noteOn (1, note, 0.8f);
        return true;
    }

    bool keyStateChanged (bool) override
    {
        for (int i = held.size(); --i >= 0;)
            if (! KeyPress::isKeyCurrentlyDown (held[i].code))
            {
                proc.keyboardState.noteOff (1, held[i].note, 0.0f);
                held.remove (i);
            }
        return false;
    }

    void focusLost (FocusChangeType) override { allOff(); }

    void allOff()
    {
        for (auto& h : held)
            proc.keyboardState.noteOff (1, h.note, 0.0f);
        held.clear();
    }

private:
    static int offsetFor (juce_wchar c)
    {
        static const char* low  = "zsxdcvgbhnjm,l.;/";
        static const char* high = "q2w3er5t6y7ui9o0p";
        for (int i = 0; low[i] != 0; ++i)
            if (c == (juce_wchar) low[i]) return i;
        for (int i = 0; high[i] != 0; ++i)
            if (c == (juce_wchar) high[i]) return 12 + i;
        return -1;
    }

    struct Held { int code, note; };
    ToyProcessor& proc;
    Array<Held> held;
    int base = 48;
    int transpose = 0;
};

//==============================================================================
namespace
{
class ToyTabButton : public TabBarButton
{
public:
    ToyTabButton (const String& name, TabbedButtonBar& bar, ToyTabs& owner)
        : TabBarButton (name, bar), tabs (owner) {}

    bool keyPressed (const KeyPress& k) override
    {
        const auto m = k.getModifiers();
        if (! m.isAnyModifierKeyDown())
        {
            const int c = k.getKeyCode();
            if (c == KeyPress::rightKey || c == KeyPress::downKey) { tabs.moveTab (1); return true; }
            if (c == KeyPress::leftKey  || c == KeyPress::upKey)   { tabs.moveTab (-1); return true; }
            if (c == KeyPress::homeKey) { tabs.moveTab (0, true); return true; }
            if (c == KeyPress::endKey)  { tabs.moveTab (tabs.getNumTabs() - 1, true); return true; }
            if (c == KeyPress::spaceKey || c == KeyPress::returnKey) return true;
        }
        return TabBarButton::keyPressed (k);
    }

    // NVDA il citeste ca fila adevarata (tab item, selectat), ca intr-un TabControl din Windows
    std::unique_ptr<AccessibilityHandler> createAccessibilityHandler() override
    {
        struct Handler : public AccessibilityHandler
        {
            explicit Handler (ToyTabButton& b)
                : AccessibilityHandler (b, AccessibilityRole::radioButton,
                                        AccessibilityActions().addAction (AccessibilityActionType::press,
                                                                          [&b] { b.tabs.setCurrentTabIndex (b.getIndex()); b.tabs.updateTabFocus(); })),
                  button (b) {}
            AccessibleState getCurrentState() const override
            {
                auto st = AccessibilityHandler::getCurrentState();
                if (button.isFrontTab())
                    st = st.withChecked().withSelected();
                return st;
            }
            String getTitle() const override { return button.getButtonText(); }
            ToyTabButton& button;
        };
        return std::make_unique<Handler> (*this);
    }

    ToyTabs& tabs;
};
}

TabBarButton* ToyTabs::createTabButton (const String& name, int)
{
    return new ToyTabButton (name, getTabbedButtonBar(), *this);
}

void ToyTabs::updateTabFocus()
{
    const int n = getNumTabs(), cur = getCurrentTabIndex();
    const auto names = getTabNames();
    for (int i = 0; i < n; ++i)
        if (auto* b = getTabbedButtonBar().getTabButton (i))
        {
            b->setWantsKeyboardFocus (i == cur);
            b->setExplicitFocusOrder (1);
            b->setTitle (names[i]);
            if (auto* h = b->getAccessibilityHandler())
                h->notifyAccessibilityEvent (AccessibilityEvent::valueChanged);
        }
}

void ToyTabs::currentTabChanged (int index, const String&)
{
    updateTabFocus();
    if (onTabChanged)
        onTabChanged (index);
}

void ToyTabs::moveTab (int delta, bool absolute)
{
    const int n = getNumTabs();
    if (n == 0)
        return;
    const int target = absolute ? jlimit (0, n - 1, delta) : ((getCurrentTabIndex() + delta) % n + n) % n;
    setCurrentTabIndex (target);
    updateTabFocus();
    if (auto* b = getTabbedButtonBar().getTabButton (target))
        b->grabKeyboardFocus();
}

//==============================================================================
// Aplicatia de pe desktop: fereastra vine in fata si primeste tastatura (si dupa actualizare)
static void bringToFront (Component& c)
{
    Process::makeForegroundProcess();
    auto* top = c.getTopLevelComponent();
    if (top == nullptr)
        return;
    top->toFront (true);
   #if JUCE_WINDOWS
    if (auto* peer = top->getPeer())
    {
        auto hwnd = (HWND) peer->getNativeHandle();
        auto fg = GetForegroundWindow();
        if (hwnd != nullptr && fg != hwnd)
        {
            const DWORD fgThread = GetWindowThreadProcessId (fg, nullptr), me = GetCurrentThreadId();
            if (fgThread != 0 && fgThread != me)
                AttachThreadInput (me, fgThread, TRUE);
            ShowWindow (hwnd, SW_RESTORE);
            BringWindowToTop (hwnd);
            SetForegroundWindow (hwnd);
            SetFocus (hwnd);
            if (fgThread != 0 && fgThread != me)
                AttachThreadInput (me, fgThread, FALSE);
        }
    }
   #endif
}

ToyEditor::ToyEditor (ToyProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    play = new PlayPanel (p);
    build = new BuildPanel (p);
    settings = new SettingsPanel (p, [this] { if (play != nullptr) play->announceAfterChange(); });
    const auto bg = Colour (0xff20242c);
    // filele au doar butoanele; panourile raman mereu copii ai ferestrei (doar se ascund),
    // altfel NVDA nu citea elementele din filele 2 si 3 dupa ce JUCE le scotea si le punea la loc
    tabs.addTab (ro ("Cântă"), bg, nullptr, false);
    tabs.addTab (ro ("Construiește instrumente"), bg, nullptr, false);
    tabs.addTab (ro ("Setări"), bg, nullptr, false);
    tabs.updateTabFocus();
    addAndMakeVisible (tabs);
    addAndMakeVisible (play);
    addChildComponent (build);
    addChildComponent (settings);
    tabs.onTabChanged = [this] (int i)
    {
        play->setVisible (i == 0);
        build->setVisible (i == 1);
        settings->setVisible (i == 2);
    };

    keys = std::make_unique<KeyboardMode> (p);
    addChildComponent (*keys);

    setSize (700, 560);

    if (p.wrapperType == AudioProcessor::wrapperType_Standalone)
        startUpdateCheck();

    SafePointer<ToyEditor> safe (this);
    const bool standalone = p.wrapperType == AudioProcessor::wrapperType_Standalone;
    Timer::callAfterDelay (400, [safe, standalone]
    {
        if (safe == nullptr || ! safe->isShowing())
            return;
        if (standalone)
            bringToFront (*safe);
        if (auto* b = safe->tabs.getTabbedButtonBar().getTabButton (safe->tabs.getCurrentTabIndex()))
            b->grabKeyboardFocus();
    });
}

ToyEditor::~ToyEditor()
{
    delete play;
    delete build;
    delete settings;
}

void ToyEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff20242c));
}

void ToyEditor::resized()
{
    tabs.setBounds (getLocalBounds());
    const auto area = getLocalBounds().withTrimmedTop (tabs.getTabBarDepth());
    for (Component* c : std::initializer_list<Component*> { play, build, settings })
        if (c != nullptr)
            c->setBounds (area);
    keys->setBounds (getLocalBounds().removeFromBottom (60).reduced (12, 8));
}

void ToyEditor::focusTab (int index)
{
    keys->setVisible (false);
    tabs.setCurrentTabIndex (index);
    speak (ro ("Fila ") + tabs.getTabNames()[index]);
    SafePointer<ToyEditor> safe (this);
    Timer::callAfterDelay (150, [safe, index]
    {
        if (safe == nullptr)
            return;
        if (index == 0 && safe->play != nullptr)
            safe->play->instrumentBox.grabKeyboardFocus();
        else if (index == 1 && safe->build != nullptr)
            safe->build->grabKeyboardFocus();
        else if (index == 2 && safe->settings != nullptr)
            safe->settings->grabKeyboardFocus();
    });
}

void ToyEditor::toggleKeyboard()
{
    if (keys->isVisible())
    {
        keys->allOff();
        keys->setVisible (false);
        speak (ro ("Claviatură oprită"));
        if (play != nullptr && tabs.getCurrentTabIndex() == 0)
            play->instrumentBox.grabKeyboardFocus();
        return;
    }
    keys->setVisible (true);
    keys->toFront (false);
    keys->grabKeyboardFocus();
    speak (ro ("Claviatură pornită"));
}

bool ToyEditor::keyPressed (const KeyPress& k)
{
    const auto m = k.getModifiers();
    const auto c = k.getKeyCode();

    if (m.isCtrlDown() && ! m.isAltDown() && c == KeyPress::tabKey)
    {
        const int n = tabs.getNumTabs();
        focusTab ((tabs.getCurrentTabIndex() + (m.isShiftDown() ? n - 1 : 1)) % n);
        return true;
    }
    if (m.isCtrlDown() && ! m.isAltDown() && ! m.isShiftDown() && c >= '1' && c <= '9')
    {
        const int index = c - '1';
        if (index < tabs.getNumTabs())
        {
            focusTab (index);
            return true;
        }
    }
    if (m.isCtrlDown() && ! m.isAltDown() && (c == 'B' || c == 'b'))
    {
        toggleKeyboard();
        return true;
    }
    if (c == KeyPress::escapeKey && keys->isVisible())
    {
        toggleKeyboard();
        return true;
    }
    if (m.isAltDown() && ! m.isCtrlDown() && play != nullptr)
    {
        if (c == '1') { play->step (-1); return true; }
        if (c == '2') { play->step (1); return true; }
        if (c == '3' && settings != nullptr)
        {
            keys->setVisible (false);
            tabs.setCurrentTabIndex (2);
            settings->chooseFolder();
            return true;
        }
    }
    return false;
}
