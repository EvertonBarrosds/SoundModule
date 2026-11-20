
#include <JuceHeader.h>
#include "FluidSynthEngine.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <functional>
#include <vector>
#include <JuceHeader.h>

#include "ArduinoSerialMidiInput.h"

// =============================================================
// BOTÃO DE PRESET
// =============================================================

class PresetButton : public juce::TextButton
{
public:
    std::function<void()> onRightClick;

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mods.isRightButtonDown())
        {
            rightClickHandled = true;

            if (onRightClick)
                onRightClick();

            return;
        }

        juce::TextButton::mouseDown(event);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (rightClickHandled)
        {
            rightClickHandled = false;
            return;
        }

        juce::TextButton::mouseUp(event);
    }

private:
    bool rightClickHandled = false;
};


// =============================================================
// MAIN COMPONENT
// =============================================================

class MainComponent : public juce::AudioAppComponent,
                      public juce::MidiInputCallback,
                      public juce::MidiKeyboardState::Listener
{
public:

    static constexpr int NUM_PRESETS = 5;

    MainComponent()
    {
        setSize(1000, 700);

        soundFontsFolder = juce::File("C:\\SoundModule\\SoundFonts");

        libraryManifest =
            soundFontsFolder.getChildFile("SoundModuleLibrary.txt");

        ignoredManifest =
            soundFontsFolder.getChildFile("SoundModuleIgnored.txt");

        categoryManifest =
            soundFontsFolder.getChildFile("SoundModuleCategories.xml");

        instrumentImagesFolder =
            juce::File("C:\\SoundModule\\imginstr");

        soundFontsFolder.createDirectory();

        // =====================================================
        // BOTÕES DA BIBLIOTECA
        // =====================================================

        addAndMakeVisible(loadButton);
        loadButton.setButtonText("CARREGAR");

        loadButton.onClick = [this]()
        {
            loadSelectedSoundFont();
        };

        addAndMakeVisible(addButton);
        addButton.setButtonText("ADICIONAR SF2");

        addButton.onClick = [this]()
        {
            chooseSoundFontToAdd();
        };

        addAndMakeVisible(scanButton);
        scanButton.setButtonText("ATUALIZAR");

        scanButton.onClick = [this]()
        {
            refreshSoundFontLibrary();
        };

        addAndMakeVisible(removeButton);
        removeButton.setButtonText("REMOVER DA LISTA");

        removeButton.onClick = [this]()
        {
            removeSelectedFromLibrary();
        };

        // =====================================================
        // SELETOR DE SOUNDFONT
        // =====================================================

        addAndMakeVisible(soundFontSelector);

        soundFontSelector.onChange = [this]()
        {
            const int index =
                soundFontSelector.getSelectedId() - 1;

            if (index >= 0 &&
                index < static_cast<int>(soundFontFiles.size()))
            {
                selectedSoundFontName =
                    soundFontFiles[static_cast<size_t>(index)]
                        .getFileName();

                if (audioReady)
                    loadSelectedSoundFont();
            }
            else
            {
                selectedSoundFontName =
                    "Nenhum SoundFont selecionado";

                repaint();
            }
        };

        addAndMakeVisible(instrumentImage);

        refreshSoundFontLibrary();

        // =====================================================
        // ENTRADA MIDI E ARDUINO SERIAL
        // =====================================================

        addAndMakeVisible(midiInputSelector);

        midiDevices = juce::MidiInput::getAvailableDevices();
        serialPorts = ArduinoSerialMidiInput::getAvailablePorts();

        // Adiciona dispositivos MIDI convencionais

        for (int i = 0; i < midiDevices.size(); ++i)
        {
            midiInputSelector.addItem(
                "MIDI - " + midiDevices[i].name,
                i + 1
            );
        }

        // Adiciona portas COM do Arduino

        for (int i = 0; i < serialPorts.size(); ++i)
        {
            midiInputSelector.addItem(
                "Arduino - " + serialPorts[i],
                1001 + i
            );
        }

        // Verifica se existe alguma entrada disponível

        if (midiDevices.isEmpty() && serialPorts.isEmpty())
        {
            midiInputSelector.addItem(
                "Nenhuma entrada encontrada",
                1
            );

            midiInputSelector.setEnabled(false);
            midiStatusText = "MIDI: NO DEVICE";
        }
        else
        {
            midiInputSelector.setEnabled(true);

            midiInputSelector.onChange = [this]()
            {
                const int selectedId =
                    midiInputSelector.getSelectedId();

                if (selectedId >= 1001)
                {
                    const int portIndex = selectedId - 1001;

                    if (portIndex >= 0 &&
                        portIndex < serialPorts.size())
                    {
                        openArduinoSerial(serialPorts[portIndex]);
                    }
                }
                else
                {
                    openMidiInput(selectedId);
                }
            };

            // Prioriza MIDI convencional.
            // Se não houver, seleciona o Arduino.

            if (!midiDevices.isEmpty())
            {
                midiInputSelector.setSelectedId(
                    1,
                    juce::dontSendNotification
                );
            }
            else
            {
                midiInputSelector.setSelectedId(
                    1001,
                    juce::dontSendNotification
                );
            }
        }

        // =====================================================
        // PIANO VIRTUAL
        // =====================================================

        addAndMakeVisible(virtualKeyboard);

        virtualKeyboard.setAvailableRange(36, 95);
        virtualKeyboard.setScrollButtonsVisible(false);
        virtualKeyboard.setKeyWidth(20.0f);

        virtualKeyboard.setColour(
            juce::MidiKeyboardComponent::whiteNoteColourId,
            juce::Colours::white);

        virtualKeyboard.setColour(
            juce::MidiKeyboardComponent::blackNoteColourId,
            juce::Colours::black);

        virtualKeyboard.setColour(
            juce::MidiKeyboardComponent::keySeparatorLineColourId,
            juce::Colours::darkgrey);

        keyboardState.addListener(this);

        // =====================================================
        // PITCH BEND
        // =====================================================

        addAndMakeVisible(pitchBendSlider);

        pitchBendSlider.setSliderStyle(
            juce::Slider::LinearVertical);

        pitchBendSlider.setTextBoxStyle(
            juce::Slider::NoTextBox, false, 0, 0);

        pitchBendSlider.setRange(0, 16383, 1);

        pitchBendSlider.setValue(
            8192, juce::dontSendNotification);

        pitchBendSlider.setColour(
            juce::Slider::trackColourId,
            juce::Colour(0xff55c8ff));

        pitchBendSlider.setColour(
            juce::Slider::thumbColourId,
            juce::Colours::white);

        pitchBendSlider.setColour(
            juce::Slider::backgroundColourId,
            juce::Colour(0xff111820));

        pitchBendSlider.onValueChange = [this]()
        {
            currentPitchBend =
                juce::roundToInt(pitchBendSlider.getValue());

            const juce::ScopedLock lock(engineLock);

            engine.setPitchBend(currentPitchBend);
        };

        pitchBendSlider.onDragEnd = [this]()
        {
            pitchBendSlider.setValue(
                8192, juce::sendNotification);
        };

        // =====================================================
        // MODULATION
        // =====================================================

        addAndMakeVisible(modulationSlider);

        modulationSlider.setSliderStyle(
            juce::Slider::LinearVertical);

        modulationSlider.setTextBoxStyle(
            juce::Slider::NoTextBox, false, 0, 0);

        modulationSlider.setRange(0, 127, 1);

        modulationSlider.setValue(
            0, juce::dontSendNotification);

        modulationSlider.setColour(
            juce::Slider::trackColourId,
            juce::Colour(0xff55c8ff));

        modulationSlider.setColour(
            juce::Slider::thumbColourId,
            juce::Colours::white);

        modulationSlider.setColour(
            juce::Slider::backgroundColourId,
            juce::Colour(0xff111820));

        modulationSlider.onValueChange = [this]()
        {
            currentModulation =
                juce::roundToInt(modulationSlider.getValue());

            const juce::ScopedLock lock(engineLock);

            engine.setModulation(currentModulation);
        };

        // =====================================================
        // VOLUME MASTER
        // =====================================================

        addAndMakeVisible(masterVolumeSlider);

        masterVolumeSlider.setSliderStyle(
            juce::Slider::RotaryHorizontalVerticalDrag);

        masterVolumeSlider.setTextBoxStyle(
            juce::Slider::NoTextBox, false, 0, 0);

        masterVolumeSlider.setRange(0, 100, 1);

        masterVolumeSlider.setValue(
            100, juce::dontSendNotification);

        masterVolumeSlider.setDoubleClickReturnValue(
            true, 100);

        masterVolumeSlider.setTooltip(
            "Volume principal do SoundModule");

        masterVolumeSlider.setColour(
            juce::Slider::rotarySliderFillColourId,
            juce::Colour(0xff55c8ff));

        masterVolumeSlider.setColour(
            juce::Slider::rotarySliderOutlineColourId,
            juce::Colour(0xff394653));

        masterVolumeSlider.setColour(
            juce::Slider::thumbColourId,
            juce::Colours::white);

        masterVolumeSlider.onValueChange = [this]()
        {
            const float volume =
                static_cast<float>(
                    masterVolumeSlider.getValue() / 100.0);

            masterVolume.store(volume);
            repaint();
        };

        // =====================================================
        // CINCO BOTÕES DE PRESET
        // =====================================================

        for (int i = 0; i < NUM_PRESETS; ++i)
        {
            auto& button =
                presetButtons[static_cast<size_t>(i)];

            addAndMakeVisible(button);

            button.setButtonText(
                "PRESET " + juce::String(i + 1) + " - VAZIO");

            button.setColour(
                juce::TextButton::buttonColourId,
                juce::Colour(0xff252d38));

            button.setColour(
                juce::TextButton::textColourOffId,
                juce::Colours::white);

            button.onClick = [this, i]()
            {
                loadPreset(i);
            };

            button.onRightClick = [this, i]()
            {
                savePreset(i);
            };
        }

        loadPresetFile();

        // =====================================================
        // ÁUDIO
        // =====================================================

        setAudioChannels(0, 2);

        audioReady = true;

        if (soundFontSelector.getSelectedId() > 0)
            loadSelectedSoundFont();

        // Abre automaticamente a entrada disponível.
        // MIDI convencional tem prioridade.
        // Se não houver MIDI, abre a primeira porta COM.

        if (!midiDevices.isEmpty())
        {
            openMidiInput(1);
        }
        else if (!serialPorts.isEmpty())
        {
            openArduinoSerial(serialPorts[0]);
        }
    }


    ~MainComponent() override
    {
        keyboardState.removeListener(this);

        if (midiInput != nullptr)
        {
            midiInput->stop();
            midiInput.reset();
        }

        // Encerra a thread serial antes de destruir o componente.

        arduinoSerial.close();

        shutdownAudio();
    }


    // =============================================================
    // ARQUIVO DE PRESETS
    // =============================================================

    juce::File getPresetFile() const
    {
        auto folder =
            juce::File::getSpecialLocation(
                juce::File::userApplicationDataDirectory)
                .getChildFile("SoundModule");

        return folder.getChildFile("Presets.xml");
    }


    void updatePresetButton(int index, bool saved)
    {
        if (index < 0 || index >= NUM_PRESETS)
            return;

        auto& button =
            presetButtons[static_cast<size_t>(index)];

        button.setButtonText(
            "PRESET " + juce::String(index + 1) +
            (saved ? " - SALVO" : " - VAZIO"));

        button.setColour(
            juce::TextButton::buttonColourId,
            saved
                ? juce::Colour(0xff17445a)
                : juce::Colour(0xff252d38));

        button.repaint();
    }


    std::unique_ptr<juce::XmlElement> readPresetDocument() const
    {
        const auto file = getPresetFile();

        if (file.existsAsFile())
        {
            auto document = juce::XmlDocument::parse(file);

            if (document != nullptr &&
                document->hasTagName("SOUNDMODULE_PRESETS"))
            {
                return document;
            }
        }

        return std::make_unique<juce::XmlElement>(
            "SOUNDMODULE_PRESETS");
    }


    void writePresetDocument(const juce::XmlElement& document)
    {
        const auto file = getPresetFile();

        file.getParentDirectory().createDirectory();

        juce::FileOutputStream stream(file);

        if (!stream.openedOk())
        {
            libraryStatusText =
                "Erro ao gravar arquivo de presets";

            repaint();
            return;
        }

        stream.setPosition(0);
        stream.truncate();

        document.writeTo(stream);

        stream.flush();
    }


    void loadPresetFile()
    {
        auto document = readPresetDocument();

        for (int i = 0; i < NUM_PRESETS; ++i)
        {
            bool saved = false;

            for (int childIndex = 0;
                 childIndex < document->getNumChildElements();
                 ++childIndex)
            {
                auto* slot =
                    document->getChildElement(childIndex);

                if (slot != nullptr &&
                    slot->hasTagName("PRESET") &&
                    slot->getIntAttribute("id", 0) == i + 1)
                {
                    saved = true;
                    break;
                }
            }

            updatePresetButton(i, saved);
        }
    }


    // =============================================================
    // SALVAR PRESET
    // =============================================================

    void savePreset(int index)
    {
        if (index < 0 || index >= NUM_PRESETS)
            return;

        auto document = readPresetDocument();

        for (int childIndex =
                 document->getNumChildElements() - 1;
             childIndex >= 0;
             --childIndex)
        {
            auto* child =
                document->getChildElement(childIndex);

            if (child != nullptr &&
                child->hasTagName("PRESET") &&
                child->getIntAttribute("id", 0) == index + 1)
            {
                document->removeChildElement(child, true);
            }
        }

        auto* slot =
            document->createNewChildElement("PRESET");

        slot->setAttribute("id", index + 1);

        slot->setAttribute(
            "pitchBend", currentPitchBend);

        slot->setAttribute(
            "modulation", currentModulation);

        slot->setAttribute(
            "masterVolume",
            juce::roundToInt(masterVolume.load() * 100.0f));

        FluidSynthEngine::PlayMode mode;
        int splitPoint = 60;

        {
            const juce::ScopedLock lock(engineLock);

            mode = engine.getPlayMode();
            splitPoint = engine.getSplitPoint();

            for (int partIndex = 0;
                 partIndex < FluidSynthEngine::NUM_PARTS;
                 ++partIndex)
            {
                const auto state =
                    engine.getPartState(partIndex);

                auto* part =
                    slot->createNewChildElement("PART");

                part->setAttribute("index", partIndex);

                part->setAttribute(
                    "soundFont",
                    juce::String::fromUTF8(
                        state.soundFontPath.c_str()));

                part->setAttribute("bank", state.bank);
                part->setAttribute("preset", state.preset);
                part->setAttribute("volume", state.volume);

                part->setAttribute(
                    "lowestNote", state.lowestNote);

                part->setAttribute(
                    "highestNote", state.highestNote);

                part->setAttribute("enabled", state.enabled);
            }
        }

        slot->setAttribute(
            "mode",
            mode == FluidSynthEngine::PlayMode::Split
                ? "SPLIT"
                : "LAYER");

        slot->setAttribute("splitPoint", splitPoint);

        writePresetDocument(*document);

        updatePresetButton(index, true);

        libraryStatusText =
            "Configuração salva no PRESET " +
            juce::String(index + 1);

        repaint();
    }


    // =============================================================
    // CARREGAR PRESET
    // =============================================================

    void loadPreset(int index)
    {
        if (index < 0 || index >= NUM_PRESETS)
            return;

        auto document = readPresetDocument();

        juce::XmlElement* slot = nullptr;

        for (int childIndex = 0;
             childIndex < document->getNumChildElements();
             ++childIndex)
        {
            auto* child =
                document->getChildElement(childIndex);

            if (child != nullptr &&
                child->hasTagName("PRESET") &&
                child->getIntAttribute("id", 0) == index + 1)
            {
                slot = child;
                break;
            }
        }

        if (slot == nullptr)
        {
            libraryStatusText =
                "PRESET " + juce::String(index + 1) +
                " está vazio";

            repaint();
            return;
        }

        bool restoredAnyPart = false;

        juce::String firstPartSoundFont;

        {
            const juce::ScopedLock lock(engineLock);

            for (int partIndex = 0;
                 partIndex < FluidSynthEngine::NUM_PARTS;
                 ++partIndex)
            {
                juce::XmlElement* partElement = nullptr;

                for (int childIndex = 0;
                     childIndex < slot->getNumChildElements();
                     ++childIndex)
                {
                    auto* child =
                        slot->getChildElement(childIndex);

                    if (child != nullptr &&
                        child->hasTagName("PART") &&
                        child->getIntAttribute(
                            "index", -1) == partIndex)
                    {
                        partElement = child;
                        break;
                    }
                }

                if (partElement == nullptr)
                    continue;

                const auto soundFont =
                    partElement->getStringAttribute("soundFont");

                if (soundFont.isNotEmpty())
                {
                    FluidSynthEngine::PartState state;

                    state.soundFontPath =
                        soundFont.toStdString();

                    state.bank =
                        partElement->getIntAttribute("bank", 0);

                    state.preset =
                        partElement->getIntAttribute("preset", 0);

                    state.volume =
                        partElement->getIntAttribute("volume", 100);

                    state.lowestNote =
                        partElement->getIntAttribute(
                            "lowestNote", 0);

                    state.highestNote =
                        partElement->getIntAttribute(
                            "highestNote", 127);

                    state.enabled =
                        partElement->getBoolAttribute(
                            "enabled", true);

                    if (engine.restorePartState(
                            partIndex, state))
                    {
                        restoredAnyPart = true;

                        if (partIndex == 0)
                            firstPartSoundFont = soundFont;
                    }
                }
                else
                {
                    engine.setPartEnabled(partIndex, false);
                }
            }

            const auto modeText =
                slot->getStringAttribute("mode", "LAYER");

            engine.setPlayMode(
                modeText.equalsIgnoreCase("SPLIT")
                    ? FluidSynthEngine::PlayMode::Split
                    : FluidSynthEngine::PlayMode::Layer);

            engine.setSplitPoint(
                slot->getIntAttribute("splitPoint", 60));

            currentPitchBend = juce::jlimit(
                0, 16383,
                slot->getIntAttribute("pitchBend", 8192));

            currentModulation = juce::jlimit(
                0, 127,
                slot->getIntAttribute("modulation", 0));

            engine.setPitchBend(currentPitchBend);
            engine.setModulation(currentModulation);
        }

        // Restaura volume principal

        const int savedVolume = juce::jlimit(
            0, 100,
            slot->getIntAttribute("masterVolume", 100));

        masterVolume.store(savedVolume / 100.0f);

        masterVolumeSlider.setValue(
            savedVolume,
            juce::dontSendNotification);

        pitchBendSlider.setValue(
            currentPitchBend,
            juce::dontSendNotification);

        modulationSlider.setValue(
            currentModulation,
            juce::dontSendNotification);

        // Atualiza o seletor sem disparar novo carregamento

        if (firstPartSoundFont.isNotEmpty())
        {
            const juce::File restoredFile(firstPartSoundFont);

            updateCurrentInstrumentDisplay(restoredFile);

            selectedSoundFontName =
                restoredFile.getFileName();

            for (size_t i = 0; i < soundFontFiles.size(); ++i)
            {
                if (soundFontFiles[i].getFullPathName()
                    .equalsIgnoreCase(firstPartSoundFont))
                {
                    soundFontSelector.setSelectedId(
                        static_cast<int>(i) + 1,
                        juce::dontSendNotification);

                    break;
                }
            }
        }

        libraryStatusText = restoredAnyPart
            ? "PRESET " + juce::String(index + 1) + " carregado"
            : "Preset lido, mas nenhum SoundFont pôde ser restaurado";

        repaint();
    }


    // =============================================================
    // CATEGORIAS DE INSTRUMENTOS
    // =============================================================

    juce::StringArray getInstrumentCategories() const
    {
        return {
            "Piano",
            "Electric Piano",
            "Organ",
            "Synth",
            "Strings",
            "Brass",
            "Woodwinds",
            "Guitar",
            "Bass",
            "Plucked",
            "Mallet",
            "Bell",
            "Choir",
            "Drums & Percussion",
            "World",
            "FX"
        };
    }


    void loadSoundFontCategories()
    {
        soundFontCategories.clear();

        if (!categoryManifest.existsAsFile())
            return;

        auto xml = juce::XmlDocument::parse(categoryManifest);

        if (xml == nullptr ||
            !xml->hasTagName("SOUNDMODULE_CATEGORIES"))
        {
            return;
        }

        for (int i = 0; i < xml->getNumChildElements(); ++i)
        {
            auto* child = xml->getChildElement(i);

            if (child != nullptr &&
                child->hasTagName("SOUNDFONT"))
            {
                const auto path =
                    child->getStringAttribute("path");

                const auto category =
                    child->getStringAttribute(
                        "category", "Uncategorized");

                if (path.isNotEmpty())
                    soundFontCategories.set(path, category);
            }
        }
    }


    void saveSoundFontCategories()
    {
        juce::XmlElement xml("SOUNDMODULE_CATEGORIES");

        const auto keys = soundFontCategories.getAllKeys();
        const auto values = soundFontCategories.getAllValues();

        for (int i = 0; i < keys.size(); ++i)
        {
            auto* item = xml.createNewChildElement("SOUNDFONT");

            item->setAttribute("path", keys[i]);
            item->setAttribute("category", values[i]);
        }

        xml.writeTo(categoryManifest);
    }


    juce::String getSoundFontCategory(const juce::File& file) const
    {
        return soundFontCategories.getValue(
            file.getFullPathName(),
            "Uncategorized");
    }


    void setSoundFontCategory(
        const juce::File& file,
        const juce::String& category)
    {
        soundFontCategories.set(
            file.getFullPathName(), category);

        saveSoundFontCategories();
    }


    // =============================================================
    // IMAGEM DA CATEGORIA DO INSTRUMENTO
    // =============================================================

    juce::String getCategoryImageFileName(const juce::String& category) const
    {
        if (category == "Piano")                 return "piano.webp";
        if (category == "Electric Piano")        return "eletricPiano.jpg";
        if (category == "Organ")                 return "organ.jpg";
        if (category == "Synth")                 return "synth.jpg";
        if (category == "Strings")               return "strings.jpg";
        if (category == "Brass")                 return "brass.jpg";
        if (category == "Woodwinds")             return "Woodwinds.webp";
        if (category == "Guitar")                return "guitar.jpg";
        if (category == "Plucked")               return "Plucked.jpg";
        if (category == "Mallet")                return "Mallet.jpg";
        if (category == "Bell")                  return "Bell.avif";
        if (category == "Choir")                 return "Choir.jpg";
        if (category == "Drums & Percussion")    return "Drums & Percussion.jpg";
        if (category == "FX")                    return "fx.webp";

        // Bass e World ainda não possuem imagem na pasta.
        return {};
    }


    void updateCurrentInstrumentDisplay(const juce::File& soundFontFile)
    {
        const auto category = getSoundFontCategory(soundFontFile);
        const auto imageFileName = getCategoryImageFileName(category);

        if (imageFileName.isNotEmpty())
        {
            const auto imageFile =
                instrumentImagesFolder.getChildFile(imageFileName);

            if (imageFile.existsAsFile())
            {
                const auto image =
                    juce::ImageFileFormat::loadFrom(imageFile);

                instrumentImage.setImage(
                    image,
                    juce::RectanglePlacement(
                        juce::RectanglePlacement::xLeft |
                        juce::RectanglePlacement::fillDestination));
            }
            else
            {
                instrumentImage.setImage(juce::Image());
            }
        }
        else
        {
            instrumentImage.setImage(juce::Image());
        }

        instrumentImage.repaint();
        repaint();
    }


    // =============================================================
    // BIBLIOTECA DE SOUNDFONTS
    // =============================================================

    void refreshSoundFontLibrary()
    {
        soundFontFiles.clear();

        loadSoundFontCategories();

        juce::StringArray ignoredPaths;

        if (ignoredManifest.existsAsFile())
            ignoredPaths.addLines(
                ignoredManifest.loadFileAsString());

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
            if (!file.existsAsFile() ||
                !file.hasFileExtension(".sf2") ||
                isIgnored(file))
            {
                return;
            }

            const auto path = file.getFullPathName();

            for (const auto& existing : soundFontFiles)
                if (existing.getFullPathName().equalsIgnoreCase(path))
                    return;

            soundFontFiles.push_back(file);
        };

        juce::Array<juce::File> foundFiles;

        if (soundFontsFolder.isDirectory())
        {
            soundFontsFolder.findChildFiles(
                foundFiles,
                juce::File::findFiles,
                true,
                "*.sf2");
        }

        for (const auto& file : foundFiles)
            addIfValid(file);

        if (libraryManifest.existsAsFile())
        {
            juce::StringArray registeredPaths;

            registeredPaths.addLines(
                libraryManifest.loadFileAsString());

            for (const auto& path : registeredPaths)
            {
                const auto trimmedPath = path.trim();

                if (trimmedPath.isNotEmpty())
                    addIfValid(juce::File(trimmedPath));
            }
        }

        std::sort(
            soundFontFiles.begin(),
            soundFontFiles.end(),
            [](const juce::File& a, const juce::File& b)
            {
                return a.getFileName().compareIgnoreCase(
                    b.getFileName()) < 0;
            });

        const int previousSelection =
            soundFontSelector.getSelectedId();

        soundFontSelector.clear(
            juce::dontSendNotification);

        for (size_t i = 0; i < soundFontFiles.size(); ++i)
        {
            const auto& file = soundFontFiles[i];

            const auto displayName =
                file.getFileNameWithoutExtension()
                + " ["
                + getSoundFontCategory(file)
                + "]";

            soundFontSelector.addItem(
                displayName,
                static_cast<int>(i) + 1);
        }

        if (!soundFontFiles.empty())
        {
            const int selection = juce::jlimit(
                1,
                static_cast<int>(soundFontFiles.size()),
                previousSelection > 0 ? previousSelection : 1);

            soundFontSelector.setSelectedId(
                selection,
                juce::sendNotification);

            libraryStatusText =
                juce::String(soundFontFiles.size()) +
                " SoundFont(s) encontrado(s)";
        }
        else
        {
            selectedSoundFontName =
                "Nenhum SoundFont encontrado";

            libraryStatusText =
                "Coloque arquivos SF2 na pasta SoundFonts ou adicione manualmente";
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

                if (!file.existsAsFile() ||
                    !file.hasFileExtension(".sf2"))
                {
                    return;
                }

                // Abre menu para escolher a categoria

                const auto categories = getInstrumentCategories();

                juce::PopupMenu menu;

                for (int i = 0; i < categories.size(); ++i)
                    menu.addItem(i + 1, categories[i]);

                menu.showMenuAsync(
                    juce::PopupMenu::Options()
                        .withTargetComponent(&addButton),
                    [this, file, categories](int result)
                    {
                        if (result < 1 ||
                            result > categories.size())
                        {
                            return;
                        }

                        setSoundFontCategory(
                            file, categories[result - 1]);

                        juce::StringArray registeredPaths;

                        if (libraryManifest.existsAsFile())
                        {
                            registeredPaths.addLines(
                                libraryManifest.loadFileAsString());
                        }

                        const auto newPath =
                            file.getFullPathName();

                        bool alreadyRegistered = false;

                        for (const auto& path : registeredPaths)
                        {
                            if (path.trim().equalsIgnoreCase(newPath))
                                alreadyRegistered = true;
                        }

                        if (!alreadyRegistered)
                        {
                            registeredPaths.add(newPath);

                            libraryManifest.replaceWithText(
                                registeredPaths.joinIntoString("\n"));
                        }

                        juce::StringArray ignoredPaths;

                        if (ignoredManifest.existsAsFile())
                        {
                            ignoredPaths.addLines(
                                ignoredManifest.loadFileAsString());
                        }

                        for (int i = ignoredPaths.size(); --i >= 0;)
                        {
                            if (ignoredPaths[i].trim()
                                .equalsIgnoreCase(newPath))
                            {
                                ignoredPaths.remove(i);
                            }
                        }

                        ignoredManifest.replaceWithText(
                            ignoredPaths.joinIntoString("\n"));

                        refreshSoundFontLibrary();

                        for (size_t i = 0;
                             i < soundFontFiles.size();
                             ++i)
                        {
                            if (soundFontFiles[i].getFullPathName()
                                .equalsIgnoreCase(newPath))
                            {
                                soundFontSelector.setSelectedId(
                                    static_cast<int>(i) + 1,
                                    juce::sendNotification);

                                break;
                            }
                        }
                    });
            });
    }


    void removeSelectedFromLibrary()
    {
        const int index =
            soundFontSelector.getSelectedId() - 1;

        if (index < 0 ||
            index >= static_cast<int>(soundFontFiles.size()))
        {
            return;
        }

        const auto selectedFile =
            soundFontFiles[static_cast<size_t>(index)];

        const auto selectedPath =
            selectedFile.getFullPathName();

        const auto managedPrefix =
            soundFontsFolder.getFullPathName()
                .trimCharactersAtEnd("\\/") + "\\";

        if (selectedPath.startsWithIgnoreCase(managedPrefix))
        {
            juce::StringArray ignoredPaths;

            if (ignoredManifest.existsAsFile())
            {
                ignoredPaths.addLines(
                    ignoredManifest.loadFileAsString());
            }

            bool alreadyIgnored = false;

            for (const auto& path : ignoredPaths)
                if (path.trim().equalsIgnoreCase(selectedPath))
                    alreadyIgnored = true;

            if (!alreadyIgnored)
                ignoredPaths.add(selectedPath);

            ignoredManifest.replaceWithText(
                ignoredPaths.joinIntoString("\n"));
        }
        else
        {
            juce::StringArray registeredPaths;

            if (libraryManifest.existsAsFile())
            {
                registeredPaths.addLines(
                    libraryManifest.loadFileAsString());
            }

            for (int i = registeredPaths.size(); --i >= 0;)
            {
                if (registeredPaths[i].trim()
                    .equalsIgnoreCase(selectedPath))
                {
                    registeredPaths.remove(i);
                }
            }

            libraryManifest.replaceWithText(
                registeredPaths.joinIntoString("\n"));
        }

        refreshSoundFontLibrary();
    }


    void loadSelectedSoundFont()
    {
        const int index =
            soundFontSelector.getSelectedId() - 1;

        if (index < 0 ||
            index >= static_cast<int>(soundFontFiles.size()))
        {
            libraryStatusText =
                "Selecione um SoundFont primeiro";

            repaint();
            return;
        }

        const auto file =
            soundFontFiles[static_cast<size_t>(index)];

        const auto path = file.getFullPathName();

        bool loaded = false;

        {
            const juce::ScopedLock lock(engineLock);

            loaded = engine.loadSoundFont(path.toRawUTF8());
        }

        if (loaded)
        {
            updateCurrentInstrumentDisplay(file);

            libraryStatusText =
                "Carregado: " + file.getFileName();
        }
        else
        {
            libraryStatusText =
                "Falha ao carregar: " + file.getFileName();
        }

        repaint();
    }


    // =============================================================
    // MIDI CONVENCIONAL
    // =============================================================

    void openMidiInput(int selectedId)
    {
        // Fecha a conexão serial antes de abrir MIDI convencional.

        arduinoSerial.close();

        if (midiInput != nullptr)
        {
            midiInput->stop();
            midiInput.reset();
        }

        const int index = selectedId - 1;

        if (index < 0 || index >= midiDevices.size())
            return;

        midiInput = juce::MidiInput::openDevice(
            midiDevices[index].identifier,
            this);

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


    // =============================================================
    // ABRIR ENTRADA SERIAL DO ARDUINO
    // =============================================================

    void openArduinoSerial(const juce::String& port)
    {
        // Fecha a entrada MIDI convencional.

        if (midiInput != nullptr)
        {
            midiInput->stop();
            midiInput.reset();
        }

        // Fecha qualquer conexão serial anterior.

        arduinoSerial.close();

        const bool opened = arduinoSerial.open(
            port,
            [this](uint8_t status, uint8_t data1, uint8_t data2)
            {
                handleArduinoMidiMessage(status, data1, data2);
            }
        );

        if (opened)
            midiStatusText = "ARDUINO: CONNECTED (" + port + ")";
        else
            midiStatusText = "ARDUINO: ERROR (" + port + ")";

        repaint();
    }


    // =============================================================
    // PROCESSAR MIDI RECEBIDO PELA SERIAL
    // =============================================================

    void handleArduinoMidiMessage(
        uint8_t status,
        uint8_t data1,
        uint8_t data2)
    {
        const int type = status & 0xF0;
        const int channel = status & 0x0F;

        // NOTE ON

        if (type == 0x90 && data2 > 0)
        {
            const juce::ScopedLock lock(engineLock);

            engine.noteOn(channel, data1, data2);
        }

        // NOTE OFF
        // Também trata NOTE ON com velocity zero como NOTE OFF.

        else if (type == 0x80 || (type == 0x90 && data2 == 0))
        {
            const juce::ScopedLock lock(engineLock);

            engine.noteOff(channel, data1);
        }

        // CONTROL CHANGE 1 = MODULATION

        else if (type == 0xB0 && data1 == 1)
        {
            {
                const juce::ScopedLock lock(engineLock);

                engine.setModulation(data2);
            }

            // Atualiza o slider na thread gráfica.

            juce::Component::SafePointer<MainComponent> safeThis(this);

            juce::MessageManager::callAsync(
                [safeThis, value = static_cast<int>(data2)]()
                {
                    if (safeThis == nullptr)
                        return;

                    safeThis->currentModulation = value;

                    safeThis->modulationSlider.setValue(
                        value,
                        juce::dontSendNotification);
                });
        }

        // Os demais CCs são ignorados por enquanto.
    }


    // =============================================================
    // RECEBER MIDI CONVENCIONAL
    // =============================================================

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
                message.getVelocity());
        }
        else if (message.isNoteOff())
        {
            engine.noteOff(
                message.getChannel() - 1,
                message.getNoteNumber());
        }
        else if (message.isPitchWheel())
        {
            engine.setPitchBend(
                message.getPitchWheelValue());

            juce::Component::SafePointer<MainComponent> safeThis(this);

            juce::MessageManager::callAsync(
                [safeThis, value = message.getPitchWheelValue()]()
                {
                    if (safeThis == nullptr)
                        return;

                    safeThis->currentPitchBend = value;

                    safeThis->pitchBendSlider.setValue(
                        value,
                        juce::dontSendNotification);
                });
        }
        else if (message.isController() &&
                 message.getControllerNumber() == 1)
        {
            engine.setModulation(
                message.getControllerValue());

            juce::Component::SafePointer<MainComponent> safeThis(this);

            juce::MessageManager::callAsync(
                [safeThis, value = message.getControllerValue()]()
                {
                    if (safeThis == nullptr)
                        return;

                    safeThis->currentModulation = value;

                    safeThis->modulationSlider.setValue(
                        value,
                        juce::dontSendNotification);
                });
        }
    }


    // =============================================================
    // PIANO VIRTUAL
    // =============================================================

    void handleNoteOn(
        juce::MidiKeyboardState*,
        int midiChannel,
        int midiNoteNumber,
        float velocity) override
    {
        juce::ignoreUnused(midiChannel);

        const juce::ScopedLock lock(engineLock);

        engine.noteOn(
            midiNoteNumber,
            juce::roundToInt(velocity * 127.0f));
    }


    void handleNoteOff(
        juce::MidiKeyboardState*,
        int midiChannel,
        int midiNoteNumber,
        float velocity) override
    {
        juce::ignoreUnused(midiChannel, velocity);

        const juce::ScopedLock lock(engineLock);

        engine.noteOff(midiNoteNumber);
    }


    // =============================================================
    // ÁUDIO
    // =============================================================

    void prepareToPlay(
        int samplesPerBlockExpected,
        double sampleRate) override
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

        buffer->clear(
            bufferToFill.startSample,
            bufferToFill.numSamples);

        if (buffer->getNumChannels() >= 2)
        {
            {
                const juce::ScopedLock lock(engineLock);

                engine.render(
                    buffer->getWritePointer(
                        0, bufferToFill.startSample),

                    buffer->getWritePointer(
                        1, bufferToFill.startSample),

                    bufferToFill.numSamples);
            }

            // Volume principal aplicado à saída estéreo.

            const float gain = masterVolume.load();

            for (int channel = 0;
                 channel < buffer->getNumChannels();
                 ++channel)
            {
                buffer->applyGain(
                    channel,
                    bufferToFill.startSample,
                    bufferToFill.numSamples,
                    gain);
            }
        }
    }


    void releaseResources() override {}


    // =============================================================
    // INTERFACE GRÁFICA
    // =============================================================

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff171a20));

        // Cabeçalho

        g.setColour(juce::Colour(0xff222832));

        g.fillRect(0, 0, getWidth(), 75);

        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(28.0f);

        g.drawText(
            "SOUNDMODULE",
            30, 15, 300, 45,
            juce::Justification::centredLeft);

        g.setColour(juce::Colours::white);
        g.setFont(14.0f);

        g.drawText(
            "SF2 INSTRUMENT PLAYER",
            340, 25, 300, 30,
            juce::Justification::centredLeft);

        // Área de imagem da categoria do instrumento

        g.setColour(juce::Colour(0xff252d38));

        g.fillRoundedRectangle(
            30, 100,
            getWidth() - 60,
            115, 10.0f);

        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(12.0f);

        g.drawText(
            "MIDI INPUT",
            760, 165, 180, 18,
            juce::Justification::centredLeft);

        // Biblioteca

        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(15.0f);

        g.drawText(
            "SOUNDFONT LIBRARY",
            30, 225, 300, 22,
            juce::Justification::centredLeft);

        g.setColour(juce::Colours::lightgrey);
        g.setFont(12.0f);

        g.drawText(
            libraryStatusText,
            30, 275, 650, 22,
            juce::Justification::centredLeft);

        g.setColour(juce::Colours::white);
        g.setFont(12.0f);

        g.drawText(
            "Selecionado: " + selectedSoundFontName,
            30, 298, 650, 20,
            juce::Justification::centredLeft);

        // Partes

        const int partY = 365;
        const int partHeight = 85;
        const int gap = 20;
        const int partWidth = (getWidth() - 80) / 3;

        for (int i = 0; i < 3; ++i)
        {
            const int x = 30 + i * (partWidth + gap);

            g.setColour(juce::Colour(0xff252d38));

            g.fillRoundedRectangle(
                static_cast<float>(x),
                static_cast<float>(partY),
                static_cast<float>(partWidth),
                static_cast<float>(partHeight),
                10.0f);

            g.setColour(juce::Colour(0xff55c8ff));
            g.setFont(18.0f);

            g.drawText(
                "PART " + juce::String(i + 1),
                x + 15, partY + 8,
                partWidth - 30, 25,
                juce::Justification::centredLeft);

            g.setColour(juce::Colours::white);
            g.setFont(18.0f);

            g.drawText(
                i == 0 ? "Grand Piano" : "Empty",
                x + 15, partY + 42,
                partWidth - 30, 27,
                juce::Justification::centredLeft);
        }

        // Painel do teclado

        g.setColour(juce::Colour(0xff252d38));

        g.fillRoundedRectangle(
            30, 460,
            getWidth() - 60,
            145, 10.0f);

        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(18.0f);

        g.drawText(
            "VIRTUAL KEYBOARD",
            175, 470, 300, 30,
            juce::Justification::centredLeft);

        g.setColour(juce::Colours::lightgrey);
        g.setFont(11.0f);

        g.drawText(
            "C2 - B6",
            750, 475, 120, 20,
            juce::Justification::centredRight);

        // Títulos dos controles

        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(11.0f);

        g.drawText(
            "PITCH",
            45, 480, 50, 20,
            juce::Justification::centred);

        g.drawText(
            "MOD",
            100, 480, 50, 20,
            juce::Justification::centred);

        // Volume principal

        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(12.0f);

        g.drawText(
            "MASTER",
            890, 480, 80, 20,
            juce::Justification::centred);

        g.setColour(juce::Colours::white);
        g.setFont(12.0f);

        g.drawText(
            juce::String(
                juce::roundToInt(masterVolume.load() * 100.0f)) + "%",
            890, 585, 80, 18,
            juce::Justification::centred);

        // Rodapé

        g.setColour(juce::Colour(0xff55c8ff));
        g.setFont(13.0f);

        g.drawText(
            midiStatusText,
            35, 645, 180, 25,
            juce::Justification::centredLeft);

        g.drawText(
            "AUDIO: READY",
            220, 645, 150, 25,
            juce::Justification::centredLeft);
    }


    void resized() override
    {
        instrumentImage.setBounds(30, 100, 200, 115);

        midiInputSelector.setBounds(
            760, 185, 180, 30);

        soundFontSelector.setBounds(
            30, 250, 500, 30);

        loadButton.setBounds(
            545, 250, 110, 32);

        scanButton.setBounds(
            665, 250, 110, 32);

        addButton.setBounds(
            785, 250, 150, 32);

        removeButton.setBounds(
            785, 285, 150, 28);

        // Cinco botões de preset

        const int presetX = 30;
        const int presetY = 320;
        const int presetWidth = 174;
        const int presetHeight = 32;
        const int presetGap = 15;

        for (int i = 0; i < NUM_PRESETS; ++i)
        {
            presetButtons[static_cast<size_t>(i)].setBounds(
                presetX + i * (presetWidth + presetGap),
                presetY,
                presetWidth,
                presetHeight);
        }

        // Teclado virtual

        virtualKeyboard.setBounds(
            175, 510, 700, 85);

        // Pitch Bend e Modulation

        pitchBendSlider.setBounds(
            55, 510, 28, 85);

        modulationSlider.setBounds(
            110, 510, 28, 85);

        // Knob principal

        masterVolumeSlider.setBounds(
            895, 500, 70, 85);
    }


private:

    // =============================================================
    // MOTOR
    // =============================================================

    FluidSynthEngine engine;

    juce::CriticalSection engineLock;

    bool audioReady = false;

    int currentPitchBend = 8192;
    int currentModulation = 0;

    std::atomic<float> masterVolume { 1.0f };


    // =============================================================
    // TECLADO
    // =============================================================

    juce::MidiKeyboardState keyboardState;

    juce::MidiKeyboardComponent virtualKeyboard {
        keyboardState,
        juce::MidiKeyboardComponent::horizontalKeyboard
    };


    // =============================================================
    // CONTROLES
    // =============================================================

    juce::Slider pitchBendSlider;
    juce::Slider modulationSlider;
    juce::Slider masterVolumeSlider;


    // =============================================================
    // BOTÕES
    // =============================================================

    juce::TextButton loadButton { "CARREGAR" };
    juce::TextButton addButton { "ADICIONAR SF2" };
    juce::TextButton scanButton { "ATUALIZAR" };
    juce::TextButton removeButton { "REMOVER DA LISTA" };

    std::array<PresetButton, NUM_PRESETS> presetButtons;


    // =============================================================
    // SELETORES
    // =============================================================

    juce::ComboBox soundFontSelector;
    juce::ComboBox midiInputSelector;


    // =============================================================
    // MIDI
    // =============================================================

    juce::Array<juce::MidiDeviceInfo> midiDevices;

    std::unique_ptr<juce::MidiInput> midiInput;

    // Entrada serial do Arduino

    ArduinoSerialMidiInput arduinoSerial;

    juce::StringArray serialPorts;


    // =============================================================
    // ARQUIVOS
    // =============================================================

    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::File soundFontsFolder;
    juce::File libraryManifest;
    juce::File ignoredManifest;

    // Arquivo de categorias

    juce::File categoryManifest;
    juce::File instrumentImagesFolder;

    juce::ImageComponent instrumentImage;

    juce::StringPairArray soundFontCategories;

    std::vector<juce::File> soundFontFiles;


    // =============================================================
    // TEXTOS
    // =============================================================

    juce::String selectedSoundFontName =
        "Nenhum SoundFont selecionado";

    juce::String libraryStatusText =
        "Procurando SoundFonts...";

    juce::String midiStatusText =
        "MIDI: DISCONNECTED";
};


// =============================================================
// APLICAÇÃO
// =============================================================

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

            setContentOwned(
                new MainComponent(),
                true);

            centreWithSize(1000, 700);

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
