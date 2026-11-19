
#pragma once

#include <fluidsynth.h>

#include <array>
#include <string>
#include <vector>

class FluidSynthEngine
{
public:
    static constexpr int NUM_PARTS = 3;

    enum class PlayMode
    {
        Layer,
        Split
    };

    struct PresetInfo
    {
        int bank = 0;
        int number = 0;
        std::string name;
    };

    // Estado completo de uma parte para os presets
    struct PartState
    {
        std::string soundFontPath;
        int bank = 0;
        int preset = 0;
        int volume = 100;
        int lowestNote = 0;
        int highestNote = 127;
        bool enabled = false;
    };

    FluidSynthEngine();
    ~FluidSynthEngine();

    bool initialize(double sampleRate);

    bool loadPartSoundFont(int partIndex, const char* filePath);

    bool selectPreset(int partIndex, int bank, int presetNumber);

    std::vector<PresetInfo> getPresets(int partIndex);

    void setPartEnabled(int partIndex, bool enabled);
    void setPartVolume(int partIndex, int volume);
    void setPartKeyRange(int partIndex, int lowestNote, int highestNote);

    // Recuperação e armazenamento de partes
    PartState getPartState(int partIndex) const;
    bool restorePartState(int partIndex, const PartState& state);

    void setPlayMode(PlayMode mode);
    PlayMode getPlayMode() const;

    void setSplitPoint(int note);
    int getSplitPoint() const;

    void noteOn(int note, int velocity);
    void noteOff(int note);

    bool loadSoundFont(const char* filePath);

    void noteOn(int channel, int note, int velocity);
    void noteOff(int channel, int note);

    void setPitchBend(int value);
    void setModulation(int value);

    void render(float* left, float* right, int numSamples);

private:

    struct Part
    {
        int soundFontId = -1;
        int channel = 0;

        int bank = 0;
        int preset = 0;

        int volume = 100;

        int lowestNote = 0;
        int highestNote = 127;

        bool enabled = false;

        std::string soundFontPath;
    };

    struct LoadedSoundFont
    {
        std::string path;
        int id = -1;
    };

    fluid_settings_t* settings = nullptr;
    fluid_synth_t* synth = nullptr;

    std::array<Part, NUM_PARTS> parts;

    std::array<std::array<bool, 128>, NUM_PARTS> activeNotes {};

    std::vector<LoadedSoundFont> loadedSoundFonts;

    PlayMode playMode = PlayMode::Layer;

    int splitPoint = 60;

    bool initialized = false;

    bool isValidPart(int partIndex) const;

    void stopPartNotes(int partIndex);
};
