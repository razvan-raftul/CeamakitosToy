#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
// Vorbeste prin NVDA (nvdaControllerClient.dll de langa plugin sau aplicatie); daca NVDA nu merge, prin anuntul de accesibilitate
void speak (const juce::String& text);
