/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#define LEFT_CHANNEL    0
#define RIGHT_CHANNEL   1

struct ChainSettings
{
    float attenuation { 0 };
    float delay { 0 };
    int filterType { 0 };
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

//==============================================================================
/**
*/
class KopczynskiXTCAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    KopczynskiXTCAudioProcessor();
    ~KopczynskiXTCAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts {*this, nullptr, "Parameters", createParameterLayout()};

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Gain = juce::dsp::Gain<float>;
    using DelayLine = juce::dsp::DelayLine<float>;
    
    using RecChain = juce::dsp::ProcessorChain<Gain, DelayLine>;
    using CutChain = juce::dsp::ProcessorChain<Filter, Filter, Filter>;
    using BPChain = juce::dsp::ProcessorChain<CutChain, CutChain>;
    
    BPChain leftBPChain, rightBPChain;
    
    RecChain leftRecChain, rightRecChain;
    
    CutChain leftHPChain, rightHPChain, leftLPChain, rightLPChain;
    
    enum BPChainPositions
    {
        BPLowPass,
        BPHighPass
    };
    
    enum RecChainPositions
    {
        Attenuation,
        Delay
    };
    
    enum CutChainPositions
    {
        Filter1,
        Filter2,
        Filter3
    };
    
    enum FilterTypes
    {
        FirstOrder,
        SecondOrder,
        ThirdOrder
    };
    
    using Coefficients = Filter::CoefficientsPtr;
    
    void updateFilters (const ChainSettings& chainSettings);
    static void updateCoefficients (Coefficients& old, const Coefficients& replacements);
    void updateAttenuation (const ChainSettings& chainSettings);
    void updateDelay (const ChainSettings& chainSettings);
    void updateAll();
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KopczynskiXTCAudioProcessor)
};
