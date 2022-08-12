/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
KopczynskiXTCAudioProcessor::KopczynskiXTCAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

KopczynskiXTCAudioProcessor::~KopczynskiXTCAudioProcessor()
{
}

//==============================================================================
const juce::String KopczynskiXTCAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool KopczynskiXTCAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool KopczynskiXTCAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool KopczynskiXTCAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double KopczynskiXTCAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int KopczynskiXTCAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int KopczynskiXTCAudioProcessor::getCurrentProgram()
{
    return 0;
}

void KopczynskiXTCAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String KopczynskiXTCAudioProcessor::getProgramName (int index)
{
    return {};
}

void KopczynskiXTCAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void KopczynskiXTCAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    
    /* prepare dsp chains for processing */
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;
    
    leftBPChain.prepare(spec);
    rightBPChain.prepare(spec);
    
    leftRecChain.prepare(spec);
    rightRecChain.prepare(spec);
    
    leftHPChain.prepare(spec);
    rightHPChain.prepare(spec);
    
    leftLPChain.prepare(spec);
    rightLPChain.prepare(spec);
    
    /* set initial attenuation, delay, and filteer coefficients/slopes */
    updateAll();
    
}

void KopczynskiXTCAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    leftBPChain.reset();
    rightBPChain.reset();
    
    leftRecChain.reset();
    rightRecChain.reset();
    
    leftHPChain.reset();
    rightHPChain.reset();
    
    leftLPChain.reset();
    rightLPChain.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool KopczynskiXTCAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void KopczynskiXTCAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    int bufferSize = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    /* update attenuation, delay, and filter slope */
    updateAll();
    
    /* copy buffer into temporary buffers to be processed */
    juce::AudioBuffer<float> bpBuffer;
    bpBuffer.makeCopyOf(buffer);
    
    juce::AudioBuffer<float> lpBuffer;
    lpBuffer.makeCopyOf(buffer);
    
    juce::AudioBuffer<float> hpBuffer;
    hpBuffer.makeCopyOf(buffer);
    
    /* create bandpass audioblock and process */
    juce::dsp::AudioBlock<float> bpBlock(bpBuffer);
    
    auto leftBPBlock = bpBlock.getSingleChannelBlock(LEFT_CHANNEL);
    auto rightBPBlock = bpBlock.getSingleChannelBlock(RIGHT_CHANNEL);
    
    juce::dsp::ProcessContextReplacing<float> leftBPContext(leftBPBlock);
    juce::dsp::ProcessContextReplacing<float> rightBPContext(rightBPBlock);
    
    leftBPChain.process(leftBPContext);
    rightBPChain.process(rightBPContext);
    
    /* iterate processing multiple times */
    for (int i = 0; i < 40; ++i)
    {
        leftRecChain.process(rightBPContext);
        rightRecChain.process(leftBPContext);
    }
    
    
    /* copy LP buffer into audioblock to be processed */
    juce::dsp::AudioBlock<float> lpBlock(lpBuffer);
    
    auto leftLPBlock = lpBlock.getSingleChannelBlock(LEFT_CHANNEL);
    auto rightLPBlock = lpBlock.getSingleChannelBlock(RIGHT_CHANNEL);
    
    juce::dsp::ProcessContextReplacing<float> leftLPContext(leftLPBlock);
    juce::dsp::ProcessContextReplacing<float> rightLPContext(rightLPBlock);
    
    leftLPChain.process(leftLPContext);
    rightLPChain.process(rightLPContext);
    
    /* copy HP buffer into audioblock to be processed */
    juce::dsp::AudioBlock<float> hpBlock(hpBuffer);
    
    auto leftHPBlock = hpBlock.getSingleChannelBlock(LEFT_CHANNEL);
    auto rightHPBlock = hpBlock.getSingleChannelBlock(RIGHT_CHANNEL);
    
    juce::dsp::ProcessContextReplacing<float> leftHPContext(leftHPBlock);
    juce::dsp::ProcessContextReplacing<float> rightHPContext(rightHPBlock);
    
    leftHPChain.process(leftHPContext);
    rightHPChain.process(rightHPContext);
    
    
    /* fill original buffer with bandpassed buffer */
    buffer.copyFrom(LEFT_CHANNEL, 0, bpBuffer, LEFT_CHANNEL, 0, bufferSize);
    buffer.copyFrom(RIGHT_CHANNEL, 0, bpBuffer, RIGHT_CHANNEL, 0, bufferSize);
    
    /* add low-passed signal */
    buffer.addFrom(LEFT_CHANNEL, 0, lpBuffer, LEFT_CHANNEL, 0, bufferSize);
    buffer.addFrom(RIGHT_CHANNEL, 0, lpBuffer, RIGHT_CHANNEL, 0, bufferSize);
    
    /* add high-passed signal */
    buffer.addFrom(LEFT_CHANNEL, 0, hpBuffer, LEFT_CHANNEL, 0, bufferSize);
    buffer.addFrom(RIGHT_CHANNEL, 0, hpBuffer, RIGHT_CHANNEL, 0, bufferSize);
}

void KopczynskiXTCAudioProcessor::updateCoefficients (Coefficients& old, const Coefficients& replacements)
{
    *old = *replacements;
}

void KopczynskiXTCAudioProcessor::updateFilters(const ChainSettings &chainSettings)
{
    auto bpHPCoeff = juce::dsp::IIR::Coefficients<float>::makeHighPass(getSampleRate(), 250.f);
    auto bpLPCoeff = juce::dsp::IIR::Coefficients<float>::makeLowPass(getSampleRate(), 5000.f);
    auto hpCoeff = juce::dsp::IIR::Coefficients<float>::makeHighPass(getSampleRate(), 250.f);
    auto lpCoeff = juce::dsp::IIR::Coefficients<float>::makeLowPass(getSampleRate(), 5000.f);
    
    auto& leftHighPass = leftBPChain.get<BPChainPositions::BPHighPass>();
    
    leftHighPass.setBypassed<0>(true);
    leftHighPass.setBypassed<1>(true);
    leftHighPass.setBypassed<2>(true);
    
    auto& rightHighPass = rightBPChain.get<BPChainPositions::BPHighPass>();
    
    rightHighPass.setBypassed<0>(true);
    rightHighPass.setBypassed<1>(true);
    rightHighPass.setBypassed<2>(true);
    
    auto& leftLowPass = leftBPChain.get<BPChainPositions::BPLowPass>();
    
    leftLowPass.setBypassed<0>(true);
    leftLowPass.setBypassed<1>(true);
    leftLowPass.setBypassed<2>(true);
    
    auto& rightLowPass = rightBPChain.get<BPChainPositions::BPLowPass>();
    
    rightLowPass.setBypassed<0>(true);
    rightLowPass.setBypassed<1>(true);
    rightLowPass.setBypassed<2>(true);
    
    leftHPChain.setBypassed<0>(true);
    leftHPChain.setBypassed<1>(true);
    leftHPChain.setBypassed<2>(true);
    
    rightHPChain.setBypassed<0>(true);
    rightHPChain.setBypassed<1>(true);
    rightHPChain.setBypassed<2>(true);
    
    leftLPChain.setBypassed<0>(true);
    leftLPChain.setBypassed<1>(true);
    leftLPChain.setBypassed<2>(true);
    
    rightLPChain.setBypassed<0>(true);
    rightLPChain.setBypassed<1>(true);
    rightLPChain.setBypassed<2>(true);
    
    switch ( chainSettings.filterType )
    {
        case 0:
        {
            updateCoefficients(leftHighPass.get<0>().coefficients, bpHPCoeff);
            leftHighPass.setBypassed<0>(false);
            
            updateCoefficients(rightHighPass.get<0>().coefficients, bpHPCoeff);
            rightHighPass.setBypassed<0>(false);
            
            updateCoefficients(leftLowPass.get<0>().coefficients, bpLPCoeff);
            leftLowPass.setBypassed<0>(false);
            
            updateCoefficients(rightLowPass.get<0>().coefficients, bpLPCoeff);
            rightLowPass.setBypassed<0>(false);
            
            updateCoefficients(leftHPChain.get<0>().coefficients, hpCoeff);
            leftHPChain.setBypassed<0>(false);
            
            updateCoefficients(rightHPChain.get<0>().coefficients, hpCoeff);
            rightHPChain.setBypassed<0>(false);
            
            updateCoefficients(leftLPChain.get<0>().coefficients, lpCoeff);
            leftLPChain.setBypassed<0>(false);
            
            updateCoefficients(rightLPChain.get<0>().coefficients, lpCoeff);
            rightLPChain.setBypassed<0>(false);
            
            break;
        }
            
        case 1:
        {
            updateCoefficients(leftHighPass.get<0>().coefficients, bpHPCoeff);
            leftHighPass.setBypassed<0>(false);
            updateCoefficients(leftHighPass.get<1>().coefficients, bpHPCoeff);
            leftHighPass.setBypassed<1>(false);
            
            updateCoefficients(rightHighPass.get<0>().coefficients, bpHPCoeff);
            rightHighPass.setBypassed<0>(false);
            updateCoefficients(rightHighPass.get<1>().coefficients, bpHPCoeff);
            rightHighPass.setBypassed<1>(false);
            
            updateCoefficients(leftLowPass.get<0>().coefficients, bpLPCoeff);
            leftLowPass.setBypassed<0>(false);
            updateCoefficients(leftLowPass.get<1>().coefficients, bpLPCoeff);
            leftLowPass.setBypassed<1>(false);
            
            updateCoefficients(rightLowPass.get<0>().coefficients, bpLPCoeff);
            rightLowPass.setBypassed<0>(false);
            updateCoefficients(rightLowPass.get<1>().coefficients, bpLPCoeff);
            rightLowPass.setBypassed<1>(false);
            
            updateCoefficients(leftHPChain.get<0>().coefficients, hpCoeff);
            leftHPChain.setBypassed<0>(false);
            updateCoefficients(leftHPChain.get<1>().coefficients, hpCoeff);
            leftHPChain.setBypassed<1>(false);
            
            updateCoefficients(rightHPChain.get<0>().coefficients, hpCoeff);
            rightHPChain.setBypassed<0>(false);
            updateCoefficients(rightHPChain.get<1>().coefficients, hpCoeff);
            rightHPChain.setBypassed<1>(false);
            
            updateCoefficients(leftLPChain.get<0>().coefficients, lpCoeff);
            leftLPChain.setBypassed<0>(false);
            updateCoefficients(leftLPChain.get<1>().coefficients, lpCoeff);
            leftLPChain.setBypassed<1>(false);
            
            updateCoefficients(rightLPChain.get<0>().coefficients, lpCoeff);
            rightLPChain.setBypassed<0>(false);
            updateCoefficients(rightLPChain.get<1>().coefficients, lpCoeff);
            rightLPChain.setBypassed<1>(false);
            
            break;
        }
            
        case 2:
        {
            updateCoefficients(leftHighPass.get<0>().coefficients, bpHPCoeff);
            leftHighPass.setBypassed<0>(false);
            updateCoefficients(leftHighPass.get<1>().coefficients, bpHPCoeff);
            leftHighPass.setBypassed<1>(false);
            updateCoefficients(leftHighPass.get<2>().coefficients, bpHPCoeff);
            leftHighPass.setBypassed<2>(false);
            
            updateCoefficients(rightHighPass.get<0>().coefficients, bpHPCoeff);
            rightHighPass.setBypassed<0>(false);
            updateCoefficients(rightHighPass.get<1>().coefficients, bpHPCoeff);
            rightHighPass.setBypassed<1>(false);
            updateCoefficients(rightHighPass.get<2>().coefficients, bpHPCoeff);
            rightHighPass.setBypassed<3>(false);
            
            updateCoefficients(leftLowPass.get<0>().coefficients, bpLPCoeff);
            leftLowPass.setBypassed<0>(false);
            updateCoefficients(leftLowPass.get<1>().coefficients, bpLPCoeff);
            leftLowPass.setBypassed<1>(false);
            updateCoefficients(leftLowPass.get<2>().coefficients, bpLPCoeff);
            leftLowPass.setBypassed<2>(false);
            
            updateCoefficients(rightLowPass.get<0>().coefficients, bpLPCoeff);
            rightLowPass.setBypassed<0>(false);
            updateCoefficients(rightLowPass.get<1>().coefficients, bpLPCoeff);
            rightLowPass.setBypassed<1>(false);
            updateCoefficients(rightLowPass.get<2>().coefficients, bpLPCoeff);
            rightLowPass.setBypassed<2>(false);
            
            updateCoefficients(leftHPChain.get<0>().coefficients, hpCoeff);
            leftHPChain.setBypassed<0>(false);
            updateCoefficients(leftHPChain.get<1>().coefficients, hpCoeff);
            leftHPChain.setBypassed<1>(false);
            updateCoefficients(leftHPChain.get<2>().coefficients, hpCoeff);
            leftHPChain.setBypassed<2>(false);
            
            updateCoefficients(rightHPChain.get<0>().coefficients, hpCoeff);
            rightHPChain.setBypassed<0>(false);
            updateCoefficients(rightHPChain.get<1>().coefficients, hpCoeff);
            rightHPChain.setBypassed<1>(false);
            updateCoefficients(rightHPChain.get<2>().coefficients, hpCoeff);
            rightHPChain.setBypassed<2>(false);
            
            updateCoefficients(leftLPChain.get<0>().coefficients, lpCoeff);
            leftLPChain.setBypassed<0>(false);
            updateCoefficients(leftLPChain.get<1>().coefficients, lpCoeff);
            leftLPChain.setBypassed<1>(false);
            updateCoefficients(leftLPChain.get<2>().coefficients, lpCoeff);
            leftLPChain.setBypassed<2>(false);
            
            updateCoefficients(rightLPChain.get<0>().coefficients, lpCoeff);
            rightLPChain.setBypassed<0>(false);
            updateCoefficients(rightLPChain.get<1>().coefficients, lpCoeff);
            rightLPChain.setBypassed<1>(false);
            updateCoefficients(rightLPChain.get<2>().coefficients, lpCoeff);
            rightLPChain.setBypassed<2>(false);
            
            break;
        }
    }
}

void KopczynskiXTCAudioProcessor::updateAttenuation(const ChainSettings &chainSettings)
{
    float gainLin = juce::Decibels::decibelsToGain(chainSettings.attenuation);
    
    leftRecChain.get<RecChainPositions::Attenuation>().setGainLinear(-gainLin);
    rightRecChain.get<RecChainPositions::Attenuation>().setGainLinear(-gainLin);
}

void KopczynskiXTCAudioProcessor::updateDelay(const ChainSettings &chainSettings)
{
    leftRecChain.get<RecChainPositions::Delay>().setDelay(chainSettings.delay * 0.001f * getSampleRate());
    rightRecChain.get<RecChainPositions::Delay>().setDelay(chainSettings.delay * 0.001f * getSampleRate());
}

void KopczynskiXTCAudioProcessor::updateAll()
{
    auto chainSettings = getChainSettings(apvts);
    
    updateAttenuation(chainSettings);
    updateDelay(chainSettings);
    updateFilters(chainSettings);
}

//==============================================================================
bool KopczynskiXTCAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* KopczynskiXTCAudioProcessor::createEditor()
{
//    return new KopczynskiXTCAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void KopczynskiXTCAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void KopczynskiXTCAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;
    
    settings.attenuation = apvts.getRawParameterValue("Attenuation")->load();
    settings.delay = apvts.getRawParameterValue("Delay")->load();
    settings.filterType = apvts.getRawParameterValue("Filter Type")->load();
    
    return settings;
}

juce::AudioProcessorValueTreeState::ParameterLayout
    KopczynskiXTCAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Attenuation",
                                                           "Attenuation",
                                                           juce::NormalisableRange<float>(-4.f, -2.f, 0.01f, 1.f),
                                                           -3.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Delay",
                                                           "Delay",
                                                           juce::NormalisableRange<float>(0.06f, 0.1f, 0.001f, 1.f),
                                                           0.06f));
    
    juce::StringArray stringArray;
    for (int i = 0; i < 3; ++i)
    {
        juce::String str;
        str << (12 + i*12);
        str << " dB/Oct";
        stringArray.add(str);
    }
    
    layout.add(std::make_unique<juce::AudioParameterChoice>("Filter Type", "Filter Type", stringArray, 0));
    
    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KopczynskiXTCAudioProcessor();
}
