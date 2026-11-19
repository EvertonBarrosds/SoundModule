#include <JuceHeader.h>
#include "FluidSynthEngine.h"

#include <algorithm>
#include <vector>

class MainComponent : public juce::AudioAppComponent,
                      public juce::MidiInputCallback
{
public:
    MainComponent()
    {
        setSize(1000, 700);

        // Biblioteca hibrida: pasta monitorada + arquivos adicionados manualmente.
        soundFontsFolder = juce::File("C:\\SoundModule\\SoundFonts");
        libraryManifest = soundFontsFolder.getChildFile("SoundModuleLibrary.txt");
        ignoredManifest = soundFontsFolder.getChildFile("SoundModuleIgnored.txt");

        soundFontsFolder.createDirectory();

        addAndMakeVisible(loadButton);
        loadButton.setButtonText("CARREGAR");
        loadButton.onClick = [this]() { loadSelectedSoundFont(); };

        addAndMakeVisible(addButton);
        addButton.setButtonText("ADICIONAR SF2");
        addButton.onClick = [this]() { chooseSoundFontToAdd(); };

        addAndMakeVisible(scanButton);
        scanButton.setButtonText("ATUALIZAR");
        scanButton.onClick = [this]() { refreshSoundFontLibrary(); };

        addAndMakeVisible(removeButton);
        removeButton.setButtonText("REMOVER DA LISTA");
        removeButton.onClick = [this]() { removeSelectedFromLibrary(); };

        addAndMakeVisible(soundFontSelector);
        soundFontSelector.onChange = [this]()
        {
            const int index = soundFontSelector.getSelectedId() - 1;
            if (index >= 0 && index < static_cast<int>(soundFontFiles.size()))
                selectedSoundFontName =
                    soundFontFiles[static_cast<size_t>(index)].getFileName();
            else
                selectedSoundFontName = "Nenhum SoundFont selecionado";

            repaint();
        };

        refreshSoundFontLibrary();
        addAndMakeVisible(midiInputSelector);

        midiDevices = juce::MidiInput::getAvailableDevices();

        for (int i = 0; i < midiDevices.size(); ++i)
            midiInputSelector.addItem(midiDevices[i].name, i + 1);

        if (midiDevices.isEmpty())
        {
            midiInputSelector.addItem("Nenhum MIDI encontrado", 1);
            midiInputSelector.setEnabled(false);
            midiStatusText = "MIDI: NO DEVICE";
        }
        else
        {
            midiInputSelector.setSelectedId(1, juce::dontSendNotification);
            midiInputSelector.onChange = [this]()
            {
                openMidiInput(midiInputSelector.getSelectedId());
            };
        }

        setAudioChannels(0, 2);

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

    // Faz a varredura da pasta padrao e inclui arquivos externos registrados.
    void refreshSoundFontLibrary()
    {
        soundFontFiles.clear();

        juce::StringArray ignoredPaths;
        if (ignoredManifest.existsAsFile())
            ignoredPaths.addLines(ignoredManifest.loadFileAsString());

        auto isIgnored = [&ignoredPaths](const juce::File& file)
        {
            const auto path = file.getFullPathName();
            for (const auto& ignored : ignoredPaths)
                if (path.equalsIgnoreCase(ignored.trim()))
                    return true;
            return false;
        };

        auto addIfValid = [this, &isIgnored](const juce::File& file)
        {
            if (!file.existsAsFile() || !file.hasFileExtension(".sf2") || isIgnored(file))
                return;

            const auto path = file.getFullPathName();
            for (const auto& existing : soundFontFiles)
                if (existing.getFullPathName().equalsIgnoreCase(path))
                    return;

            soundFontFiles.push_back(file);
        };

        // Arquivos colocados diretamente na pasta ou em suas subpastas.
        juce::Array<juce::File> foundFiles;
        if (soundFontsFolder.isDirectory())
            soundFontsFolder.findChildFiles(foundFiles, juce::File::findFiles,
                                             true, "*.sf2");

        for (const auto& file : foundFiles)
            addIfValid(file);

        // Arquivos escolhidos manualmente em qualquer outra pasta.
        if (libraryManifest.existsAsFile())
        {
            juce::StringArray registeredPaths;
            registeredPaths.addLines(libraryManifest.loadFileAsString());

            for (const auto& path : registeredPaths)
            {
                const auto trimmedPath = path.trim();
                if (trimmedPath.isNotEmpty())
                    addIfValid(juce::File(trimmedPath));
            }
        }

        std::sort(soundFontFiles.begin(), soundFontFiles.end(),
                  [](const juce::File& a, const juce::File& b)
                  {
                      return a.getFileName().compareIgnoreCase(b.getFileName()) < 0;
                  });

        const int previousSelection = soundFontSelector.getSelectedId();
        soundFontSelector.clear(juce::dontSendNotification);

        for (size_t i = 0; i < soundFontFiles.size(); ++i)
        {
            const auto label = soundFontFiles[i].getFileNameWithoutExtension();
            soundFontSelector.addItem(label, static_cast<int>(i) + 1);
        }

        if (!soundFontFiles.empty())
        {
            const int selection = juce::jlimit(
                1, static_cast<int>(soundFontFiles.size()),
                previousSelection > 0 ? previousSelection : 1);

            soundFontSelector.setSelectedId(selection, juce::sendNotification);
            libraryStatusText = juce::String(soundFontFiles.size()) +
                                " SoundFont(s) encontrado(s)";
        }
        else
        {
            selectedSoundFontName = "Nenhum SoundFont encontrado";
            libraryStatusText = "Coloque arquivos SF2 na pasta SoundFonts ou adicione manualmente";
        }

        repaint();
    }

    void chooseSoundFontToAdd()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Adicionar SoundFont a biblioteca",
            juce::File{},
            "*.sf2");

        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& chooser)
            {
                const auto file = chooser.getResult();

                if (!file.existsAsFile() || !file.hasFileExtension(".sf2"))
                    return;

                // Registra o caminho externo sem copiar, mover ou apagar o original.
                juce::StringArray registeredPaths;
                if (libraryManifest.existsAsFile())
                    registeredPaths.addLines(libraryManifest.loadFileAsString());

                const auto newPath = file.getFullPathName();
                bool alreadyRegistered = false;

                for (const auto& path : registeredPaths)
                    if (path.trim().equalsIgnoreCase(newPath))
                        alreadyRegistered = true;

                if (!alreadyRegistered)
                {
                    registeredPaths.add(newPath);
                    libraryManifest.replaceWithText(registeredPaths.joinIntoString("\n"));
                }

                // Se o arquivo estava marcado como ignorado, remove essa marca.
                juce::StringArray ignoredPaths;
                if (ignoredManifest.existsAsFile())
                    ignoredPaths.addLines(ignoredManifest.loadFileAsString());

                for (int i = ignoredPaths.size(); --i >= 0;)
                    if (ignoredPaths[i].trim().equalsIgnoreCase(newPath))
                        ignoredPaths.remove(i);

                ignoredManifest.replaceWithText(ignoredPaths.joinIntoString("\n"));

                refreshSoundFontLibrary();

                for (size_t i = 0; i < soundFontFiles.size(); ++i)
                {
                    if (soundFontFiles[i].getFullPathName().equalsIgnoreCase(newPath))
                    {
                        soundFontSelector.setSelectedId(
                            static_cast<int>(i) + 1, juce::sendNotification);
                        break;
                    }
                }
            });
    }

    void removeSelectedFromLibrary()
    {
        const int index = soundFontSelector.getSelectedId() - 1;
        if (index < 0 || index >= static_cast<int>(soundFontFiles.size()))
            return;

        const auto selectedFile = soundFontFiles[static_cast<size_t>(index)];
        const auto selectedPath = selectedFile.getFullPathName();
        const auto managedPrefix = soundFontsFolder.getFullPathName()
                                       .trimCharactersAtEnd("\\/") + "\\";

        if (selectedPath.startsWithIgnoreCase(managedPrefix))
        {
            // Arquivo da pasta automatica: apenas oculta da biblioteca.
            juce::StringArray ignoredPaths;
            if (ignoredManifest.existsAsFile())
                ignoredPaths.addLines(ignoredManifest.loadFileAsString());

            bool alreadyIgnored = false;
            for (const auto& path : ignoredPaths)
                if (path.trim().equalsIgnoreCase(selectedPath))
                    alreadyIgnored = true;

            if (!alreadyIgnored)
                ignoredPaths.add(selectedPath);

            ignoredManifest.replaceWithText(ignoredPaths.joinIntoString("\n"));
        }
        else
        {
            // Arquivo externo: remove somente o registro, nunca o arquivo original.
            juce::StringArray registeredPaths;
            if (libraryManifest.existsAsFile())
                registeredPaths.addLines(libraryManifest.loadFileAsString());

            for (int i = registeredPaths.size(); --i >= 0;)
                if (registeredPaths[i].trim().equalsIgnoreCase(selectedPath))
                    registeredPaths.remove(i);

            libraryManifest.replaceWithText(registeredPaths.joinIntoString("\n"));
        }

        refreshSoundFontLibrary();
    }

    void loadSelectedSoundFont()
    {
        const int index = soundFontSelector.getSelectedId() - 1;
        if (index < 0 || index >= static_cast<int>(soundFontFiles.size()))
        {
            libraryStatusText = "Selecione um SoundFont primeiro";
            repaint();
            return;
        }

        const auto file = soundFontFiles[static_cast<size_t>(index)];
        const auto path = file.getFullPathName();

        bool loaded = false;
        {
            const juce::ScopedLock lock(engineLock);
            loaded = engine.loadSoundFont(path.toRawUTF8());
        }

        if (loaded)
        {
            currentInstrumentName = file.getFileNameWithoutExtension();
            libraryStatusText = "Carregado: " + file.getFileName();
        }
        else
        {
            libraryStatusText = "Falha ao carregar: " + file.getFileName();
        }

        repaint();
    }

    void openMidiInput(int selectedId)
    {
        if (midiInput != nullptr)
        {
            midiInput->stop();
            midiInput.reset();
        }

        const int index = selectedId - 1;
        if (index < 0 || index >= midiDevices.size())
            return;

        midiInput = juce::MidiInput::openDevice(
            midiDevices[index].identifier, this);

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

    void handleIncomingMidiMessage(
        juce::MidiInput* source,
        const juce::MidiMessage& message) override
    {
        juce::ignoreUnused(source);
        const juce::ScopedLock lock(engineLock);

        if (message.isNoteOn())
        {
            engine.noteOn(message.getChannel() - 1,
                          message.getNoteNumber(),
                          message.getVelocity());
        }
        else if (message.isNoteOff())
        {
            engine.noteOff(message.getChannel() - 1,
                           message.getNoteNumber());
        }
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        juce::ignoreUnused(samplesPerBlockExpected);
        const juce::ScopedLock lock(engineLock);
        engine.initialize(sampleRate);
    }

    void getNextAudioBlock(
        const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        auto* buffer = bufferToFill.buffer;
        if (buffer == nullptr)
            return;

        buffer->clear(bufferToFill.startSample, bufferToFill.numSamples);

        if (buffer->getNumChannels() >= 2)
        {
            const juce::ScopedLock lock(engineLock);
            engine.render(
                buffer->getWritePointer(0, bufferToFill.startSample),
                buffer->getWritePointer(1, bufferToFill.startSample),
                bufferToFill.numSamples);
        }
    }

    void releaseResources() override {}

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff171a20));

        g.setColour(juce::Colour(0xff222832));
        g.fillRect(0, 0, getWidth(), 75);

        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(28.0f);
        g.drawText("SOUNDMODULE", 30, 15, 300, 45,
                   juce::Justification::centredLeft);

        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawText("SF2 INSTRUMENT PLAYER", 340, 25, 300, 30,
                   juce::Justification::centredLeft);

        // Instrumento atual.
        g.setColour(juce::Colour(0xff252d38));
        g.fillRoundedRectangle(30, 100, getWidth() - 60, 115, 10.0f);

        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(15.0f);
        g.drawText("CURRENT INSTRUMENT", 55, 112, 300, 25,
                   juce::Justification::centredLeft);

        g.setColour(juce::Colours::white);
        g.setFont(26.0f);
        g.drawText(currentInstrumentName, 55, 143, 650, 45,
                   juce::Justification::centredLeft);

        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(12.0f);
        g.drawText("MIDI INPUT", 760, 165, 180, 18,
                   juce::Justification::centredLeft);

        // Biblioteca de SoundFonts.
        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(15.0f);
        g.drawText("SOUNDFONT LIBRARY", 30, 225, 300, 22,
                   juce::Justification::centredLeft);

        g.setColour(juce::Colours::lightgrey);
        g.setFont(12.0f);
        g.drawText(libraryStatusText, 30, 275, 650, 22,
                   juce::Justification::centredLeft);

        g.setColour(juce::Colours::white);
        g.setFont(12.0f);
        g.drawText("Selecionado: " + selectedSoundFontName,
                   30, 298, 650, 20,
                   juce::Justification::centredLeft);

        // Partes multitimbral.
        const int partY = 335;
        const int partHeight = 105;
        const int gap = 20;
        const int partWidth = (getWidth() - 80) / 3;

        for (int i = 0; i < 3; ++i)
        {
            const int x = 30 + i * (partWidth + gap);

            g.setColour(juce::Colour(0xff252d38));
            g.fillRoundedRectangle(static_cast<float>(x),
                                   static_cast<float>(partY),
                                   static_cast<float>(partWidth),
                                   static_cast<float>(partHeight), 10.0f);

            g.setColour(juce::Colour(0xff55c8ff));
            g.setFont(18.0f);
            g.drawText("PART " + juce::String(i + 1),
                       x + 15, partY + 12, partWidth - 30, 28,
                       juce::Justification::centredLeft);

            g.setColour(juce::Colours::white);
            g.setFont(20.0f);
            g.drawText(i == 0 ? "Grand Piano" : "Empty",
                       x + 15, partY + 50, partWidth - 30, 30,
                       juce::Justification::centredLeft);
        }

        // Cenas Instant.
        g.setColour(juce::Colour(0xff252d38));
        g.fillRoundedRectangle(30, 460, getWidth() - 60, 145, 10.0f);

        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(18.0f);
        g.drawText("INSTANT SCENES", 50, 475, 300, 30,
                   juce::Justification::centredLeft);

        for (int i = 0; i < 8; ++i)
        {
            const int x = 50 + i * 112;

            g.setColour(juce::Colour(0xff364454));
            g.fillRoundedRectangle(static_cast<float>(x), 520.0f,
                                   95.0f, 55.0f, 8.0f);

            g.setColour(juce::Colours::white);
            g.setFont(16.0f);
            g.drawText(juce::String(i + 1), x, 520, 95, 55,
                       juce::Justification::centred);
        }

        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(13.0f);
        g.drawText(midiStatusText, 35, 645, 180, 25,
                   juce::Justification::centredLeft);
        g.drawText("AUDIO: READY", 220, 645, 150, 25,
                   juce::Justification::centredLeft);
    }

    void resized() override
    {
        midiInputSelector.setBounds(760, 185, 180, 30);

        soundFontSelector.setBounds(30, 250, 500, 30);
        loadButton.setBounds(545, 250, 110, 32);
        scanButton.setBounds(665, 250, 110, 32);
        addButton.setBounds(785, 250, 150, 32);
        removeButton.setBounds(785, 285, 150, 28);
    }

private:
    FluidSynthEngine engine;
    juce::CriticalSection engineLock;

    juce::TextButton loadButton { "CARREGAR" };
    juce::TextButton addButton { "ADICIONAR SF2" };
    juce::TextButton scanButton { "ATUALIZAR" };
    juce::TextButton removeButton { "REMOVER DA LISTA" };

    juce::ComboBox soundFontSelector;
    juce::ComboBox midiInputSelector;

    juce::Array<juce::MidiDeviceInfo> midiDevices;
    std::unique_ptr<juce::MidiInput> midiInput;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::File soundFontsFolder;
    juce::File libraryManifest;
    juce::File ignoredManifest;
    std::vector<juce::File> soundFontFiles;

    juce::String currentInstrumentName = "Nenhum SF2 carregado";
    juce::String selectedSoundFontName = "Nenhum SoundFont selecionado";
    juce::String libraryStatusText = "Procurando SoundFonts...";
    juce::String midiStatusText = "MIDI: DISCONNECTED";
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
            : DocumentWindow("SoundModule",
                             juce::Colour(0xff171a20),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            centreWithSize(1000, 700);
            setResizable(true, true);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(SoundModuleApplication)
