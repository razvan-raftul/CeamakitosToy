#include "Speech.h"

#if JUCE_WINDOWS
 #include <windows.h>
#endif

using namespace juce;

namespace
{
#if JUCE_WINDOWS
typedef unsigned long (__stdcall* TestFn)();
typedef unsigned long (__stdcall* SpeakFn) (const wchar_t*);
typedef unsigned long (__stdcall* CancelFn)();

struct Nvda
{
    Nvda()
    {
        auto dir = File::getSpecialLocation (File::currentExecutableFile).getParentDirectory();
        for (auto name : { "nvdaControllerClient.dll", "nvdaControllerClient64.dll" })
        {
            auto f = dir.getChildFile (name);
            if (f.existsAsFile())
            {
                module = LoadLibraryW (f.getFullPathName().toWideCharPointer());
                if (module != nullptr)
                    break;
            }
        }
        if (module != nullptr)
        {
            test   = (TestFn)   GetProcAddress (module, "nvdaController_testIfRunning");
            say    = (SpeakFn)  GetProcAddress (module, "nvdaController_speakText");
            cancel = (CancelFn) GetProcAddress (module, "nvdaController_cancelSpeech");
        }
    }

    bool speak (const String& t)
    {
        if (test == nullptr || say == nullptr || test() != 0)
            return false;
        if (cancel != nullptr)
            cancel();
        return say (t.toWideCharPointer()) == 0;
    }

    HMODULE module = nullptr;
    TestFn test = nullptr;
    SpeakFn say = nullptr;
    CancelFn cancel = nullptr;
};
#endif
}

void speak (const String& text)
{
#if JUCE_WINDOWS
    static Nvda nvda;
    if (nvda.speak (text))
        return;
#endif
    AccessibilityHandler::postAnnouncement (text, AccessibilityHandler::AnnouncementPriority::high);
}
