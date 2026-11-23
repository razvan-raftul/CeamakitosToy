#include "PluginEditor.h"
#include "Updater.h"
#include <algorithm>
#include <cstring>
#include "Speech.h"
#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

using namespace juce;

// placa de sunet a aplicatiei (doar in aplicatie, nu in REAPER)
static AudioDeviceManager* appDevices()
{
   #if JucePlugin_Build_Standalone
    if (auto* h = StandalonePluginHolder::getInstance())
        return &h->deviceManager;
   #endif
    return nullptr;
}

static File audioMark()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Ceamakito's toy").getChildFile ("sunet.txt");
}

// drivere ASIO generale, instalate si fara placa de sunet (iau sunetul Windows-ului si NVDA amuteste)
static bool isGenericAsio (const String& name)
{
    auto n = name.toLowerCase();
    for (auto* g : { "asio4all", "realtek", "fl studio", "generic low latency", "voicemeeter", "built-in", "magix", "steinberg generic" })
        if (n.contains (g)) return true;
    return false;
}

static StringArray realAsioDevices (AudioDeviceManager& dm)
{
    StringArray out;
    for (auto* t : dm.getAvailableDeviceTypes())
        if (t->getTypeName() == "ASIO")
        {
            t->scanForDevices();
            for (auto& d : t->getDeviceNames())
                if (! isGenericAsio (d)) out.add (d);
        }
    return out;
}

// implicit: sunetul normal de Windows (o data, si pentru cine avea deja ASIO pus de versiunea veche)
static void firstRunNormal (AudioDeviceManager& dm)
{
    if (audioMark().loadFileAsString().trim() == "2")
        return;
    audioMark().getParentDirectory().createDirectory();
    audioMark().replaceWithText ("2");
    dm.setCurrentAudioDeviceType ("Windows Audio", true);
}

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
                const int idx = proc.getCurrentIndex(), n = names.size();
                announce (st + (n > 1 && idx == 0 ? ro (", primul") : n > 1 && idx == n - 1 ? ro (", ultimul") : String()));
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
class SettingsPanel : public Component, private ChangeListener, private Timer
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
        volumeSlider.setWantsKeyboardFocus (true);

        setupLabel (sensLabel, ro ("Cât contează cât de tare apeși clapa"), sensSlider);
        sensSlider.setSliderStyle (Slider::LinearHorizontal);
        sensSlider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (sensSlider);
        sensAtt = std::make_unique<SA> (proc.apvts, "sensibilitate", sensSlider);
        sensSlider.setWantsKeyboardFocus (true);
        sensSlider.textFromValueFunction = [] (double v) { return String (roundToInt (v * 100.0)) + ro (" la sută"); };
        sensSlider.updateText();

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

        dm = standalone ? appDevices() : nullptr;
        for (auto* b : { &driverBox, &deviceBox, &bufferBox })
            addChildComponent (*b);
        if (dm != nullptr)
        {
            setupLabel (driverLabel, "Driverul de sunet", driverBox);
            setupLabel (deviceLabel, ro ("Placa de sunet"), deviceBox);
            setupLabel (bufferLabel, ro ("Întârzierea"), bufferBox);
            for (auto* b : { &driverBox, &deviceBox, &bufferBox })
                b->setVisible (true);
            for (auto* box : { &driverBox, &deviceBox, &bufferBox })
                box->onChange = [this, box]
                {
                    if (filling) return;
                    announce (box->getText());
                    pendingBox = box;
                    startTimer (900);
                };
            firstRunNormal (*dm);
            fillAudio();
        }

        int order = 1;
        for (Component* c : std::initializer_list<Component*> { &chooseButton, &folderBox, &volumeSlider, &sensSlider,
                                                               &releaseBox, &driverBox, &deviceBox, &bufferBox, &versionBox, &updateButton })
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
        if (dm != nullptr)
            for (auto* b : { &driverBox, &deviceBox, &bufferBox })
                b->setBounds (row (26));
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

    // nume pe intelesul tau
    static String driverLabelFor (const String& type)
    {
        if (type == "Windows Audio") return ro ("Windows normal");
        if (type == "Windows Audio (Low Latency Mode)") return ro ("Windows cu întârziere mică");
        if (type == "ASIO") return "ASIO";
        return type;
    }

    // se aplica abia dupa ce te opresti din sageti (schimbarea placii blocheaza putin)
    void timerCallback() override
    {
        stopTimer();
        auto* box = pendingBox;
        pendingBox = nullptr;
        if (box == &driverBox)
        {
            const int i = driverBox.getSelectedItemIndex();
            if (! isPositiveAndBelow (i, driverTypes.size())) return;
            dm->setCurrentAudioDeviceType (driverTypes[i], true);
            if (driverTypes[i] == "ASIO")
            {
                auto real = realAsioDevices (*dm);
                if (real.size() > 0)
                {
                    auto st = dm->getAudioDeviceSetup();
                    st.outputDeviceName = st.inputDeviceName = real[0];
                    dm->setAudioDeviceSetup (st, true);
                }
            }
        }
        else if (box == &deviceBox)
        {
            auto st = dm->getAudioDeviceSetup();
            st.outputDeviceName = deviceBox.getText();
            st.inputDeviceName = dm->getCurrentAudioDeviceType() == "ASIO" ? deviceBox.getText() : String();
            dm->setAudioDeviceSetup (st, true);
        }
        else if (box == &bufferBox)
        {
            const int i = bufferBox.getSelectedItemIndex();
            if (! isPositiveAndBelow (i, buffers.size())) return;
            auto st = dm->getAudioDeviceSetup();
            st.bufferSize = buffers[i];
            dm->setAudioDeviceSetup (st, true);
        }
        else return;

        // daca n-a pornit nimic, inapoi la sunetul normal, ca sa nu ramai fara sunet
        if (dm->getCurrentAudioDevice() == nullptr)
        {
            dm->setCurrentAudioDeviceType ("Windows Audio", true);
            announce (ro ("Nu a mers, am pus înapoi Windows normal."));
        }
        fillAudio();
    }

    void fillAudio()
    {
        filling = true;
        driverBox.clear (dontSendNotification);
        deviceBox.clear (dontSendNotification);
        bufferBox.clear (dontSendNotification);
        buffers.clear();
        driverTypes.clear();
        const auto cur = dm->getCurrentAudioDeviceType();
        const auto real = realAsioDevices (*dm);
        StringArray wanted { "Windows Audio", "Windows Audio (Low Latency Mode)" };
        if (real.size() > 0 || cur == "ASIO") wanted.add ("ASIO");
        for (auto* t : dm->getAvailableDeviceTypes())
            if (wanted.contains (t->getTypeName()))
            {
                driverTypes.add (t->getTypeName());
                driverBox.addItem (driverLabelFor (t->getTypeName()), driverTypes.size());
                if (t->getTypeName() == cur) driverBox.setSelectedId (driverTypes.size(), dontSendNotification);
            }
        if (auto* t = dm->getCurrentDeviceTypeObject())
        {
            if (cur == "ASIO") deviceBox.addItemList (real, 1);
            else { t->scanForDevices(); deviceBox.addItemList (t->getDeviceNames (false), 1); }
        }
        if (auto* d = dm->getCurrentAudioDevice())
        {
            deviceBox.setText (d->getName(), dontSendNotification);
            const double rate = d->getCurrentSampleRate() > 0 ? d->getCurrentSampleRate() : 44100.0;
            int id = 1;
            for (auto b : d->getAvailableBufferSizes())
            {
                buffers.add (b);
                bufferBox.addItem (String (b) + ro (" eșantioane, ") + String (b * 1000.0 / rate, 1).replaceCharacter ('.', ',') + " milisecunde", id);
                if (b == d->getCurrentBufferSizeSamples()) bufferBox.setSelectedId (id, dontSendNotification);
                ++id;
            }
        }
        filling = false;
    }

    StringArray driverTypes;
    ComboBox* pendingBox = nullptr;
    AudioDeviceManager* dm = nullptr;
    bool filling = false;
    ComboBox driverBox, deviceBox, bufferBox;
    Label driverLabel, deviceLabel, bufferLabel;
    Array<int> buffers;

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
//==============================================================================
// Nota din numele fisierului: "a2", "C#4", "db3", "Do4", "sol#2", "la_3"... (Do4 = C4 = 60)
static int noteFromName (const String& fileName)
{
    auto stem = File::createFileWithoutCheckingPath (fileName).getFileNameWithoutExtension().toLowerCase();
    auto tokens = StringArray::fromTokens (stem, " _-.()[]", "");
    tokens.removeEmptyStrings();
    // si perechi lipite cu spatiu/liniuta: "la 3" -> "la3"
    for (int i = 0; i + 1 < tokens.size(); ++i)
        if (tokens[i + 1].containsOnly ("-0123456789") && tokens[i].containsOnly ("abcdefgilmors#"))
            tokens.add (tokens[i] + tokens[i + 1]);

    static const std::pair<const char*, int> ro[] = { { "do", 0 }, { "re", 2 }, { "mi", 4 }, { "fa", 5 }, { "sol", 7 }, { "la", 9 }, { "si", 11 } };
    for (int t = tokens.size(); --t >= 0;)   // ultimul token cu nota castiga (de obicei e la final)
    {
        const auto tok = tokens[t];
        int i = 0, pc = -1;
        for (auto& r : ro)
            if (tok.startsWith (r.first)) { pc = r.second; i = (int) strlen (r.first); break; }
        if (pc < 0)
        {
            const auto c = tok[0];
            if (c < 'a' || c > 'g') continue;
            static const int en[] = { 9, 11, 0, 2, 4, 5, 7 };   // a b c d e f g
            pc = en[c - 'a']; i = 1;
        }
        auto rest = tok.substring (i);
        if (rest.startsWith ("#") || rest.startsWith ("s")) { ++pc; rest = rest.substring (1); }
        else if (rest.startsWith ("b") && rest.length() > 1) { --pc; rest = rest.substring (1); }
        if (rest.isEmpty() || ! rest.containsOnly ("-0123456789") || rest.length() > 2) continue;
        const int oct = rest.getIntValue();
        if (oct < -1 || oct > 9) continue;
        const int note = (oct + 1) * 12 + pc;
        if (note >= 0 && note <= 127)
            return note;
    }
    return -1;
}

// cat de tare din nume: pp, p, mp, mf, f, ff (sau soft / loud); -1 daca nu scrie
static int dynamicFromName (const String& fileName)
{
    auto stem = File::createFileWithoutCheckingPath (fileName).getFileNameWithoutExtension().toLowerCase();
    auto tokens = StringArray::fromTokens (stem, " _-.()[]", "");
    static const std::pair<const char*, int> dyn[] = { { "ppp", 0 }, { "pp", 1 }, { "p", 2 }, { "soft", 2 }, { "mp", 3 }, { "mf", 4 }, { "f", 5 }, { "loud", 5 }, { "ff", 6 }, { "fff", 7 } };
    for (auto& tok : tokens)
        for (auto& d : dyn)
            if (tok == d.first)
                return d.second;
    return -1;
}

// Lista care spune singura prin NVDA randul pe care esti (NVDA nu citea randurile listei JUCE)
class SpokenList : public ListBox
{
public:
    std::function<void()> onFocus;
    void focusGained (FocusChangeType t) override
    {
        ListBox::focusGained (t);
        if (onFocus) onFocus();
    }
};

// "opreste" sau "stop" in nume: sunetul tine doar cat tii clapa; "continua": se aude pana la capat
static int stopFromName (const String& fileName)
{
    auto stem = File::createFileWithoutCheckingPath (fileName).getFileNameWithoutExtension().toLowerCase();
    auto tokens = StringArray::fromTokens (stem, " _-.()[]", "");
    for (auto& tok : tokens)
    {
        if (tok == "opreste" || tok == ro ("oprește") || tok == ro ("opreşte") || tok == "stop") return 1;
        if (tok == "continua" || tok == ro ("continuă")) return 2;
    }
    return 0;
}

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

        setupCombo (fillBox, fillLabel, ro ("Umple clapele dintre note"), { "Da", "Nu" });
        fillBox.setSelectedItemIndex (fillFile().loadFileAsString().trim() == "0" ? 1 : 0, dontSendNotification);
        fillBox.onChange = [this]
        {
            fillFile().getParentDirectory().createDirectory();
            fillFile().replaceWithText (fillOn() ? "1" : "0");
        };

        list.setModel (this);
        list.setTitle ("Sunetele instrumentului");
        list.setRowHeight (24);
        list.onFocus = [this]
        {
            if (quietFocus) { quietFocus = false; return; }
            String t = ro ("Sunetele instrumentului, listă. ");
            if (entries.empty()) t << ro ("Lista e goală, adaugă sunete.");
            else t << rowText (list.getSelectedRow());
            announce (t);
        };
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
        setupCombo (stopBox, stopLabel, ro ("La ridicarea clapei, pentru acest sunet"),
                    { ro ("Ca în Setări"), ro ("Sunetul se oprește"), ro ("Sunetul continuă până la capăt") });
        setupCombo (vloBox, vloLabel, ro ("Cel mai încet la care sună"), levels);
        setupCombo (vhiBox, vhiLabel, ro ("Cel mai tare la care sună"), levels);

        int order = 1;
        for (Component* c : std::initializer_list<Component*> { &newName, &createButton, &instBox, &addButton, &fillBox, &list, &listenButton,
                                                               &learnButton, &removeButton, &loBox, &hiBox, &rootBox, &grpBox, &vloBox, &vhiBox, &stopBox })
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
        fillBox.setBounds (r.removeFromTop (24).withTrimmedLeft (labelW));
        r.removeFromTop (6);
        list.setBounds (r.removeFromTop (130));
        r.removeFromTop (6);
        auto b = r.removeFromTop (28);
        r.removeFromTop (6);
        const int w = b.getWidth() / 3;
        listenButton.setBounds (b.removeFromLeft (w).reduced (2, 0));
        learnButton.setBounds (b.removeFromLeft (w).reduced (2, 0));
        removeButton.setBounds (b.reduced (2, 0));
        for (auto* c : { &loBox, &hiBox, &rootBox, &grpBox, &vloBox, &vhiBox, &stopBox })
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
        if (e.stop == 1) s << ro (", se oprește când ridici clapa");
        if (e.stop == 2) s << ro (", continuă până la capăt");
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

    void selectedRowsChanged (int) override
    {
        showFields();
        if (list.hasKeyboardFocus (true) && ! quietFocus)
            announce (rowText (list.getSelectedRow()));
    }

    String rowText (int r)
    {
        if (! isPositiveAndBelow (r, (int) entries.size()))
            return {};
        return getNameForRow (r) + ", " + String (r + 1) + " din " + String ((int) entries.size());
    }

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
        {
            const bool q = quietFocus;
            quietFocus = true;
            list.selectRow (selectRow);
            quietFocus = q;
        }
        proc.reloadCurrent();
    }

    void showFields()
    {
        const int r = list.getSelectedRow();
        const bool ok = isPositiveAndBelow (r, (int) entries.size());
        for (auto* c : { &loBox, &hiBox, &rootBox, &grpBox, &vloBox, &vhiBox, &stopBox })
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
        stopBox.setSelectedItemIndex (jlimit (0, 2, e.stop), dontSendNotification);
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
        e.stop = jmax (0, stopBox.getSelectedItemIndex());
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

    void addFiles (const Array<File>& filesIn)
    {
        if (filesIn.isEmpty())
            return;
        auto files = filesIn;
        std::sort (files.begin(), files.end(), [] (const File& a, const File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });

        int next = 36;
        for (auto& e : entries)
            next = jmax (next, e.hi + 1);

        struct Added { String file; int note, dyn; };
        std::vector<Added> withNote, without;
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
            const int n = noteFromName (f.getFileName());
            (n >= 0 ? withNote : without).push_back ({ dest.getFileName(), n, dynamicFromName (f.getFileName()) });
        }

        // sunetele cu nota in nume: fiecare pe nota lui; clapele dintre note se impart intre vecini
        std::vector<int> notes;
        for (auto& a : withNote) if (std::find (notes.begin(), notes.end(), a.note) == notes.end()) notes.push_back (a.note);
        std::sort (notes.begin(), notes.end());
        for (size_t k = 0; k < notes.size(); ++k)
        {
            const int n = notes[k];
            int lo, hi;
            if (! fillOn()) { lo = hi = n; }
            else if (notes.size() == 1) { lo = jmax (0, n - 24); hi = jmin (127, n + 24); }
            else
            {
                lo = k == 0 ? jmax (0, n - (notes[1] - n) / 2) : notes[k - 1] + (n - notes[k - 1]) / 2 + 1;
                hi = k + 1 == notes.size() ? jmin (127, n + (n - notes[k - 1]) / 2) : n + (notes[k + 1] - n) / 2;
            }
            // pe aceeasi nota: straturi dupa cat de tare (p, f...), iar ce are aceeasi tarie se alterneaza (round robin)
            std::vector<int> dyns;
            for (auto& a : withNote) if (a.note == n && std::find (dyns.begin(), dyns.end(), a.dyn) == dyns.end()) dyns.push_back (a.dyn);
            std::sort (dyns.begin(), dyns.end());
            for (auto& a : withNote)
            {
                if (a.note != n) continue;
                ToyProcessor::MapLine l;
                l.lo = lo; l.hi = hi; l.root = n; l.file = a.file; l.stop = stopFromName (a.file);
                const int layers = (int) dyns.size();
                if (layers > 1)
                {
                    const int idx = (int) (std::find (dyns.begin(), dyns.end(), a.dyn) - dyns.begin());
                    l.vlo = idx * 128 / layers;
                    l.vhi = (idx + 1) * 128 / layers - 1;
                }
                entries.push_back (l);
            }
        }
        // restul (fara nota in nume, ca la tobe): cate unul pe clapa urmatoare
        for (auto& a : without)
        {
            ToyProcessor::MapLine l;
            l.lo = l.hi = l.root = jmin (127, next++);
            l.file = a.file;
            l.stop = stopFromName (a.file);
            entries.push_back (l);
        }

        const int added = (int) (withNote.size() + without.size());
        save ((int) entries.size() - 1);
        String msg = ro ("Am adăugat ") + String (added) + (added == 1 ? ro (" sunet") : ro (" sunete")) + ". ";
        if (! withNote.empty())
            msg << String ((int) withNote.size()) << ro (" le-am pus pe nota din nume, de la ") << ToyProcessor::noteName (notes.front())
                << ro (" până la ") << ToyProcessor::noteName (notes.back()) << ". ";
        if (! without.empty())
            msg << String ((int) without.size()) << ro (" nu aveau nota în nume și le-am pus pe clapele următoare.");
        announce (msg);
        quietFocus = true;
        list.grabKeyboardFocus();
        quietFocus = false;
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
    Label newLabel, instLabel, loLabel, hiLabel, rootLabel, grpLabel, vloLabel, vhiLabel, stopLabel;
    TextEditor newName;
    TextButton createButton, addButton, listenButton, learnButton, removeButton;
    ComboBox instBox, loBox, hiBox, rootBox, grpBox, vloBox, vhiBox, stopBox;
    SpokenList list;
    ComboBox fillBox;
    Label fillLabel;
    bool fillOn() const { return fillBox.getSelectedItemIndex() != 1; }
    static File fillFile()
    {
        return File::getSpecialLocation (File::userApplicationDataDirectory)
                   .getChildFile ("Ceamakito's toy").getChildFile ("umple.txt");
    }
    bool quietFocus = false;

    std::vector<ToyProcessor::MapLine> entries;
    File shownDir;
    StringArray shownNames;
    std::unique_ptr<FileChooser> chooser;
    int learningRow = -1, learnCount = 0;
};

//==============================================================================
// Claviatura cu tastele calculatorului, pornita cu Alt B
class KeyboardMode : public Component
{
public:
    explicit KeyboardMode (ToyProcessor& p) : proc (p)
    {
        setWantsKeyboardFocus (true);
        setTitle (ro ("Claviatură"));
        setDescription (ro ("Cânți cu tastele Z S X D C pentru octava de jos și Q 2 W 3 E pentru cea de sus. Săgeată sus și jos te transpun cu câte un semiton, săgeată stânga și dreapta schimbă octava. Alt B sau Escape o închide."));
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

private:
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
        }
}

void ToyTabs::currentTabChanged (int, const String&)
{
    updateTabFocus();
}

void ToyTabs::moveTab (int delta, bool absolute)
{
    const int n = getNumTabs();
    if (n == 0)
        return;
    // la capete se opreste, ca un TabControl din Windows (nu sare de la ultima la prima)
    const int target = absolute ? jlimit (0, n - 1, delta) : jlimit (0, n - 1, getCurrentTabIndex() + delta);
    setCurrentTabIndex (target);
    updateTabFocus();
    if (auto* b = getTabbedButtonBar().getTabButton (target))
        b->grabKeyboardFocus();
}

//==============================================================================
ToyEditor::ToyEditor (ToyProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    play = new PlayPanel (p);
    build = new BuildPanel (p);
    settings = new SettingsPanel (p, [this] { if (play != nullptr) play->announceAfterChange(); });
    const auto bg = Colour (0xff20242c);
    tabs.addTab (ro ("Cântă"), bg, play, true);
    tabs.addTab (ro ("Construiește instrumente"), bg, build, true);
    tabs.addTab (ro ("Setări"), bg, settings, true);
    tabs.updateTabFocus();
    addAndMakeVisible (tabs);

    keys = std::make_unique<KeyboardMode> (p);
    addChildComponent (*keys);

    setSize (700, 560);

    if (p.wrapperType == AudioProcessor::wrapperType_Standalone)
        startUpdateCheck();

    // JUCE face din randul cu file un container care tine Tab-ul inauntru; il deschidem,
    // ca Tab sa treaca din rand in fila deschisa si inapoi (Shift Tab)
    tabs.getTabbedButtonBar().setFocusContainerType (FocusContainerType::none);

    // cand fereastra devine activa si nimic nu are focus, punem cursorul pe fila aleasa
    SafePointer<ToyEditor> safe (this);
    auto tries = std::make_shared<int> (0);
    auto focusTabs = std::make_shared<std::function<void()>>();
    *focusTabs = [safe, tries, focusTabs]
    {
        if (safe == nullptr || ++(*tries) > 40)
            return;
        auto* peer = safe->getPeer();
        const bool active = peer != nullptr && peer->isFocused();
        if (safe->isShowing() && active && ! safe->hasKeyboardFocus (true))
        {
            if (auto* b = safe->tabs.getTabbedButtonBar().getTabButton (safe->tabs.getCurrentTabIndex()))
                b->grabKeyboardFocus();
            return;
        }
        if (! safe->hasKeyboardFocus (true))
            Timer::callAfterDelay (250, *focusTabs);
    };
    Timer::callAfterDelay (300, *focusTabs);
}

ToyEditor::~ToyEditor() = default;

void ToyEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff20242c));
}

void ToyEditor::resized()
{
    tabs.setBounds (getLocalBounds());
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
        else if (auto* c = safe->tabs.getCurrentContentComponent())
            c->grabKeyboardFocus();
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
    if (m.isAltDown() && ! m.isCtrlDown() && (c == 'B' || c == 'b'))
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
