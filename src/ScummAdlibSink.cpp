#include "imwrap/ScummAdlibSink.h"

#include "../third_party/libadlmidi/include/adlmidi.h"
#include "../third_party/libadlmidi/src/chips/mame_opl2.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <utility>
#include <vector>

namespace imwrap {
namespace {

constexpr std::size_t kAdlibChannels = 16;
constexpr std::size_t kAdlibVoices = 9;
constexpr std::size_t kMidiNotes = 128;
constexpr int kCallbackFrequency = 250;
constexpr int kTimerIncrease = 0x0D69;
constexpr int kTimerThreshold = 0x411B;
constexpr uint8_t kPercussionChannel = 9;
constexpr uint8_t kInvalidPercussionInstrument = 0xFF;

template <typename T>
T ClampValue(T value, T minValue, T maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

void SetError(std::string* error, const char* message) {
    if (error) {
        *error = message ? message : "";
    }
}

struct InstrumentExtra {
    uint8_t a = 0;
    uint8_t b = 0;
    uint8_t c = 0;
    uint8_t d = 0;
    uint8_t e = 0;
    uint8_t f = 0;
    uint8_t g = 0;
    uint8_t h = 0;
};

struct AdlibInstrument {
    uint8_t modCharacteristic = 0;
    uint8_t modScalingOutputLevel = 0;
    uint8_t modAttackDecay = 0;
    uint8_t modSustainRelease = 0;
    uint8_t modWaveformSelect = 0;
    uint8_t carCharacteristic = 0;
    uint8_t carScalingOutputLevel = 0;
    uint8_t carAttackDecay = 0;
    uint8_t carSustainRelease = 0;
    uint8_t carWaveformSelect = 0;
    uint8_t feedback = 0;
    uint8_t flagsA = 0;
    InstrumentExtra extraA = {};
    uint8_t flagsB = 0;
    InstrumentExtra extraB = {};
    uint8_t duration = 0;
};

struct Struct10 {
    uint8_t active = 0;
    int16_t curVal = 0;
    int16_t count = 0;
    uint16_t maxValue = 0;
    int16_t startValue = 0;
    uint8_t loop = 0;
    uint8_t tableA[4] = {};
    uint8_t tableB[4] = {};
    int8_t unk3 = 0;
    int8_t modWheel = 0;
    int8_t modWheelLast = 0;
    uint16_t speedLoMax = 0;
    uint16_t numSteps = 0;
    int16_t speedHi = 0;
    int8_t direction = 0;
    uint16_t speedLo = 0;
    uint16_t speedLoCounter = 0;
};

struct Struct11 {
    int16_t modifyVal = 0;
    uint8_t param = 0;
    uint8_t flag0x40 = 0;
    uint8_t flag0x10 = 0;
    Struct10* s10 = nullptr;
};

struct Voice {
    bool active = false;
    uint8_t channel = 0;
    uint8_t slot = 0;
    uint8_t note = 0;
    uint8_t twoChan = 0;
    uint8_t vol1 = 0;
    uint8_t vol2 = 0;
    bool waitForPedal = false;
    int16_t duration = 0;
    uint64_t age = 0;
    Struct10 s10a = {};
    Struct11 s11a = {};
    Struct10 s10b = {};
    Struct11 s11b = {};
};

enum class ChannelMode {
    Generic,
    Custom
};

struct ChannelState {
    ChannelMode mode = ChannelMode::Generic;
    bool hasCustomInstrument = false;
    AdlibInstrument instrument = {};
    int16_t pitchBend = 0;
    uint8_t pitchBendRange = 2;
    uint8_t volume = 127;
    uint8_t modWheel = 0;
    bool sustain = false;
    uint8_t bankMsb = 0;
    uint8_t bankLsb = 0;
    uint8_t rpnMsb = 0x7F;
    uint8_t rpnLsb = 0x7F;
};

struct PercussionState {
    std::array<uint8_t, kMidiNotes> mappedNotes = {};
    std::array<AdlibInstrument, kMidiNotes> instruments = {};
    std::array<uint8_t, kMidiNotes> hasCustomInstrument = {};
};

struct AdlibSetParams {
    uint8_t registerBase;
    uint8_t shift;
    uint8_t mask;
    uint8_t inversion;
};

constexpr std::array<uint8_t, kAdlibVoices> kOperator1Offsets = {{
    0, 1, 2, 8, 9, 10, 16, 17, 18
}};

constexpr std::array<uint8_t, kAdlibVoices> kOperator2Offsets = {{
    3, 4, 5, 11, 12, 13, 19, 20, 21
}};

constexpr std::array<AdlibSetParams, 15> kSetParamTable = {{
    {0x40, 0, 63, 63},
    {0xE0, 2, 0, 0},
    {0x40, 6, 192, 0},
    {0x20, 0, 15, 0},
    {0x60, 4, 240, 15},
    {0x60, 0, 15, 15},
    {0x80, 4, 240, 15},
    {0x80, 0, 15, 15},
    {0xE0, 0, 3, 0},
    {0x20, 7, 128, 0},
    {0x20, 6, 64, 0},
    {0x20, 5, 32, 0},
    {0x20, 4, 16, 0},
    {0xC0, 0, 1, 0},
    {0xC0, 1, 14, 0}
}};

constexpr std::array<uint8_t, 16> kParamTable1 = {{
    29, 28, 27, 0, 3, 4, 7, 8, 13, 16, 17, 20, 21, 30, 31, 0
}};

constexpr std::array<uint16_t, 16> kMaxValTable = {{
    0x2FF, 0x1F, 0x7, 0x3F, 0x0F, 0x0F, 0x0F, 0x3,
    0x3F, 0x0F, 0x0F, 0x0F, 0x3, 0x3E, 0x1F, 0
}};

constexpr std::array<uint16_t, 32> kNumStepsTable = {{
    1, 2, 4, 5, 6, 7, 8, 9,
    10, 12, 14, 16, 18, 21, 24, 30,
    36, 50, 64, 82, 100, 136, 160, 192,
    240, 276, 340, 460, 600, 860, 1200, 1600
}};

constexpr std::array<uint8_t, 192> kNoteFrequencies = {{
    90, 91, 92, 92, 93, 94, 94, 95, 96, 96, 97, 98, 98, 99, 100, 101,
    101, 102, 103, 104, 104, 105, 106, 107, 107, 108, 109, 110, 111, 111, 112, 113,
    114, 115, 115, 116, 117, 118, 119, 120, 121, 121, 122, 123, 124, 125, 126, 127,
    128, 129, 130, 131, 132, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142,
    143, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 157, 158, 159, 160,
    161, 162, 163, 165, 166, 167, 168, 169, 171, 172, 173, 174, 176, 177, 178, 180,
    181, 182, 184, 185, 186, 188, 189, 190, 192, 193, 194, 196, 197, 199, 200, 202,
    203, 205, 206, 208, 209, 211, 212, 214, 215, 217, 218, 220, 222, 223, 225, 226,
    228, 230, 231, 233, 235, 236, 238, 240, 242, 243, 245, 247, 249, 251, 252, 254
}};

constexpr std::array<uint8_t, 64> kVolumeTable = {{
    0, 4, 7, 11, 13, 16, 18, 20,
    22, 24, 26, 27, 29, 30, 31, 33,
    34, 35, 36, 37, 38, 39, 40, 41,
    42, 43, 44, 44, 45, 46, 47, 47,
    48, 49, 49, 50, 51, 51, 52, 53,
    53, 54, 54, 55, 55, 56, 56, 57,
    57, 58, 58, 59, 59, 60, 60, 60,
    61, 61, 62, 62, 62, 63, 63, 63
}};

void LoadAdlibInstrumentCore(AdlibInstrument& instrument, const uint8_t* data) {
    instrument.modCharacteristic = data[0];
    instrument.modScalingOutputLevel = data[1];
    instrument.modAttackDecay = data[2];
    instrument.modSustainRelease = data[3];
    instrument.modWaveformSelect = data[4];
    instrument.carCharacteristic = data[5];
    instrument.carScalingOutputLevel = data[6];
    instrument.carAttackDecay = data[7];
    instrument.carSustainRelease = data[8];
    instrument.carWaveformSelect = data[9];
    instrument.feedback = data[10];
}

#include "ScummAdlibPercussionData.inl"

} // namespace

struct ScummAdlibSink::Impl {
    std::recursive_mutex mutex;
    uint32_t sampleRate = 0;
    bool started = false;
    std::unique_ptr<MameOPL2> opl;
    ADL_MIDIPlayer* genericPlayer = nullptr;
    std::vector<float> genericBuffer;
    std::array<uint8_t, 256> regCache = {};
    std::array<ChannelState, kAdlibChannels> channels = {};
    PercussionState percussion = {};
    std::array<Voice, kAdlibVoices> voices = {};
    std::array<int16_t, kAdlibVoices> channelTable2 = {};
    std::array<uint16_t, kAdlibVoices> curNoteTable = {};
    std::array<std::array<uint8_t, 32>, 64> volumeLookup = {};
    std::size_t nextVoiceIndex = 0;
    uint64_t voiceAgeCounter = 0;
    int timerCounter = 0;
    double callbackIntervalFrames = 0.0;
    double callbackCountdownFrames = 0.0;
    uint8_t randomSeed = 1;

    void clearState() {
        regCache.fill(0);
        channelTable2.fill(0);
        curNoteTable.fill(0);
        channels.fill(ChannelState{});
        percussion = PercussionState{};
        nextVoiceIndex = 0;
        voiceAgeCounter = 0;
        timerCounter = 0;
        randomSeed = 1;
        for (std::size_t note = 0; note < percussion.mappedNotes.size(); ++note) {
            percussion.mappedNotes[note] = static_cast<uint8_t>(note);
        }
        for (std::size_t index = 0; index < voices.size(); ++index) {
            voices[index] = Voice{};
            voices[index].slot = static_cast<uint8_t>(index);
            voices[index].s11a.s10 = &voices[index].s10b;
            voices[index].s11b.s10 = &voices[index].s10a;
        }
    }

    void initVolumeLookup() {
        for (int i = 0; i < 64; ++i) {
            int sum = i;
            for (int j = 0; j < 32; ++j) {
                volumeLookup[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = static_cast<uint8_t>(sum >> 5);
                sum += i;
            }
            volumeLookup[static_cast<std::size_t>(i)][0] = 0;
        }
    }

    static int lookupVolume(const std::array<std::array<uint8_t, 32>, 64>& table, int a, int b) {
        if (b == 0) {
            return 0;
        }
        if (b == 31) {
            return a;
        }
        if (a < -63 || a > 63) {
            return (b * (a + 1)) >> 5;
        }

        if (b < 0) {
            return (a < 0) ? table[static_cast<std::size_t>(-a)][static_cast<std::size_t>(-b)]
                           : -static_cast<int>(table[static_cast<std::size_t>(a)][static_cast<std::size_t>(-b)]);
        }

        return (a < 0) ? -static_cast<int>(table[static_cast<std::size_t>(-a)][static_cast<std::size_t>(b)])
                       : table[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)];
    }

    void resetChip() {
        if (!opl) {
            return;
        }
        opl->reset();
        regCache.fill(0);
        adlibWrite(0x08, 0x40);
        adlibWrite(0xBD, 0x00);
        adlibWrite(0x01, 0x20);
    }

    void startTimer() {
        callbackIntervalFrames = static_cast<double>(sampleRate) / static_cast<double>(kCallbackFrequency);
        callbackCountdownFrames = callbackIntervalFrames;
        timerCounter = 0;
    }

    void stopPlayers() {
        if (genericPlayer) {
            adl_close(genericPlayer);
            genericPlayer = nullptr;
        }
        opl.reset();
        started = false;
    }

    void adlibWrite(uint8_t reg, uint8_t value) {
        if (!opl) {
            return;
        }
        if (regCache[reg] == value) {
            return;
        }
        regCache[reg] = value;
        opl->writeReg(reg, value);
    }

    uint8_t adlibGetRegValue(uint8_t reg) const {
        return regCache[reg];
    }

    void adlibKeyOff(std::size_t channel) {
        const uint8_t reg = static_cast<uint8_t>(0xB0 + channel);
        adlibWrite(reg, static_cast<uint8_t>(adlibGetRegValue(reg) & ~0x20));
    }

    void adlibPlayNote(std::size_t channel, int note) {
        if (channel >= kAdlibVoices) {
            return;
        }

        uint8_t note2 = static_cast<uint8_t>((note >> 7) - 4);
        note2 = (note2 < 128) ? note2 : 0;

        uint8_t octave = static_cast<uint8_t>(note2 / 12);
        if (octave > 7) {
            octave = 7 << 2;
        } else {
            octave = static_cast<uint8_t>(octave << 2);
        }

        uint8_t noteIndex = static_cast<uint8_t>((note2 % 12) + 3);
        uint8_t old = adlibGetRegValue(static_cast<uint8_t>(channel + 0xB0));
        if (old & 0x20) {
            old &= ~0x20;
            if (octave > old) {
                if (noteIndex < 6) {
                    noteIndex = static_cast<uint8_t>(noteIndex + 12);
                    octave = static_cast<uint8_t>(octave - 4);
                }
            } else if (octave < old) {
                if (noteIndex > 11) {
                    noteIndex = static_cast<uint8_t>(noteIndex - 12);
                    octave = static_cast<uint8_t>(octave + 4);
                }
            }
        }

        const std::size_t freqIndex = static_cast<std::size_t>((noteIndex << 3) + ((note >> 4) & 0x7));
        if (freqIndex >= kNoteFrequencies.size()) {
            return;
        }

        adlibWrite(static_cast<uint8_t>(channel + 0xA0), kNoteFrequencies[freqIndex]);
        adlibWrite(static_cast<uint8_t>(channel + 0xB0), static_cast<uint8_t>(octave | 0x20));
    }

    void adlibNoteOn(std::size_t channel, uint8_t note, int mod) {
        if (channel >= kAdlibVoices) {
            return;
        }
        const int code = (static_cast<int>(note) << 7) + mod;
        curNoteTable[channel] = static_cast<uint16_t>(code);
        adlibPlayNote(channel, static_cast<int>(channelTable2[channel]) + code);
    }

    void adlibNoteOnEx(std::size_t channel, uint8_t note, int mod) {
        if (channel >= kAdlibVoices) {
            return;
        }
        const int code = (static_cast<int>(note) << 7) + mod;
        curNoteTable[channel] = static_cast<uint16_t>(code);
        channelTable2[channel] = 0;
        adlibPlayNote(channel, code);
    }

    int randomNr(int limit) {
        if (randomSeed & 1) {
            randomSeed >>= 1;
            randomSeed ^= 0xB8;
        } else {
            randomSeed >>= 1;
        }
        return (randomSeed * limit) >> 8;
    }

    Voice* allocateVoice() {
        for (std::size_t attempt = 0; attempt < voices.size(); ++attempt) {
            nextVoiceIndex = (nextVoiceIndex + 1) % voices.size();
            Voice& voice = voices[nextVoiceIndex];
            if (!voice.active) {
                return &voice;
            }
        }

        auto best = std::min_element(voices.begin(), voices.end(), [](const Voice& lhs, const Voice& rhs) {
            return lhs.age < rhs.age;
        });
        if (best == voices.end()) {
            return nullptr;
        }
        releaseVoice(*best);
        return &(*best);
    }

    void releaseVoice(Voice& voice) {
        if (!voice.active) {
            return;
        }
        adlibKeyOff(voice.slot);
        const uint8_t slot = voice.slot;
        voice = Voice{};
        voice.slot = slot;
        voice.s11a.s10 = &voice.s10b;
        voice.s11b.s10 = &voice.s10a;
    }

    void struct10Setup(Struct10& s10) {
        int b = s10.unk3;
        const int phase = static_cast<int>(s10.active) - 1;

        const uint8_t tableA = s10.tableA[phase];
        int steps = kNumStepsTable[volumeLookup[static_cast<std::size_t>(tableA & 0x7F)][static_cast<std::size_t>(b)]];
        if (tableA & 0x80) {
            steps = randomNr(steps);
        }
        if (steps == 0) {
            steps = 1;
        }

        s10.numSteps = static_cast<uint16_t>(steps);
        s10.speedLoMax = static_cast<uint16_t>(steps);

        int delta = 0;
        if (phase != 2) {
            const int maxValue = s10.maxValue;
            const int startValue = s10.startValue;
            const uint8_t tableB = s10.tableB[phase];
            int candidate = lookupVolume(volumeLookup, maxValue, (tableB & 0x7F) - 31);
            if (tableB & 0x80) {
                candidate = randomNr(candidate);
            }

            int limited = 0;
            if (candidate + startValue > maxValue) {
                limited = maxValue - startValue;
            } else {
                limited = candidate;
                if (candidate + startValue < 0) {
                    limited = -startValue;
                }
            }
            delta = limited - s10.curVal;
        }

        s10.speedHi = static_cast<int16_t>(delta / steps);
        if (delta < 0) {
            delta = -delta;
            s10.direction = -1;
        } else {
            s10.direction = 1;
        }

        s10.speedLo = static_cast<uint16_t>(delta % steps);
        s10.speedLoCounter = 0;
    }

    uint8_t struct10OnTimer(Struct10& s10, Struct11& s11) {
        uint8_t result = 0;
        if (s10.count && (s10.count = static_cast<int16_t>(s10.count - 17)) <= 0) {
            s10.active = 0;
            return 0;
        }

        int current = s10.curVal + s10.speedHi;
        s10.speedLoCounter = static_cast<uint16_t>(s10.speedLoCounter + s10.speedLo);
        if (s10.speedLoCounter >= s10.speedLoMax) {
            s10.speedLoCounter = static_cast<uint16_t>(s10.speedLoCounter - s10.speedLoMax);
            current += s10.direction;
        }

        if (s10.curVal != current || s10.modWheel != s10.modWheelLast) {
            s10.curVal = static_cast<int16_t>(current);
            s10.modWheelLast = s10.modWheel;
            const int adjusted = lookupVolume(volumeLookup, current, s10.modWheelLast);
            if (adjusted != s11.modifyVal) {
                s11.modifyVal = static_cast<int16_t>(adjusted);
                result = 1;
            }
        }

        if (--s10.numSteps == 0) {
            ++s10.active;
            if (s10.active > 4) {
                if (s10.loop) {
                    s10.active = 1;
                    result |= 2;
                    struct10Setup(s10);
                } else {
                    s10.active = 0;
                }
            } else {
                struct10Setup(s10);
            }
        }

        return result;
    }

    int adlibGetRegValueParam(std::size_t channel, uint8_t param) {
        if (channel >= kAdlibVoices) {
            return 0;
        }

        uint8_t regChannel = 0;
        uint8_t effectiveParam = param;
        if (effectiveParam <= 12) {
            regChannel = kOperator2Offsets[channel];
        } else if (effectiveParam <= 25) {
            effectiveParam = static_cast<uint8_t>(effectiveParam - 13);
            regChannel = kOperator1Offsets[channel];
        } else if (effectiveParam <= 27) {
            effectiveParam = static_cast<uint8_t>(effectiveParam - 13);
            regChannel = static_cast<uint8_t>(channel);
        } else if (effectiveParam == 28) {
            return 0x0F;
        } else if (effectiveParam == 29) {
            return 0x017F;
        } else {
            return 0;
        }

        const AdlibSetParams& params = kSetParamTable[effectiveParam];
        uint8_t value = static_cast<uint8_t>(adlibGetRegValue(static_cast<uint8_t>(regChannel + params.registerBase)) & params.mask);
        value = static_cast<uint8_t>(value >> params.shift);
        if (params.inversion) {
            value = static_cast<uint8_t>(params.inversion - value);
        }
        return value;
    }

    void adlibSetParam(std::size_t channel, uint8_t param, int value) {
        if (channel >= kAdlibVoices) {
            return;
        }

        uint8_t reg = 0;
        uint8_t effectiveParam = param;
        if (effectiveParam <= 12) {
            reg = kOperator2Offsets[channel];
        } else if (effectiveParam <= 25) {
            effectiveParam = static_cast<uint8_t>(effectiveParam - 13);
            reg = kOperator1Offsets[channel];
        } else if (effectiveParam <= 27) {
            effectiveParam = static_cast<uint8_t>(effectiveParam - 13);
            reg = static_cast<uint8_t>(channel);
        } else if (effectiveParam == 28 || effectiveParam == 29) {
            if (effectiveParam == 28) {
                value -= 15;
            } else {
                value -= 383;
            }
            value *= 16;
            channelTable2[channel] = static_cast<int16_t>(value);
            adlibPlayNote(channel, static_cast<int>(curNoteTable[channel]) + value);
            return;
        } else {
            return;
        }

        const AdlibSetParams& params = kSetParamTable[effectiveParam];
        if (params.inversion) {
            value = params.inversion - value;
        }

        reg = static_cast<uint8_t>(reg + params.registerBase);
        const uint8_t current = adlibGetRegValue(reg);
        const uint8_t next = static_cast<uint8_t>((current & ~params.mask) | ((static_cast<uint8_t>(value) << params.shift) & params.mask));
        adlibWrite(reg, next);
    }

    void adlibKeyOnOff(std::size_t channel) {
        if (channel >= kAdlibVoices) {
            return;
        }
        const uint8_t reg = static_cast<uint8_t>(channel + 0xB0);
        const uint8_t current = adlibGetRegValue(reg);
        adlibWrite(reg, static_cast<uint8_t>(current & ~0x20));
        adlibWrite(reg, static_cast<uint8_t>(current | 0x20));
    }

    void adlibSetupChannel(std::size_t slot, const AdlibInstrument& instrument, uint8_t vol1, uint8_t vol2) {
        if (slot >= kAdlibVoices) {
            return;
        }

        uint8_t channel = kOperator1Offsets[slot];
        adlibWrite(static_cast<uint8_t>(channel + 0x20), instrument.modCharacteristic);
        adlibWrite(static_cast<uint8_t>(channel + 0x40), static_cast<uint8_t>((instrument.modScalingOutputLevel | 0x3F) - vol1));
        adlibWrite(static_cast<uint8_t>(channel + 0x60), static_cast<uint8_t>(~instrument.modAttackDecay));
        adlibWrite(static_cast<uint8_t>(channel + 0x80), static_cast<uint8_t>(~instrument.modSustainRelease));
        adlibWrite(static_cast<uint8_t>(channel + 0xE0), instrument.modWaveformSelect);

        channel = kOperator2Offsets[slot];
        adlibWrite(static_cast<uint8_t>(channel + 0x20), instrument.carCharacteristic);
        adlibWrite(static_cast<uint8_t>(channel + 0x40), static_cast<uint8_t>((instrument.carScalingOutputLevel | 0x3F) - vol2));
        adlibWrite(static_cast<uint8_t>(channel + 0x60), static_cast<uint8_t>(~instrument.carAttackDecay));
        adlibWrite(static_cast<uint8_t>(channel + 0x80), static_cast<uint8_t>(~instrument.carSustainRelease));
        adlibWrite(static_cast<uint8_t>(channel + 0xE0), instrument.carWaveformSelect);

        adlibWrite(static_cast<uint8_t>(slot + 0xC0), instrument.feedback);
    }

    void struct10Init(Struct10& s10, const InstrumentExtra& extra) {
        s10.active = 1;
        s10.curVal = 0;
        s10.modWheelLast = 31;
        s10.count = extra.a ? static_cast<int16_t>(extra.a * 63) : 0;
        s10.tableA[0] = extra.b;
        s10.tableA[1] = extra.d;
        s10.tableA[2] = extra.f;
        s10.tableA[3] = extra.g;
        s10.tableB[0] = extra.c;
        s10.tableB[1] = extra.e;
        s10.tableB[2] = 0;
        s10.tableB[3] = extra.h;
        struct10Setup(s10);
    }

    void mcInitStuff(Voice& voice, Struct10& s10, Struct11& s11, uint8_t flags, const InstrumentExtra& extra) {
        ChannelState& channel = channels[voice.channel];
        s11.modifyVal = 0;
        s11.flag0x40 = static_cast<uint8_t>(flags & 0x40);
        s10.loop = static_cast<uint8_t>(flags & 0x20);
        s11.flag0x10 = static_cast<uint8_t>(flags & 0x10);
        s11.param = kParamTable1[flags & 0x0F];
        s10.maxValue = kMaxValTable[flags & 0x0F];
        s10.unk3 = 31;
        s10.modWheel = s11.flag0x40 ? static_cast<int8_t>(channel.modWheel >> 2) : 31;

        switch (s11.param) {
        case 0:
            s10.startValue = voice.vol2;
            break;
        case 13:
            s10.startValue = voice.vol1;
            break;
        case 30:
            s10.startValue = 31;
            if (s11.s10) {
                s11.s10->modWheel = 0;
            }
            break;
        case 31:
            s10.startValue = 0;
            if (s11.s10) {
                s11.s10->unk3 = 0;
            }
            break;
        default:
            s10.startValue = static_cast<int16_t>(adlibGetRegValueParam(voice.slot, s11.param));
            break;
        }

        struct10Init(s10, extra);
    }

    void mcIncStuff(Voice& voice, Struct10& s10, Struct11& s11) {
        const uint8_t code = struct10OnTimer(s10, s11);
        if (code & 1) {
            switch (s11.param) {
            case 0: {
                voice.vol2 = static_cast<uint8_t>(ClampValue<int>(s10.startValue + s11.modifyVal, 0, 63));
                const uint8_t volumeIndex = static_cast<uint8_t>(channels[voice.channel].volume >> 2);
                adlibSetParam(voice.slot, 0, kVolumeTable[volumeLookup[voice.vol2][volumeIndex]]);
                break;
            }
            case 13: {
                voice.vol1 = static_cast<uint8_t>(ClampValue<int>(s10.startValue + s11.modifyVal, 0, 63));
                if (voice.twoChan) {
                    const uint8_t volumeIndex = static_cast<uint8_t>(channels[voice.channel].volume >> 2);
                    adlibSetParam(voice.slot, 13, kVolumeTable[volumeLookup[voice.vol1][volumeIndex]]);
                } else {
                    adlibSetParam(voice.slot, 13, voice.vol1);
                }
                break;
            }
            case 30:
                if (s11.s10) {
                    s11.s10->modWheel = static_cast<int8_t>(s11.modifyVal);
                }
                break;
            case 31:
                if (s11.s10) {
                    s11.s10->unk3 = static_cast<int8_t>(s11.modifyVal);
                }
                break;
            default:
                adlibSetParam(voice.slot, s11.param, s10.startValue + s11.modifyVal);
                break;
            }
        }

        if ((code & 2) && s11.flag0x10) {
            adlibKeyOnOff(voice.slot);
        }
    }

    void stepAutomation() {
        for (Voice& voice : voices) {
            if (!voice.active) {
                continue;
            }

            if (voice.duration && (voice.duration = static_cast<int16_t>(voice.duration - 0x11)) <= 0) {
                releaseVoice(voice);
                continue;
            }

            if (voice.s10a.active) {
                mcIncStuff(voice, voice.s10a, voice.s11a);
            }
            if (voice.s10b.active) {
                mcIncStuff(voice, voice.s10b, voice.s11b);
            }
        }
    }

    void callbackTick() {
        timerCounter += kTimerIncrease;
        while (timerCounter >= kTimerThreshold) {
            timerCounter -= kTimerThreshold;
            stepAutomation();
        }
    }

    void updateChannelVolume(uint8_t channelIndex) {
        const uint8_t channelVolume = static_cast<uint8_t>(channels[channelIndex].volume >> 2);
        for (Voice& voice : voices) {
            if (!voice.active || voice.channel != channelIndex) {
                continue;
            }
            adlibSetParam(voice.slot, 0, kVolumeTable[volumeLookup[voice.vol2][channelVolume]]);
            if (voice.twoChan) {
                adlibSetParam(voice.slot, 13, kVolumeTable[volumeLookup[voice.vol1][channelVolume]]);
            }
        }
    }

    void updateChannelPitchBend(uint8_t channelIndex) {
        const ChannelState& channel = channels[channelIndex];
        for (Voice& voice : voices) {
            if (!voice.active || voice.channel != channelIndex) {
                continue;
            }
            const int mod = (channel.pitchBend * channel.pitchBendRange) >> 6;
            adlibNoteOn(voice.slot, voice.note, mod);
        }
    }

    void updateChannelModWheel(uint8_t channelIndex) {
        const int8_t modWheel = static_cast<int8_t>(channels[channelIndex].modWheel >> 2);
        for (Voice& voice : voices) {
            if (!voice.active || voice.channel != channelIndex) {
                continue;
            }
            if (voice.s10a.active && voice.s11a.flag0x40) {
                voice.s10a.modWheel = modWheel;
            }
            if (voice.s10b.active && voice.s11b.flag0x40) {
                voice.s10b.modWheel = modWheel;
            }
        }
    }

    void noteOffCustom(uint8_t channelIndex, uint8_t note, bool allowSustain = true) {
        for (Voice& voice : voices) {
            if (!voice.active || voice.channel != channelIndex || voice.note != note) {
                continue;
            }
            if (allowSustain && channels[channelIndex].sustain) {
                voice.waitForPedal = true;
            } else {
                releaseVoice(voice);
            }
        }
    }

    void releaseSustain(uint8_t channelIndex) {
        for (Voice& voice : voices) {
            if (!voice.active || voice.channel != channelIndex || !voice.waitForPedal) {
                continue;
            }
            releaseVoice(voice);
        }
    }

    void allNotesOffCustom(uint8_t channelIndex) {
        for (Voice& voice : voices) {
            if (voice.active && voice.channel == channelIndex) {
                releaseVoice(voice);
            }
        }
    }

    void noteOnInstrument(uint8_t channelIndex, uint8_t note, uint8_t velocity, const AdlibInstrument& instrument) {
        ChannelState& channel = channels[channelIndex];
        Voice* voice = allocateVoice();
        if (!voice) {
            return;
        }

        const int rawVol1 = ClampValue<int>(
            (instrument.modScalingOutputLevel & 0x3F) + volumeLookup[velocity >> 1][instrument.modWaveformSelect >> 2],
            0,
            0x3F);
        const int rawVol2 = ClampValue<int>(
            (instrument.carScalingOutputLevel & 0x3F) + volumeLookup[velocity >> 1][instrument.carWaveformSelect >> 2],
            0,
            0x3F);

        voice->active = true;
        voice->channel = channelIndex;
        voice->note = note;
        voice->twoChan = static_cast<uint8_t>(instrument.feedback & 1);
        voice->vol1 = static_cast<uint8_t>(rawVol1);
        voice->vol2 = static_cast<uint8_t>(rawVol2);
        voice->waitForPedal = false;
        voice->duration = instrument.duration ? static_cast<int16_t>(instrument.duration * 63) : 0;
        voice->age = ++voiceAgeCounter;

        const uint8_t volumeIndex = static_cast<uint8_t>(channel.volume >> 2);
        const uint8_t outVol1 = voice->twoChan ? kVolumeTable[volumeLookup[voice->vol1][volumeIndex]] : voice->vol1;
        const uint8_t outVol2 = kVolumeTable[volumeLookup[voice->vol2][volumeIndex]];

        adlibSetupChannel(voice->slot, instrument, outVol1, outVol2);
        adlibNoteOnEx(voice->slot, note, (channel.pitchBend * channel.pitchBendRange) >> 6);

        if (instrument.flagsA & 0x80) {
            mcInitStuff(*voice, voice->s10a, voice->s11a, instrument.flagsA, instrument.extraA);
        } else {
            voice->s10a.active = 0;
        }

        if (instrument.flagsB & 0x80) {
            mcInitStuff(*voice, voice->s10b, voice->s11b, instrument.flagsB, instrument.extraB);
        } else {
            voice->s10b.active = 0;
        }
    }

    void noteOnCustom(uint8_t channelIndex, uint8_t note, uint8_t velocity) {
        ChannelState& channel = channels[channelIndex];
        if (!channel.hasCustomInstrument) {
            return;
        }
        noteOnInstrument(channelIndex, note, velocity, channel.instrument);
    }

    void noteOffPercussion(uint8_t note) {
        if (percussion.hasCustomInstrument[note]) {
            note = percussion.mappedNotes[note];
        }
        noteOffCustom(kPercussionChannel, note, false);
    }

    void noteOnPercussion(uint8_t note, uint8_t velocity) {
        const AdlibInstrument* instrument = nullptr;
        uint8_t playbackNote = note;

        if (percussion.hasCustomInstrument[note]) {
            instrument = &percussion.instruments[note];
            playbackNote = percussion.mappedNotes[note];
        } else {
            const uint8_t mappedInstrument = kGmPercussionInstrumentMap[note];
            if (mappedInstrument != kInvalidPercussionInstrument && mappedInstrument < kGmPercussionInstruments.size()) {
                instrument = &kGmPercussionInstruments[mappedInstrument];
            }
        }

        if (!instrument) {
            return;
        }

        noteOnInstrument(kPercussionChannel, playbackNote, velocity, *instrument);
    }

    void forwardGenericMidiMessage(uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) {
        if (!genericPlayer) {
            return;
        }

        const uint8_t messageType = status & 0xF0;
        const ADL_UInt8 channel = static_cast<ADL_UInt8>(status & 0x0F);
        if (channel == kPercussionChannel) {
            return;
        }
        switch (messageType) {
        case 0x80:
            adl_rt_noteOff(genericPlayer, channel, data1);
            break;
        case 0x90:
            if (!hasData2 || data2 == 0) {
                adl_rt_noteOff(genericPlayer, channel, data1);
            } else {
                adl_rt_noteOn(genericPlayer, channel, data1, data2);
            }
            break;
        case 0xA0:
            if (hasData2) {
                adl_rt_noteAfterTouch(genericPlayer, channel, data1, data2);
            }
            break;
        case 0xB0:
            if (hasData2) {
                adl_rt_controllerChange(genericPlayer, channel, data1, data2);
            }
            break;
        case 0xC0:
            adl_rt_patchChange(genericPlayer, channel, data1);
            break;
        case 0xD0:
            adl_rt_channelAfterTouch(genericPlayer, channel, data1);
            break;
        case 0xE0:
            if (hasData2) {
                adl_rt_pitchBendML(genericPlayer, channel, data2, data1);
            }
            break;
        default:
            break;
        }
    }

    void handleCustomController(uint8_t channelIndex, uint8_t controller, uint8_t value) {
        ChannelState& channel = channels[channelIndex];
        if (channelIndex == kPercussionChannel) {
            switch (controller) {
            case 7:
                channel.volume = value;
                updateChannelVolume(channelIndex);
                break;
            case 120:
            case 123:
                allNotesOffCustom(channelIndex);
                break;
            case 121:
                channel.modWheel = 0;
                channel.pitchBendRange = 2;
                channel.sustain = false;
                updateChannelModWheel(channelIndex);
                updateChannelPitchBend(channelIndex);
                break;
            default:
                break;
            }
            return;
        }

        switch (controller) {
        case 0:
            channel.bankMsb = value;
            break;
        case 1:
            channel.modWheel = value;
            updateChannelModWheel(channelIndex);
            break;
        case 6:
            if (channel.rpnMsb == 0 && channel.rpnLsb == 0) {
                channel.pitchBendRange = static_cast<uint8_t>(ClampValue<int>(value, 0, 12));
                updateChannelPitchBend(channelIndex);
            }
            break;
        case 7:
            channel.volume = value;
            updateChannelVolume(channelIndex);
            break;
        case 32:
            channel.bankLsb = value;
            break;
        case 64: {
            const bool wasSustain = channel.sustain;
            channel.sustain = value != 0;
            if (wasSustain && !channel.sustain) {
                releaseSustain(channelIndex);
            }
            break;
        }
        case 100:
            channel.rpnLsb = value;
            break;
        case 101:
            channel.rpnMsb = value;
            break;
        case 120:
        case 123:
            allNotesOffCustom(channelIndex);
            break;
        case 121:
            channel.modWheel = 0;
            channel.pitchBendRange = 0;
            channel.sustain = false;
            updateChannelModWheel(channelIndex);
            updateChannelPitchBend(channelIndex);
            releaseSustain(channelIndex);
            break;
        default:
            break;
        }
    }

    void frameTimerStep() {
        while (callbackCountdownFrames <= 0.0) {
            callbackTick();
            callbackCountdownFrames += callbackIntervalFrames;
        }
        callbackCountdownFrames -= 1.0;
    }
};

ScummAdlibSink::ScummAdlibSink()
    : impl_(std::make_unique<Impl>()) {}

ScummAdlibSink::~ScummAdlibSink() {
    stop();
}

bool ScummAdlibSink::start(uint32_t sampleRate, std::string* error) {
    stop();
    if (!impl_) {
        SetError(error, "AdLib backend is unavailable");
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    impl_->sampleRate = sampleRate ? sampleRate : 44100;
    impl_->initVolumeLookup();
    impl_->clearState();

    impl_->genericPlayer = adl_init(static_cast<long>(impl_->sampleRate));
    if (!impl_->genericPlayer) {
        SetError(error, "adl_init() failed");
        impl_->stopPlayers();
        return false;
    }

    adl_setVolumeRangeModel(impl_->genericPlayer, ADLMIDI_VolumeModel_DMX);
    adl_setBank(impl_->genericPlayer, 0);
    adl_setNumChips(impl_->genericPlayer, 1);

    impl_->opl = std::make_unique<MameOPL2>();
    impl_->opl->setRate(impl_->sampleRate);
    impl_->resetChip();
    impl_->startTimer();
    impl_->started = true;

    SetError(error, "");
    return true;
}

void ScummAdlibSink::stop() {
    if (!impl_) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    impl_->stopPlayers();
    impl_->genericBuffer.clear();
    impl_->clearState();
}

void ScummAdlibSink::render(float* interleavedStereo, uint32_t frameCount) {
    if (!interleavedStereo || frameCount == 0) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    std::fill(interleavedStereo, interleavedStereo + static_cast<std::size_t>(frameCount) * 2, 0.0f);
    if (!impl_->started) {
        return;
    }

    impl_->genericBuffer.assign(static_cast<std::size_t>(frameCount) * 2, 0.0f);
    if (impl_->genericPlayer) {
        ADLMIDI_AudioFormat format;
        format.type = ADLMIDI_SampleType_F32;
        format.containerSize = sizeof(float);
        format.sampleOffset = sizeof(float) * 2;

        adl_generateFormat(impl_->genericPlayer,
                           static_cast<int>(frameCount),
                           reinterpret_cast<ADL_UInt8*>(impl_->genericBuffer.data()),
                           reinterpret_cast<ADL_UInt8*>(impl_->genericBuffer.data() + 1),
                           &format);
    }

    for (uint32_t frame = 0; frame < frameCount; ++frame) {
        impl_->frameTimerStep();

        int16_t oplFrame[2] = {0, 0};
        if (impl_->opl) {
            impl_->opl->generate(oplFrame, 1);
        }

        const std::size_t offset = static_cast<std::size_t>(frame) * 2;
        const float left = impl_->genericBuffer[offset] + (static_cast<float>(oplFrame[0]) / 32768.0f);
        const float right = impl_->genericBuffer[offset + 1] + (static_cast<float>(oplFrame[1]) / 32768.0f);
        interleavedStereo[offset] = ClampValue(left, -1.0f, 1.0f);
        interleavedStereo[offset + 1] = ClampValue(right, -1.0f, 1.0f);
    }
}

bool ScummAdlibSink::isAvailable() const {
    return impl_ && impl_->started;
}

void ScummAdlibSink::onMidiMessage(uint16_t, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) {
    if (!impl_) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (!impl_->started) {
        return;
    }

    const uint8_t messageType = status & 0xF0;
    const uint8_t channelIndex = status & 0x0F;
    if (channelIndex >= kAdlibChannels) {
        return;
    }

    ChannelState& channel = impl_->channels[channelIndex];
    const bool isPercussionChannel = channelIndex == kPercussionChannel;
    switch (messageType) {
    case 0x80:
        if (isPercussionChannel) {
            impl_->noteOffPercussion(data1);
        } else {
            impl_->noteOffCustom(channelIndex, data1);
            impl_->forwardGenericMidiMessage(status, data1, true, hasData2 ? data2 : 0);
        }
        break;
    case 0x90:
        if (!hasData2 || data2 == 0) {
            if (isPercussionChannel) {
                impl_->noteOffPercussion(data1);
            } else {
                impl_->noteOffCustom(channelIndex, data1);
                impl_->forwardGenericMidiMessage(status, data1, true, 0);
            }
            break;
        }
        if (isPercussionChannel) {
            impl_->noteOnPercussion(data1, data2);
        } else if (channel.mode == ChannelMode::Custom && channel.hasCustomInstrument) {
            impl_->noteOnCustom(channelIndex, data1, data2);
        } else {
            impl_->forwardGenericMidiMessage(status, data1, true, data2);
        }
        break;
    case 0xA0:
        if (!isPercussionChannel) {
            impl_->forwardGenericMidiMessage(status, data1, hasData2, data2);
        }
        break;
    case 0xB0:
        if (hasData2) {
            impl_->handleCustomController(channelIndex, data1, data2);
        }
        if (!isPercussionChannel) {
            impl_->forwardGenericMidiMessage(status, data1, hasData2, data2);
        }
        break;
    case 0xC0:
        if (!isPercussionChannel) {
            channel.mode = ChannelMode::Generic;
            impl_->forwardGenericMidiMessage(status, data1, false, 0);
        }
        break;
    case 0xD0:
        if (!isPercussionChannel) {
            impl_->forwardGenericMidiMessage(status, data1, false, 0);
        }
        break;
    case 0xE0:
        if (hasData2) {
            channel.pitchBend = static_cast<int16_t>(((static_cast<int>(data2) << 7) | data1) - 0x2000);
            impl_->updateChannelPitchBend(channelIndex);
        }
        if (!isPercussionChannel) {
            impl_->forwardGenericMidiMessage(status, data1, hasData2, data2);
        }
        break;
    default:
        break;
    }
}

void ScummAdlibSink::onSysEx(uint16_t, ByteView message) {
    if (!impl_) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (!impl_->started || !impl_->genericPlayer || message.empty()) {
        return;
    }

    std::vector<uint8_t> sysex;
    sysex.reserve(message.size() + 2);
    if (static_cast<uint8_t>(message.data()[0]) != 0xF0) {
        sysex.push_back(0xF0);
    }
    sysex.insert(sysex.end(), message.data(), message.data() + message.size());
    if (sysex.empty() || sysex.back() != 0xF7) {
        sysex.push_back(0xF7);
    }
    adl_rt_systemExclusive(impl_->genericPlayer, sysex.data(), sysex.size());
}

void ScummAdlibSink::onCustomInstrument(uint16_t, uint8_t channel, uint32_t type, ByteView data) {
    if (!impl_) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (!impl_->started || channel >= kAdlibChannels) {
        return;
    }

    if (type == 'ADLP') {
        if (channel != kPercussionChannel || data.size() < 13) {
            return;
        }

        const uint8_t sourceNote = data.data()[0];
        if (sourceNote >= kMidiNotes) {
            return;
        }

        impl_->percussion.mappedNotes[sourceNote] = data.data()[1];
        impl_->percussion.instruments[sourceNote] = AdlibInstrument{};
        LoadAdlibInstrumentCore(impl_->percussion.instruments[sourceNote], data.data() + 2);
        impl_->percussion.hasCustomInstrument[sourceNote] = 1;
        return;
    }

    if (type != 'ADL ' || data.size() < 30) {
        return;
    }

    if (channel == kPercussionChannel) {
        return;
    }

    ChannelState& state = impl_->channels[channel];
    state.mode = ChannelMode::Custom;
    state.hasCustomInstrument = true;
    state.instrument = AdlibInstrument{};
    LoadAdlibInstrumentCore(state.instrument, data.data());
    state.instrument.flagsA = data.data()[11];
    state.instrument.extraA = InstrumentExtra{
        data.data()[12], data.data()[13], data.data()[14], data.data()[15],
        data.data()[16], data.data()[17], data.data()[18], data.data()[19]
    };
    state.instrument.flagsB = data.data()[20];
    state.instrument.extraB = InstrumentExtra{
        data.data()[21], data.data()[22], data.data()[23], data.data()[24],
        data.data()[25], data.data()[26], data.data()[27], data.data()[28]
    };
    state.instrument.duration = data.data()[29];
}

void ScummAdlibSink::onAllNotesOff() {
    if (!impl_) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (!impl_->started) {
        return;
    }

    if (impl_->genericPlayer) {
        adl_rt_resetState(impl_->genericPlayer);
        adl_setVolumeRangeModel(impl_->genericPlayer, ADLMIDI_VolumeModel_DMX);
        adl_setBank(impl_->genericPlayer, 0);
        adl_setNumChips(impl_->genericPlayer, 1);
    }

    impl_->clearState();
    impl_->resetChip();
    impl_->startTimer();
}

} // namespace imwrap
