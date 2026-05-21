#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include <cmath>
#include <algorithm>

// Délai modulé (Empêche toute résonance de tuyau statique)
class ModulatedDelay {
public:
    void prepare(double sr, float baseMs, float modHz, float depthMs) {
        sampleRate = sr;
        maxDelay = sr * ((baseMs + depthMs + 10.0f) / 1000.0f);
        buffer.assign(static_cast<size_t>(maxDelay), 0.0f);
        baseDelaySamples = baseMs * sr / 1000.0f;
        modDepthSamples = depthMs * sr / 1000.0f;
        phaseInc = 6.28318530718f * modHz / sr;
        writePos = 0; currentPhase = 0.0f;
    }
    void setPhaseOffset(float p) { currentPhase = p; }
    
    float process(float in) {
        // Sécurité
        if (std::isnan(in) || std::isinf(in)) in = 0.0f;
        in = std::clamp(in, -1.0f, 1.0f);

        buffer[writePos] = in;

        // Le LFO invisible qui "casse" la géométrie du tuyau
        float mod = std::sin(currentPhase) * modDepthSamples;
        currentPhase += phaseInc;
        if (currentPhase > 6.28318530718f) currentPhase -= 6.28318530718f;

        float readPos = static_cast<float>(writePos) - (baseDelaySamples + mod);
        if (readPos < 0.0f) readPos += static_cast<float>(buffer.size());

        int i0 = static_cast<int>(readPos);
        int i1 = (i0 + 1) % buffer.size();
        float frac = readPos - static_cast<float>(i0);
        float out = buffer[i0] + frac * (buffer[i1] - buffer[i0]);

        writePos++;
        if (writePos >= buffer.size()) writePos = 0;
        return out;
    }
private:
    std::vector<float> buffer;
    int writePos = 0;
    float maxDelay = 0.0f, baseDelaySamples = 0.0f, modDepthSamples = 0.0f;
    float currentPhase = 0.0f, phaseInc = 0.0f, sampleRate = 44100.0f;
};

// Filtre d'étouffement organique (absorbe l'air)
class OrganicDamping {
public:
    void setDamping(float damp) { alpha = damp; }
    float process(float in) {
        z1 = in * (1.0f - alpha) + z1 * alpha;
        return z1;
    }
private:
    float z1 = 0.0f, alpha = 0.5f;
};

// Le nuage FDN (Feedback Delay Network) sans résonance
class LushSustainEngine {
public:
    void prepare(double sr) {
        // Temps de délai basés sur des nombres premiers. 
        // Modulés très lentement pour créer la magie.
        d0.prepare(sr, 31.0f, 0.7f, 1.5f); d0.setPhaseOffset(0.0f);
        d1.prepare(sr, 37.0f, 0.9f, 1.5f); d1.setPhaseOffset(1.5f);
        d2.prepare(sr, 43.0f, 1.1f, 1.5f); d2.setPhaseOffset(3.1f);
        d3.prepare(sr, 47.0f, 1.3f, 1.5f); d3.setPhaseOffset(4.7f);
        fb0 = fb1 = fb2 = fb3 = 0.0f;
    }

    void process(float inL, float inR, float& outL, float& outR, float sustain, float damp) {
        damp0.setDamping(damp); damp1.setDamping(damp);
        damp2.setDamping(damp); damp3.setDamping(damp);

        // Injection croisée
        float in0 = inL + fb0;
        float in1 = inR + fb1;
        float in2 = inL - fb2;
        float in3 = inR - fb3;

        float v0 = d0.process(in0);
        float v1 = d1.process(in1);
        float v2 = d2.process(in2);
        float v3 = d3.process(in3);

        // Matrice de Hadamard (Stabilité absolue 4x4)
        float mix0 = (v0 + v1 + v2 + v3) * 0.5f;
        float mix1 = (v0 - v1 + v2 - v3) * 0.5f;
        float mix2 = (v0 + v1 - v2 - v3) * 0.5f;
        float mix3 = (v0 - v1 - v2 + v3) * 0.5f;

        // Boucle de Feedback avec filtre
        fb0 = damp0.process(mix0) * sustain;
        fb1 = damp1.process(mix1) * sustain;
        fb2 = damp2.process(mix2) * sustain;
        fb3 = damp3.process(mix3) * sustain;

        outL = v0 - v2;
        outR = v1 - v3;
    }
private:
    ModulatedDelay d0, d1, d2, d3;
    OrganicDamping damp0, damp1, damp2, damp3;
    float fb0, fb1, fb2, fb3;
};

class NaturalSustainMaster : public juce::AudioProcessor {
public:
    NaturalSustainMaster();
    ~NaturalSustainMaster() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Natural Sustain Master"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
private:
    juce::AudioProcessorValueTreeState apvts;
    LushSustainEngine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NaturalSustainMaster)
};
