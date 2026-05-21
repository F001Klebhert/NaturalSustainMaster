#include "PluginProcessor.h"
#include <memory>

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    // Le volume de la brume harmonique
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("amount", 1), "Sustain Amount", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));
        
    // La durée de vie de la note (Feedback)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("decay", 1), "Fade Length", 
        juce::NormalisableRange<float>(0.1f, 0.95f, 0.01f), 0.85f));
        
    // La chaleur du son (Damping) : Haut = très sombre, Bas = brillant
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("warmth", 1), "Warmth (Damping)", 
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f), 0.60f));
        
    return layout;
}

NaturalSustainMaster::NaturalSustainMaster()
    : juce::AudioProcessor(juce::AudioProcessor::BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

void NaturalSustainMaster::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine.prepare(sampleRate);
}

void NaturalSustainMaster::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    int numSamples = buffer.getNumSamples();
    auto* channelDataL = buffer.getWritePointer(0);
    auto* channelDataR = buffer.getWritePointer(1);

    float amt = apvts.getRawParameterValue("amount")->load();
    float decay = apvts.getRawParameterValue("decay")->load();
    float warmth = apvts.getRawParameterValue("warmth")->load();

    for (int i = 0; i < numSamples; ++i) {
        float inL = channelDataL[i];
        float inR = channelDataR[i];
        float susL = 0.0f, susR = 0.0f;

        // Le moteur génère le nuage (Le son d'origine ne passe jamais dedans)
        engine.process(inL, inR, susL, susR, decay, warmth);

        // Mixage : Le Timbre Dry (inL/inR) est 100% conservé !
        channelDataL[i] = inL + (susL * amt);
        channelDataR[i] = inR + (susR * amt);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new NaturalSustainMaster();
}
