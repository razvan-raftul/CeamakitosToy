#include "Updater.h"
#include "PluginProcessor.h"
#include "Speech.h"

using namespace juce;

namespace
{
const char* releasesUrl = "https://api.github.com/repos/razvan-raftul/CeamakitosToy/releases/latest";
const char* headers = "User-Agent: CeamakitosToy\r\nAccept: application/vnd.github+json";

bool isNewer (const String& remote, const String& local)
{
    auto a = StringArray::fromTokens (remote.trimCharactersAtStart ("vV"), ".", "");
    auto b = StringArray::fromTokens (local, ".", "");
    for (int i = 0; i < jmax (a.size(), b.size()); ++i)
    {
        const int x = a[i].getIntValue(), y = b[i].getIntValue();
        if (x != y)
            return x > y;
    }
    return false;
}

bool reaperIsOpen()
{
    ChildProcess p;
    if (! p.start ("tasklist /FI \"IMAGENAME eq reaper.exe\" /NH"))
        return false;
    return p.readAllProcessOutput().containsIgnoreCase ("reaper.exe");
}

void say (const String& s) { speak (s); }

class UpdateContent : public Component
{
public:
    explicit UpdateContent (double& p) : bar (p)
    {
        bar.setPercentageDisplay (true);
        bar.setTitle ("Actualizare");
        bar.setWantsKeyboardFocus (true);
        addAndMakeVisible (bar);
        setSize (360, 70);
    }
    void resized() override { bar.setBounds (getLocalBounds().reduced (14)); }
    ProgressBar bar;
};

class UpdateWindow : public DocumentWindow
{
public:
    explicit UpdateWindow (double& p)
        : DocumentWindow ("Ceamakito's toy", Colour (0xff20242c), 0)
    {
        content = new UpdateContent (p);
        setContentOwned (content, true);
        setUsingNativeTitleBar (true);
        centreWithSize (getWidth(), getHeight());
        setAlwaysOnTop (true);
        setVisible (true);
        toFront (true);
        content->bar.grabKeyboardFocus();
    }
    void closeButtonPressed() override {}
    UpdateContent* content = nullptr;
};

class Updater : public Thread, private Timer
{
public:
    Updater() : Thread ("Ceamakito update") { startThread(); startTimer (100); }
    ~Updater() override { signalThreadShouldExit(); stopThread (5000); }

    void run() override
    {
        URL api (releasesUrl);
        auto stream = api.createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                                                 .withExtraHeaders (headers)
                                                 .withConnectionTimeoutMs (10000)
                                                 .withNumRedirectsToFollow (5));
        if (stream == nullptr)
            return finish (State::nothing);

        auto json = JSON::parse (stream->readEntireStreamAsString());
        const String tag = json["tag_name"].toString();
        if (tag.isEmpty() || ! isNewer (tag, JucePlugin_VersionString))
            return finish (State::nothing);

        String assetUrl;
        if (auto* assets = json["assets"].getArray())
            for (auto& a : *assets)
                if (a["name"].toString().endsWithIgnoreCase (".exe"))
                    assetUrl = a["browser_download_url"].toString();
        if (assetUrl.isEmpty())
            return finish (State::nothing);

        if (reaperIsOpen())
            return finish (State::reaperOpen);

        state = State::downloading;

        target = File::getSpecialLocation (File::tempDirectory).getChildFile ("Instaleaza Ceamakitos toy.exe");
        target.deleteFile();

        auto dl = URL (assetUrl).createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                                                        .withExtraHeaders ("User-Agent: CeamakitosToy")
                                                        .withConnectionTimeoutMs (15000)
                                                        .withNumRedirectsToFollow (10));
        if (dl == nullptr)
            return finish (State::failed);

        const int64 total = dl->getTotalLength();
        int64 done = 0;
        {
            FileOutputStream out (target);
            if (! out.openedOk())
                return finish (State::failed);
            HeapBlock<char> buf (65536);
            while (! threadShouldExit())
            {
                const int n = dl->read (buf, 65536);
                if (n <= 0)
                    break;
                out.write (buf, (size_t) n);
                done += n;
                if (total > 0)
                    progress = (double) done / (double) total;
            }
            out.flush();
        }
        if (threadShouldExit() || done < 100000 || (total > 0 && done < total))
            return finish (State::failed);

        progress = 1.0;
        finish (State::ready);
    }

private:
    enum class State { checking, downloading, nothing, reaperOpen, failed, ready };

    void finish (State s) { state = s; }

    void timerCallback() override
    {
        const State s = state.load();
        if (s == State::downloading || s == State::ready)
        {
            if (window == nullptr)
                window = std::make_unique<UpdateWindow> (shownProgress);
            shownProgress = progress.load();
            const int pct = (int) (shownProgress * 100.0);
            if (pct / 10 != lastTen)
            {
                lastTen = pct / 10;
                say (String (lastTen * 10) + "%");
            }
        }

        if (s == State::checking || s == State::downloading)
            return;

        stopTimer();
        if (s == State::reaperOpen)
            say (ro ("Există o versiune nouă de Ceamakito's toy. Închide REAPER și deschide din nou aplicația, ca să se actualizeze."));

        if (s == State::ready)
        {
            if (target.startAsProcess ("/VERYSILENT /SUPPRESSMSGBOXES /NORESTART"))
            {
                if (auto* app = JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
                return;
            }
        }
        window.reset();
    }

    std::atomic<State> state { State::checking };
    std::atomic<double> progress { 0.0 };
    double shownProgress = 0.0;
    int lastTen = -1;
    File target;
    std::unique_ptr<UpdateWindow> window;
};

std::unique_ptr<Updater> updater;
} // namespace

void startUpdateCheck()
{
    static bool started = false;
    if (started)
        return;
    started = true;
    updater = std::make_unique<Updater>();
}

void stopUpdateCheck()
{
    updater.reset();
}
