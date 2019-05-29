/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             SKlavier
 version:          2.0.1
 vendor:           JUCE
 website:          http://juce.com
 description:      Synthesiser with midi input.

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
				   juce_audio_processors, juce_audio_utils, juce_core,
				   juce_data_structures, juce_events, juce_graphics,
				   juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2017, linux_make

 type:             Component
 mainClass:        MainContentComponent

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/


#pragma once

//==============================================================================
struct SineWaveSound : public SynthesiserSound
{
	SineWaveSound() {}

	bool appliesToNote(int) override { return true; }
	bool appliesToChannel(int) override { return true; }
};

//==============================================================================
struct SineWaveVoice : public SynthesiserVoice
{
	SineWaveVoice() {}

	bool canPlaySound(SynthesiserSound* sound) override
	{
		return dynamic_cast<SineWaveSound*> (sound) != nullptr;
	}

	void startNote(int midiNoteNumber, float velocity,
		SynthesiserSound*, int currentPitchWheelPosition) override
	{
		currentAngle = 0.0; //phase
		level = velocity * 0.15;
		tailOff = 0.0;

		auto cyclesPerSecond = MidiMessage::getMidiNoteInHertz(midiNoteNumber)/*+(currentPitchWheelPosition/128)*/; //frequency
		auto cyclesPerSample = cyclesPerSecond / getSampleRate();

		angleDelta = cyclesPerSample * 2.0 * MathConstants<double>::pi; //increment
	}

	void stopNote(float /*velocity*/, bool allowTailOff) override
	{
		if (allowTailOff)
		{
			if (tailOff == 0.0)
				tailOff = 1.0;
		}
		else
		{
			clearCurrentNote();
			angleDelta = 0.0;
		}
	}

	void pitchWheelMoved(int) override {}
	void controllerMoved(int, int) override {}

	void renderNextBlock(AudioSampleBuffer & outputBuffer, int startSample, int numSamples) override
	{
		if (angleDelta != 0.0)
		{
			if (tailOff > 0.0) // when note is depressed
			{
				while (--numSamples >= 0)
				{
					auto currentSample = (float)((std::sin(currentAngle) * level * tailOff) + (std::sin(2 * currentAngle) * level * tailOff / 2) + (std::sin(3 * currentAngle) * level * tailOff / 5) + (std::sin(4 * currentAngle) * level * tailOff / 10) + (std::sin(5 * currentAngle) * level * tailOff / 50) + (std::sin(6 * currentAngle) * level * tailOff / 80) + (std::sin(7 * currentAngle) * level * tailOff / 100));

					for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
						outputBuffer.addSample(i, startSample, currentSample);

					currentAngle += angleDelta;
					++startSample;

					tailOff *= 0.99955; // damping

					if (tailOff <= 0.00045)
					{
						clearCurrentNote(); // erasing quiet notes

						angleDelta = 0.0;
						break;
					}
				}
			}
			else
			{
				while (--numSamples >= 0) // that is happening when note is pressed
				{


					auto currentSample = (float)((std::sin(currentAngle) * level)+(std::sin(2*currentAngle) * level/2)+(std::sin(3 * currentAngle) * level / 5) + (std::sin(4 * currentAngle) * level / 10) + (std::sin(5 * currentAngle) * level / 50) + (std::sin(6 * currentAngle) * level / 80) + (std::sin(7 * currentAngle) * level / 100));

					for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
						outputBuffer.addSample(i, startSample, currentSample);

					currentAngle += angleDelta;
					++startSample;
				
				}
			}
		}
	}

private:
	double currentAngle = 0.0, angleDelta = 0.0, level = 0.0, tailOff = 0.0;
};

//==============================================================================
class SynthAudioSource : public AudioSource
{
public:
	SynthAudioSource(MidiKeyboardState& keyState)
		: keyboardState(keyState)
	{
		for (auto i = 0; i < 4; ++i)
			synth.addVoice(new SineWaveVoice());

		synth.addSound(new SineWaveSound());
	}

	void setUsingSineWaveSound()
	{
		synth.clearSounds();
	}

	void prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) override
	{
		synth.setCurrentPlaybackSampleRate(sampleRate);
		midiCollector.reset(sampleRate); // [10]
	}

	void releaseResources() override {}

	void getNextAudioBlock(const AudioSourceChannelInfo & bufferToFill) override
	{
		bufferToFill.clearActiveBufferRegion();

		MidiBuffer incomingMidi;
		midiCollector.removeNextBlockOfMessages(incomingMidi, bufferToFill.numSamples); // [11]

		keyboardState.processNextMidiBuffer(incomingMidi, bufferToFill.startSample,
			bufferToFill.numSamples, true);

		synth.renderNextBlock(*bufferToFill.buffer, incomingMidi,
			bufferToFill.startSample, bufferToFill.numSamples);
	}

	MidiMessageCollector* getMidiCollector()
	{
		return &midiCollector;
	}

private:
	MidiKeyboardState& keyboardState;
	Synthesiser synth;
	MidiMessageCollector midiCollector;
};

//==============================================================================
class MainContentComponent : public AudioAppComponent,
	private Timer
{
public:
	MainContentComponent()
		: synthAudioSource(keyboardState),
		keyboardComponent(keyboardState, MidiKeyboardComponent::horizontalKeyboard)
	{
		addAndMakeVisible(midiInputListLabel);
		midiInputListLabel.setText("MIDI Input:", dontSendNotification);
		midiInputListLabel.attachToComponent(&midiInputList, true);

		auto midiInputs = MidiInput::getDevices();
		addAndMakeVisible(midiInputList);
		midiInputList.setTextWhenNoChoicesAvailable("No MIDI Inputs Enabled");
		midiInputList.addItemList(midiInputs, 1);
		midiInputList.onChange = [this] { setMidiInput(midiInputList.getSelectedItemIndex()); };

		for (auto midiInput : midiInputs)
		{
			if (deviceManager.isMidiInputEnabled(midiInput))
			{
				setMidiInput(midiInputs.indexOf(midiInput));
				break;
			}
		}

		if (midiInputList.getSelectedId() == 0)
			setMidiInput(0);

		addAndMakeVisible(keyboardComponent);
		setAudioChannels(0, 2);

		setSize(600, 190);
		startTimer(400);
	}

	~MainContentComponent()
	{
		shutdownAudio();
	}

	void resized() override
	{
		midiInputList.setBounds(200, 10, getWidth() - 210, 20);
		keyboardComponent.setBounds(10, 40, getWidth() - 20, getHeight() - 50);
	}

	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
	{
		synthAudioSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
	}

	void getNextAudioBlock(const AudioSourceChannelInfo & bufferToFill) override
	{
		synthAudioSource.getNextAudioBlock(bufferToFill);
	}

	void releaseResources() override
	{
		synthAudioSource.releaseResources();
	}

private:
	void timerCallback() override
	{
		keyboardComponent.grabKeyboardFocus();
		stopTimer();
	}

	void setMidiInput(int index)
	{
		auto list = MidiInput::getDevices();

		deviceManager.removeMidiInputCallback(list[lastInputIndex], synthAudioSource.getMidiCollector());

		auto newInput = list[index];

		if (!deviceManager.isMidiInputEnabled(newInput))
			deviceManager.setMidiInputEnabled(newInput, true);

		deviceManager.addMidiInputCallback(newInput, synthAudioSource.getMidiCollector());
		midiInputList.setSelectedId(index + 1, dontSendNotification);

		lastInputIndex = index;
	}

	//==========================================================================
	SynthAudioSource synthAudioSource;
	MidiKeyboardState keyboardState;
	MidiKeyboardComponent keyboardComponent;

	ComboBox midiInputList;
	Label midiInputListLabel;
	int lastInputIndex = 0;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};