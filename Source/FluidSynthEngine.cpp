
#include "FluidSynthEngine.h"

#include <algorithm>

FluidSynthEngine::FluidSynthEngine()
{
    for (int i = 0; i < NUM_PARTS; ++i)
        parts[i].channel = i;
}

FluidSynthEngine::~FluidSynthEngine()
{
    if (synth != nullptr)
        delete_fluid_synth(synth);

    if (settings != nullptr)
        delete_fluid_settings(settings);
}

bool FluidSynthEngine::initialize(double sampleRate)
{
    if (initialized)
        return true;

    settings = new_fluid_settings();

    if (settings == nullptr)
        return false;

    fluid_settings_setnum(settings, "synth.sample-rate", sampleRate);
    fluid_settings_setnum(settings, "synth.gain", 0.8);
    fluid_settings_setint(settings, "synth.polyphony", 256);

    synth = new_fluid_synth(settings);

    if (synth == nullptr)
        return false;

    initialized = true;
    return true;
}

bool FluidSynthEngine::isValidPart(int partIndex) const
{
    return partIndex >= 0 && partIndex < NUM_PARTS;
}

bool FluidSynthEngine::loadPartSoundFont(
    int partIndex,
    const char* filePath)
{
    if (!initialized || !isValidPart(partIndex) || filePath == nullptr)
        return false;

    Part& part = parts[partIndex];

    std::string requestedPath(filePath);

    int soundFontId = -1;

    // Verifica se o SoundFont já está carregado.
    for (const auto& loaded : loadedSoundFonts)
    {
        if (loaded.path == requestedPath)
        {
            soundFontId = loaded.id;
            break;
        }
    }

    // Carrega somente se ainda não estiver na memória.
    if (soundFontId < 0)
    {
        soundFontId = fluid_synth_sfload(
            synth,
            filePath,
            0
        );

        if (soundFontId == FLUID_FAILED)
            return false;

        loadedSoundFonts.push_back(
            { requestedPath, soundFontId }
        );
    }

    fluid_sfont_t* soundFont =
        fluid_synth_get_sfont_by_id(synth, soundFontId);

    if (soundFont == nullptr)
        return false;

    fluid_sfont_iteration_start(soundFont);

    fluid_preset_t* preset =
        fluid_sfont_iteration_next(soundFont);

    if (preset == nullptr)
        return false;

    int bank = fluid_preset_get_banknum(preset);
    int presetNumber = fluid_preset_get_num(preset);

    stopPartNotes(partIndex);

    if (fluid_synth_program_select(
        synth,
        part.channel,
        soundFontId,
        bank,
        presetNumber) != FLUID_OK)
    {
        return false;
    }

    part.soundFontId = soundFontId;
    part.soundFontPath = requestedPath;
    part.bank = bank;
    part.preset = presetNumber;
    part.enabled = true;

    fluid_synth_cc(synth, part.channel, 7, part.volume);

    return true;
}

bool FluidSynthEngine::selectPreset(
    int partIndex,
    int bank,
    int presetNumber)
{
    if (!initialized || !isValidPart(partIndex))
        return false;

    Part& part = parts[partIndex];

    if (part.soundFontId < 0)
        return false;

    stopPartNotes(partIndex);

    int result = fluid_synth_program_select(
        synth,
        part.channel,
        part.soundFontId,
        bank,
        presetNumber
    );

    if (result != FLUID_OK)
        return false;

    part.bank = bank;
    part.preset = presetNumber;

    return true;
}

std::vector<FluidSynthEngine::PresetInfo>
FluidSynthEngine::getPresets(int partIndex)
{
    std::vector<PresetInfo> result;

    if (!initialized || !isValidPart(partIndex))
        return result;

    const Part& part = parts[partIndex];

    if (part.soundFontId < 0)
        return result;

    fluid_sfont_t* soundFont =
        fluid_synth_get_sfont_by_id(synth, part.soundFontId);

    if (soundFont == nullptr)
        return result;

    fluid_sfont_iteration_start(soundFont);

    fluid_preset_t* preset = nullptr;

    while ((preset = fluid_sfont_iteration_next(soundFont)) != nullptr)
    {
        PresetInfo info;

        info.bank = fluid_preset_get_banknum(preset);
        info.number = fluid_preset_get_num(preset);

        const char* name = fluid_preset_get_name(preset);

        if (name != nullptr)
            info.name = name;

        result.push_back(info);
    }

    return result;
}

void FluidSynthEngine::setPartEnabled(
    int partIndex,
    bool enabled)
{
    if (!initialized || !isValidPart(partIndex))
        return;

    Part& part = parts[partIndex];

    if (part.soundFontId < 0)
        return;

    if (!enabled)
        stopPartNotes(partIndex);

    part.enabled = enabled;
}

void FluidSynthEngine::setPartVolume(
    int partIndex,
    int volume)
{
    if (!isValidPart(partIndex))
        return;

    Part& part = parts[partIndex];

    part.volume = std::clamp(volume, 0, 127);

    if (initialized && part.soundFontId >= 0)
    {
        fluid_synth_cc(
            synth,
            part.channel,
            7,
            part.volume
        );
    }
}

void FluidSynthEngine::setPartKeyRange(
    int partIndex,
    int lowestNote,
    int highestNote)
{
    if (!isValidPart(partIndex))
        return;

    Part& part = parts[partIndex];

    part.lowestNote = std::clamp(lowestNote, 0, 127);
    part.highestNote = std::clamp(highestNote, 0, 127);

    if (part.lowestNote > part.highestNote)
        std::swap(part.lowestNote, part.highestNote);
}

FluidSynthEngine::PartState
FluidSynthEngine::getPartState(int partIndex) const
{
    PartState state;

    if (!isValidPart(partIndex))
        return state;

    const Part& part = parts[partIndex];

    state.soundFontPath = part.soundFontPath;
    state.bank = part.bank;
    state.preset = part.preset;
    state.volume = part.volume;
    state.lowestNote = part.lowestNote;
    state.highestNote = part.highestNote;
    state.enabled = part.enabled;

    return state;
}

bool FluidSynthEngine::restorePartState(
    int partIndex,
    const PartState& state)
{
    if (!isValidPart(partIndex))
        return false;

    if (state.soundFontPath.empty())
        return false;

    if (!loadPartSoundFont(
        partIndex,
        state.soundFontPath.c_str()))
    {
        return false;
    }

    if (!selectPreset(
        partIndex,
        state.bank,
        state.preset))
    {
        return false;
    }

    setPartVolume(partIndex, state.volume);

    setPartKeyRange(
        partIndex,
        state.lowestNote,
        state.highestNote
    );

    setPartEnabled(partIndex, state.enabled);

    return true;
}

void FluidSynthEngine::setPlayMode(PlayMode mode)
{
    playMode = mode;

    for (int i = 0; i < NUM_PARTS; ++i)
        stopPartNotes(i);
}

FluidSynthEngine::PlayMode FluidSynthEngine::getPlayMode() const
{
    return playMode;
}

void FluidSynthEngine::setSplitPoint(int note)
{
    splitPoint = std::clamp(note, 0, 127);
}

int FluidSynthEngine::getSplitPoint() const
{
    return splitPoint;
}

void FluidSynthEngine::noteOn(int note, int velocity)
{
    if (!initialized)
        return;

    note = std::clamp(note, 0, 127);
    velocity = std::clamp(velocity, 0, 127);

    if (velocity == 0)
    {
        noteOff(note);
        return;
    }

    for (int i = 0; i < NUM_PARTS; ++i)
    {
        Part& part = parts[i];

        if (!part.enabled || part.soundFontId < 0)
            continue;

        bool shouldPlay = false;

        if (playMode == PlayMode::Layer)
        {
            shouldPlay = true;
        }
        else
        {
            shouldPlay =
                note >= part.lowestNote &&
                note <= part.highestNote;
        }

        if (!shouldPlay)
            continue;

        fluid_synth_noteon(
            synth,
            part.channel,
            note,
            velocity
        );

        activeNotes[i][note] = true;
    }
}

void FluidSynthEngine::noteOff(int note)
{
    if (!initialized)
        return;

    note = std::clamp(note, 0, 127);

    for (int i = 0; i < NUM_PARTS; ++i)
    {
        if (!activeNotes[i][note])
            continue;

        fluid_synth_noteoff(
            synth,
            parts[i].channel,
            note
        );

        activeNotes[i][note] = false;
    }
}

void FluidSynthEngine::stopPartNotes(int partIndex)
{
    if (!isValidPart(partIndex))
        return;

    activeNotes[partIndex].fill(false);

    if (!initialized)
        return;

    Part& part = parts[partIndex];

    if (part.soundFontId >= 0)
        fluid_synth_all_sounds_off(synth, part.channel);
}

bool FluidSynthEngine::loadSoundFont(const char* filePath)
{
    return loadPartSoundFont(0, filePath);
}

void FluidSynthEngine::noteOn(
    int channel,
    int note,
    int velocity)
{
    (void)channel;
    noteOn(note, velocity);
}

void FluidSynthEngine::noteOff(int channel, int note)
{
    (void)channel;
    noteOff(note);
}

void FluidSynthEngine::setPitchBend(int value)
{
    if (!initialized)
        return;

    value = std::clamp(value, 0, 16383);

    for (int i = 0; i < NUM_PARTS; ++i)
    {
        fluid_synth_pitch_bend(
            synth,
            parts[i].channel,
            value
        );
    }
}

void FluidSynthEngine::setModulation(int value)
{
    if (!initialized)
        return;

    value = std::clamp(value, 0, 127);

    for (int i = 0; i < NUM_PARTS; ++i)
    {
        fluid_synth_cc(
            synth,
            parts[i].channel,
            1,
            value
        );
    }
}

void FluidSynthEngine::render(
    float* left,
    float* right,
    int numSamples)
{
    if (!initialized || left == nullptr || right == nullptr)
        return;

    fluid_synth_write_float(
        synth,
        numSamples,
        left, 0, 1,
        right, 0, 1
    );
}
