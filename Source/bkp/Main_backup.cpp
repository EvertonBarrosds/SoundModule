
#include <JuceHeader.h>
#include "FluidSynthEngine.h"

class MainComponent : public juce::AudioAppComponent,
                      public juce::MidiInputCallback
{
public:
    MainComponent()
    {
        setSize(1000, 650);

        // Botão LOAD SF2
        addAndMakeVisible(loadButton);
        loadButton.setBounds(760, 130, 180, 45);

        loadButton.onClick = [this]()
        {
            fileChooser = std::make_unique<juce::FileChooser>(
                "Selecione um SoundFont",
                juce::File{},
                "*.sf2"
            );

            fileChooser->launchAsync(
                juce::FileBrowserComponent::openMode |
                juce::FileBrowserComponent::canSelectFiles,

                [this](const juce::FileChooser& chooser)
                {
                    const auto file = chooser.getResult();

                    if (file.existsAsFile())
                    {
                        const auto path = file.getFullPathName();

                        bool loaded = false;

                        {
                            const juce::ScopedLock lock(engineLock);
                            loaded = engine.loadSoundFont(
                                path.toRawUTF8()
                            );
                        }

                        if (loaded)
                            currentInstrumentName =
                                file.getFileNameWithoutExtension();
                        else
                            currentInstrumentName =
                                "Falha ao carregar SF2";

                        repaint();
                    }
                }
            );
        };

        // Seleção de dispositivo MIDI
        addAndMakeVisible(midiInputSelector);

        midiDevices = juce::MidiInput::getAvailableDevices();

        for (int i = 0; i < midiDevices.size(); ++i)
        {
            midiInputSelector.addItem(
                midiDevices[i].name,
                i + 1
            );
        }

        if (midiDevices.isEmpty())
        {
            midiInputSelector.addItem(
                "Nenhum MIDI encontrado",
                1
            );

            midiInputSelector.setEnabled(false);
            midiStatusText = "MIDI: NO DEVICE";
        }
        else
        {
            midiInputSelector.setSelectedId(
                1,
                juce::dontSendNotification
            );

            midiInputSelector.onChange = [this]()
            {
                openMidiInput(
                    midiInputSelector.getSelectedId()
                );
            };
        }

        midiInputSelector.setBounds(760, 185, 180, 30);

        // Inicializa saída de áudio estéreo
        setAudioChannels(0, 2);

        // Abre automaticamente o primeiro dispositivo MIDI
        if (!midiDevices.isEmpty())
            openMidiInput(1);
    }

    ~MainComponent() override
    {
        if (midiInput != nullptr)
        {
            midiInput->stop();
            midiInput.reset();
        }

        shutdownAudio();
    }

    // Abre dispositivo MIDI selecionado
    void openMidiInput(int selectedId)
    {
        if (midiInput != nullptr)
        {
            midiInput->stop();
            midiInput.reset();
        }

        int index = selectedId - 1;

        if (index < 0 || index >= midiDevices.size())
            return;

        midiInput = juce::MidiInput::openDevice(
            midiDevices[index].identifier,
            this
        );

        if (midiInput != nullptr)
        {
            midiInput->start();
            midiStatusText = "MIDI: CONNECTED";
        }
        else
        {
            midiStatusText = "MIDI: ERROR";
        }

        repaint();
    }

    // Recebe mensagens MIDI
    void handleIncomingMidiMessage(
        juce::MidiInput* source,
        const juce::MidiMessage& message) override
    {
        juce::ignoreUnused(source);

        const juce::ScopedLock lock(engineLock);

        if (message.isNoteOn())
        {
            engine.noteOn(
                message.getChannel() - 1,
                message.getNoteNumber(),
                message.getVelocity()
            );
        }
        else if (message.isNoteOff())
        {
            engine.noteOff(
                message.getChannel() - 1,
                message.getNoteNumber()
            );
        }
    }

    // Inicialização do motor de áudio
    void prepareToPlay(
        int samplesPerBlockExpected,
        double sampleRate) override
    {
        juce::ignoreUnused(samplesPerBlockExpected);

        const juce::ScopedLock lock(engineLock);
        engine.initialize(sampleRate);
    }

    // Processamento de áudio
    void getNextAudioBlock(
        const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        auto* buffer = bufferToFill.buffer;

        if (buffer == nullptr)
            return;

        buffer->clear(
            bufferToFill.startSample,
            bufferToFill.numSamples
        );

        if (buffer->getNumChannels() >= 2)
        {
            const juce::ScopedLock lock(engineLock);

            engine.render(
                buffer->getWritePointer(
                    0, bufferToFill.startSample),
                buffer->getWritePointer(
                    1, bufferToFill.startSample),
                bufferToFill.numSamples
            );
        }
    }

    void releaseResources() override
    {
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff171a20));

        // Cabeçalho
        g.setColour(juce::Colour(0xff222832));
        g.fillRect(0, 0, getWidth(), 75);

        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(28.0f);
        g.drawText("SOUNDMODULE",
                   30, 15, 300, 45,
                   juce::Justification::centredLeft);

        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawText("SF2 INSTRUMENT PLAYER",
                   340, 25, 300, 30,
                   juce::Justification::centredLeft);

        // Display principal
        g.setColour(juce::Colour(0xff252d38));
        g.fillRoundedRectangle(30, 100,
                               getWidth() - 60, 125, 10.0f);

        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(15.0f);
        g.drawText("CURRENT INSTRUMENT",
                   55, 115, 300, 25,
                   juce::Justification::centredLeft);

        g.setColour(juce::Colours::white);
        g.setFont(26.0f);
        g.drawText(currentInstrumentName,
                   55, 145, 650, 50,
                   juce::Justification::centredLeft);

        // Identificação dos controles
        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(12.0f);
        g.drawText("MIDI INPUT",
                   760, 165, 180, 18,
                   juce::Justification::centredLeft);

        // Partes
        const int partY = 255;
        const int partHeight = 130;
        const int gap = 20;
        const int partWidth = (getWidth() - 80) / 3;

        for (int i = 0; i < 3; ++i)
        {
            int x = 30 + i * (partWidth + gap);

            g.setColour(juce::Colour(0xff252d38));
            g.fillRoundedRectangle(
                (float)x, (float)partY,
                (float)partWidth, (float)partHeight, 10.0f);

            g.setColour(juce::Colour(0xff55c8ff));
            g.setFont(18.0f);

            g.drawText("PART " + juce::String(i + 1),
                       x + 15, partY + 15,
                       partWidth - 30, 30,
                       juce::Justification::centredLeft);

            g.setColour(juce::Colours::white);
            g.setFont(22.0f);

            g.drawText(i == 0 ? "Grand Piano" :
                       i == 1 ? "Empty" : "Empty",
                       x + 15, partY + 55,
                       partWidth - 30, 35,
                       juce::Justification::centredLeft);
        }

        // Cenas Instant
        g.setColour(juce::Colour(0xff252d38));
        g.fillRoundedRectangle(30, 415,
                               getWidth() - 60, 145, 10.0f);

        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(18.0f);
        g.drawText("INSTANT SCENES",
                   50, 430, 300, 30,
                   juce::Justification::centredLeft);

        for (int i = 0; i < 8; ++i)
        {
            int x = 50 + i * 112;

            g.setColour(juce::Colour(0xff364454));
            g.fillRoundedRectangle(
                (float)x, 475.0f, 95.0f, 55.0f, 8.0f);

            g.setColour(juce::Colours::white);
            g.setFont(16.0f);
            g.drawText(juce::String(i + 1),
                       x, 475, 95, 55,
                       juce::Justification::centred);
        }

        // Rodapé
        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(13.0f);

        g.drawText(midiStatusText,
                   35, 590, 180, 25,
                   juce::Justification::centredLeft);

        g.drawText("AUDIO: READY",
                   220, 590, 150, 25,
                   juce::Justification::centredLeft);
    }

private:
    FluidSynthEngine engine;

    juce::CriticalSection engineLock;

    juce::TextButton loadButton { "LOAD SF2" };

    juce::ComboBox midiInputSelector;

    juce::Array<juce::MidiDeviceInfo> midiDevices;

    std::unique_ptr<juce::MidiInput> midiInput;

    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::String currentInstrumentName =
        "Nenhum SF2 carregado";

    juce::String midiStatusText =
        "MIDI: DISCONNECTED";
};

class SoundModuleApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override
    {
        return "SoundModule";
    }

    const juce::String getApplicationVersion() override
    {
        return "1.0";
    }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>();
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow()
            : DocumentWindow(
                "SoundModule",
                juce::Colour(0xff171a20),
                DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            centreWithSize(1000, 650);
            setResizable(true, true);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()
                ->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(SoundModuleApplication)
