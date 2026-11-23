#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Updater.h"
#include <algorithm>
#include <cmath>

using namespace juce;

static const char* soundPatterns = "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3";

static bool hasSounds (const File& d)
{
    if (d.getChildFile ("harta.txt").existsAsFile())
        return true;
    return d.getNumberOfChildFiles (File::findFiles, soundPatterns) > 0;
}

static void sortNatural (Array<File>& list)
{
    std::sort (list.begin(), list.end(), [] (const File& a, const File& b)
    {
        return a.getFileName().compareNatural (b.getFileName()) < 0;
    });
}

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout ToyProcessor::createLayout (ToyProcessor& p)
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "instrument", 1 }, "Instrument",
                                                     0, maxInstruments - 1, 0,
                                                     AudioParameterIntAttributes()
                                                         .withStringFromValueFunction ([&p] (int v, int) { return p.instrumentName (v); })));

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "numar", 1 }, "Numar instrumente",
                                                     0, maxInstruments, 0,
                                                     AudioParameterIntAttributes().withAutomatable (false)));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "reincarca", 1 }, "Reincarca folderul", false,
                                                      AudioParameterBoolAttributes().withAutomatable (false)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "volum", 1 }, "Volum general",
                                                       NormalisableRange<float> (-30.0f, 12.0f, 0.5f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "sensibilitate", 1 },
                                                       ro ("Cât contează cât de tare apeși clapa"),
                                                       NormalisableRange<float> (0.0f, 1.0f, 0.05f), 1.0f));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "ridicare", 1 }, ro ("La ridicarea clapei"),
                                                        StringArray { ro ("Sunetul continuă până la capăt"),
                                                                      ro ("Sunetul se oprește") }, 0));
    return layout;
}

ToyProcessor::ToyProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      Thread ("Ceamakito loader"),
      apvts (*this, nullptr, "CeamakitosToy", createLayout (*this))
{
    formats.registerBasicFormats();

    volumeParam  = apvts.getRawParameterValue ("volum");
    sensParam    = apvts.getRawParameterValue ("sensibilitate");
    releaseParam = apvts.getRawParameterValue ("ridicare");

    apvts.addParameterListener ("instrument", this);
    apvts.addParameterListener ("reincarca", this);

    {
        const ScopedLock sl (listLock);
        root = readGlobalRoot();
    }
    rescan();
    startThread();
    requestLoad();
}

ToyProcessor::~ToyProcessor()
{
    if (wrapperType == wrapperType_Standalone)
        stopUpdateCheck();
    cancelPendingUpdate();
    apvts.removeParameterListener ("instrument", this);
    apvts.removeParameterListener ("reincarca", this);
    signalThreadShouldExit();
    notify();
    stopThread (4000);
}

bool ToyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::stereo() || out == AudioChannelSet::mono();
}

void ToyProcessor::prepareToPlay (double sampleRate, int)
{
    hostRate = sampleRate > 0 ? sampleRate : 44100.0;
    fadeStep = (float) (1.0 / (0.02 * hostRate));
    for (auto& v : voices)
        v.active = false;
}

//==============================================================================
File ToyProcessor::globalFile()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
               .getChildFile ("Ceamakito's toy").getChildFile ("radacina.txt");
}

File ToyProcessor::defaultRoot()
{
    auto docs = File::getSpecialLocation (File::userDocumentsDirectory)
                    .getChildFile ("Ceamakito's toy").getChildFile ("Instrumente");
    if (docs.isDirectory())
        return docs;
    return File::getSpecialLocation (File::userApplicationDataDirectory)
               .getChildFile ("REAPER").getChildFile ("Data")
               .getChildFile ("Ceamakito's toy").getChildFile ("Instrumente");
}

File ToyProcessor::readGlobalRoot() const
{
    auto g = globalFile();
    if (g.existsAsFile())
    {
        auto path = g.loadFileAsString().trim();
        if (path.isNotEmpty() && File::isAbsolutePath (path) && File (path).isDirectory())
            return File (path);
    }
    auto d = defaultRoot();
    return d.isDirectory() ? d : File();
}

File ToyProcessor::getRoot() const
{
    const ScopedLock sl (listLock);
    return root;
}

void ToyProcessor::setRoot (const File& chosen)
{
    File r = chosen;
    String pick;

    // daca ai ales chiar un instrument, radacina e folderul de deasupra
    bool hasInstrumentFolders = false;
    for (auto& d : chosen.findChildFiles (File::findDirectories, false))
        if (hasSounds (d)) { hasInstrumentFolders = true; break; }

    if (! hasInstrumentFolders && hasSounds (chosen))
    {
        r = chosen.getParentDirectory();
        pick = chosen.getFileName();
    }

    globalFile().getParentDirectory().createDirectory();
    globalFile().replaceWithText (r.getFullPathName());

    {
        const ScopedLock sl (listLock);
        root = r;
    }
    rescan();

    int index = 0;
    if (pick.isNotEmpty())
        index = jmax (0, getInstrumentNames().indexOf (pick));
    selectInstrument (index);
    requestLoad();
}

void ToyProcessor::rescan()
{
    File r = getRoot();
    Array<File> list;

    if (r.isDirectory())
    {
        for (auto& d : r.findChildFiles (File::findDirectories, false))
            if (hasSounds (d))
                list.add (d);
        sortNatural (list);

        if (list.isEmpty() && hasSounds (r))
            list.add (r);
    }

    while (list.size() > maxInstruments)
        list.removeLast();

    {
        const ScopedLock sl (listLock);
        folders = list;
    }

    if (auto* p = apvts.getParameter ("numar"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) list.size()));

    if (list.isEmpty())
        setStatus (r == File() ? ro ("Alege folderul cu instrumente.")
                               : ro ("În folderul ") + r.getFullPathName() + ro (" nu sunt instrumente."));
    changes.sendChangeMessage();
}

juce::StringArray ToyProcessor::getInstrumentNames() const
{
    const ScopedLock sl (listLock);
    StringArray names;
    for (auto& f : folders)
        names.add (f.getFileName());
    return names;
}

int ToyProcessor::getInstrumentCount() const
{
    const ScopedLock sl (listLock);
    return folders.size();
}

juce::String ToyProcessor::instrumentName (int index) const
{
    const ScopedLock sl (listLock);
    if (isPositiveAndBelow (index, folders.size()))
        return folders[index].getFileName();
    return String (index + 1);
}

int ToyProcessor::getCurrentIndex() const
{
    return roundToInt (apvts.getRawParameterValue ("instrument")->load());
}

void ToyProcessor::selectInstrument (int index)
{
    if (auto* p = apvts.getParameter ("instrument"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) jlimit (0, maxInstruments - 1, index)));
        p->endChangeGesture();
    }
}

void ToyProcessor::stepInstrument (int delta)
{
    const int n = getInstrumentCount();
    if (n == 0)
        return;
    selectInstrument (((getCurrentIndex() + delta) % n + n) % n);
}

juce::String ToyProcessor::getStatus() const
{
    const ScopedLock sl (listLock);
    return status;
}

void ToyProcessor::setStatus (const String& s)
{
    {
        const ScopedLock sl (listLock);
        status = s;
    }
    changes.sendChangeMessage();
}

//==============================================================================
void ToyProcessor::parameterChanged (const String& id, float)
{
    if (id == "instrument")
    {
        requestLoad();
        changes.sendChangeMessage();
    }
    else if (id == "reincarca")
    {
        // scriptul din REAPER a scris un folder nou in radacina.txt
        if (MessageManager::existsAndIsCurrentThread())
            handleAsyncUpdate();
        else
            triggerAsyncUpdate();
    }
}

void ToyProcessor::handleAsyncUpdate()
{
    {
        const ScopedLock sl (listLock);
        root = readGlobalRoot();
    }
    rescan();
    requestLoad();
}

void ToyProcessor::requestLoad()
{
    pendingLoad = true;
    notify();
}

void ToyProcessor::run()
{
    while (! threadShouldExit())
    {
        while (pendingLoad.exchange (false) && ! threadShouldExit())
        {
            const int index = getCurrentIndex();
            File dir;
            {
                const ScopedLock sl (listLock);
                if (isPositiveAndBelow (index, folders.size()))
                    dir = folders[index];
            }

            std::shared_ptr<Instrument> inst;
            if (dir != File())
                inst = build (dir);

            if (threadShouldExit())
                return;

            {
                const SpinLock::ScopedLockType sl (instLock);
                current = inst;
            }

            if (inst != nullptr)
            {
                if (inst->zones.empty())
                    setStatus (inst->name + ro (": nu am putut încărca niciun sunet."));
                else
                    setStatus (inst->name + ": " + String ((int) inst->zones.size())
                               + (inst->zones.size() == 1 ? ro (" sunet") : ro (" sunete")));
            }
            else if (getInstrumentCount() > 0)
            {
                setStatus (ro ("Instrumentul ") + String (index + 1) + ro (" nu există."));
            }
        }
        wait (-1);
    }
}

juce::String ToyProcessor::noteName (int note)
{
    static const char* names[] = { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };
    note = jlimit (0, 127, note);
    return String (names[note % 12]) + String (note / 12 - 1) + " (" + String (note) + ")";
}

juce::File ToyProcessor::instrumentFolder (int index) const
{
    const ScopedLock sl (listLock);
    return isPositiveAndBelow (index, folders.size()) ? folders[index] : File();
}

std::vector<ToyProcessor::MapLine> ToyProcessor::readMap (const File& dir)
{
    std::vector<MapLine> lines;
    auto map = dir.getChildFile ("harta.txt");
    if (map.existsAsFile())
    {
        StringArray rows;
        map.readLines (rows);
        for (auto row : rows)
        {
            row = row.trim();
            if (row.isEmpty() || row.startsWithChar ('#'))
                continue;
            const int bar = row.indexOfChar ('|');
            if (bar < 0)
                continue;
            auto nums = StringArray::fromTokens (row.substring (0, bar), " \t", "");
            nums.removeEmptyStrings();
            if (nums.isEmpty())
                continue;

            MapLine l;
            l.lo   = nums[0].getIntValue();
            l.hi   = nums.size() > 1 ? nums[1].getIntValue() : l.lo;
            l.root = nums.size() > 2 ? nums[2].getIntValue() : l.lo;
            l.vlo  = nums.size() > 3 ? nums[3].getIntValue() : 0;
            l.vhi  = nums.size() > 4 ? nums[4].getIntValue() : 127;
            l.grp  = nums.size() > 5 ? nums[5].getIntValue() : 0;
            l.stop = nums.size() > 6 ? jlimit (0, 2, nums[6].getIntValue()) : 0;
            l.file = row.substring (bar + 1).trim().unquoted();
            if (l.file.isNotEmpty())
                lines.push_back (l);
        }
    }
    else
    {
        // fara harta.txt: cate un sunet pe clapa, in ordine alfabetica, de la nota 36
        auto files = dir.findChildFiles (File::findFiles, false, soundPatterns);
        sortNatural (files);
        int note = 36;
        for (auto& f : files)
        {
            if (note > 127)
                break;
            MapLine l;
            l.lo = l.hi = l.root = note++;
            l.file = f.getFileName();
            lines.push_back (l);
        }
    }
    return lines;
}

bool ToyProcessor::writeMap (const File& dir, const std::vector<MapLine>& lines)
{
    String t;
    t << "# Ceamakito's toy: clapa de jos, clapa de sus, clapa pe care suna neschimbat, cat de tare minim, cat de tare maxim, grupa, la ridicarea clapei (0 ca in Setari, 1 se opreste, 2 continua) | fisier\n";
    for (auto& l : lines)
        t << l.lo << " " << l.hi << " " << l.root << " " << l.vlo << " " << l.vhi << " " << l.grp << " " << l.stop << " | " << l.file << "\n";
    return dir.getChildFile ("harta.txt").replaceWithText (t);
}

std::shared_ptr<Instrument> ToyProcessor::build (const File& dir)
{
    auto inst = std::make_shared<Instrument>();
    inst->name = dir.getFileName();
    const auto lines = readMap (dir);

    for (auto& l : lines)
    {
        if (threadShouldExit())
            break;

        File f = File::isAbsolutePath (l.file) ? File (l.file) : dir.getChildFile (l.file);
        std::unique_ptr<AudioFormatReader> reader (formats.createReaderFor (f));
        if (reader == nullptr || reader->lengthInSamples < 2)
            continue;

        const int len = (int) jmin<int64> (reader->lengthInSamples, (int64) 48000 * 60 * 10);
        const int ch = jlimit (1, 2, (int) reader->numChannels);

        Zone z;
        z.lo    = jlimit (0, 127, l.lo);
        z.hi    = jlimit (z.lo, 127, l.hi);
        z.root  = jlimit (0, 127, l.root);
        z.vlo   = jlimit (0, 127, l.vlo);
        z.vhi   = jlimit (z.vlo, 127, l.vhi);
        z.group = jmax (0, l.grp);
        z.stop  = l.stop;
        z.sampleRate = reader->sampleRate > 0 ? reader->sampleRate : 44100.0;
        z.data.setSize (ch, len + 1);
        z.data.clear();
        reader->read (&z.data, 0, len, 0, true, ch > 1);
        inst->zones.push_back (std::move (z));
    }

    inst->id = ++nextId;
    return inst;
}

//==============================================================================
void ToyProcessor::noteOn (int note, int velocity)
{
    if (playing == nullptr)
        return;

    const auto& zs = playing->zones;
    int cand[128];
    int nc = 0;
    for (int i = 0; i < (int) zs.size() && nc < 128; ++i)
    {
        const auto& z = zs[(size_t) i];
        if (note >= z.lo && note <= z.hi && velocity >= z.vlo && velocity <= z.vhi)
            cand[nc++] = i;
    }
    if (nc == 0)
        return;

    const int zi = cand[roundRobin++ % (juce::uint32) nc];
    const auto& z = zs[(size_t) zi];

    if (z.group > 0)
        for (auto& v : voices)
            if (v.active && v.group == z.group)
                v.fading = true;

    Voice* slot = nullptr;
    for (auto& v : voices)
        if (! v.active) { slot = &v; break; }
    if (slot == nullptr)
    {
        slot = &voices[0];
        for (auto& v : voices)
            if (v.age < slot->age)
                slot = &v;
    }

    const float s = sensParam->load();
    slot->active = true;
    slot->fading = false;
    slot->zone   = zi;
    slot->note   = note;
    slot->group  = z.group;
    slot->stop   = z.stop;
    slot->pos    = 0.0;
    slot->step   = z.sampleRate / hostRate * std::pow (2.0, (note - z.root) / 12.0);
    slot->gain   = (1.0f - s) + s * ((float) velocity / 127.0f);
    slot->level  = 1.0f;
    slot->age    = ++ageCounter;
}

void ToyProcessor::noteOff (int note)
{
    const bool general = releaseParam->load() >= 0.5f;
    for (auto& v : voices)
        if (v.active && v.note == note && (v.stop == 1 || (v.stop == 0 && general)))
            v.fading = true;
}

void ToyProcessor::fadeAll()
{
    for (auto& v : voices)
        if (v.active)
            v.fading = true;
}

void ToyProcessor::render (AudioBuffer<float>& out, int start, int end)
{
    if (playing == nullptr || start >= end)
        return;

    float* L = out.getWritePointer (0);
    float* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;

    for (auto& v : voices)
    {
        if (! v.active)
            continue;
        if (v.zone >= (int) playing->zones.size())
        {
            v.active = false;
            continue;
        }

        const auto& z = playing->zones[(size_t) v.zone];
        const int len = z.data.getNumSamples() - 1;
        const float* d0 = z.data.getReadPointer (0);
        const float* d1 = z.data.getNumChannels() > 1 ? z.data.getReadPointer (1) : d0;

        for (int i = start; i < end; ++i)
        {
            const int p = (int) v.pos;
            if (p >= len)
            {
                v.active = false;
                break;
            }
            const float fr = (float) (v.pos - p);
            const float a = d0[p] + fr * (d0[p + 1] - d0[p]);
            const float b = d1[p] + fr * (d1[p + 1] - d1[p]);
            const float g = v.gain * v.level;

            if (R != nullptr) { L[i] += a * g; R[i] += b * g; }
            else              { L[i] += 0.5f * (a + b) * g; }

            if (v.fading)
            {
                v.level -= fadeStep;
                if (v.level <= 0.0f)
                {
                    v.active = false;
                    break;
                }
            }
            v.pos += v.step;
        }
    }
}

void ToyProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    {
        const SpinLock::ScopedTryLockType tl (instLock);
        if (tl.isLocked() && current != playing)
        {
            playing = current;
            for (auto& v : voices)
                v.active = false;
        }
    }

    int pos = 0;
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        const int t = jlimit (0, n, m.samplePosition);
        render (buffer, pos, t);
        pos = t;

        if (msg.isNoteOn())
        {
            noteOn (msg.getNoteNumber(), msg.getVelocity());
            lastNote = msg.getNoteNumber();
            ++noteCount;
        }
        else if (msg.isNoteOff())
            noteOff (msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            fadeAll();
    }
    render (buffer, pos, n);

    buffer.applyGain (Decibels::decibelsToGain (volumeParam->load()));
}

//==============================================================================
void ToyProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("radacina", getRoot().getFullPathName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void ToyProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = ValueTree::fromXml (*xml);
    const String saved = state.getProperty ("radacina").toString();
    apvts.replaceState (state);

    File r = (saved.isNotEmpty() && File::isAbsolutePath (saved) && File (saved).isDirectory())
                 ? File (saved) : readGlobalRoot();
    {
        const ScopedLock sl (listLock);
        root = r;
    }
    rescan();
    requestLoad();
}

AudioProcessorEditor* ToyProcessor::createEditor()
{
    return new ToyEditor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ToyProcessor();
}
