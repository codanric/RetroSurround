// PluginProcessor.cpp — createEditor() is implemented here (not inline in
// the header) specifically because it needs PluginEditor.h, which itself
// includes PluginProcessor.h -- keeping the include in the .cpp avoids a
// circular header dependency.
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorEditor* RetroSurroundProcessor::createEditor()
{
    return new RetroSurroundEditor(*this);
}

// Required by JUCE's plugin wrapper code to instantiate the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RetroSurroundProcessor();
}
