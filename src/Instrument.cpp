/* ==========================================================================
 *
 * iMWrap v6 - A modern iMWrap implementation attempt with Adventure Game Studio Companion Plugin
 *
 * This program is the legal property of Masami Komuro and few other contributors,
 * Please refer to the COPYRIGHT file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ========================================================================== */

#include "imwrap/Instrument.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>

namespace imwrap {
namespace {

std::unordered_map<std::string, uint8_t> g_dynamicRolandToGmMap;
std::mutex g_mappingMutex;

struct RolandProgramMapEntry {
    const char *name;
    uint8_t program;
};

static const RolandProgramMapEntry kRolandToGmMap[] = {
    {"badspit   ", 62},
    {"Big Drum  ", 116},
    {"burp      ", 58},
    {"foghorn   ", 60},
    {"glop      ", 39},
    {"LeshBass  ", 33},
    {"ML explosn", 127},
    {"ReggaeBass", 32},
    {"rumble    ", 89},
    {"SdTrk Bend", 97},
    {"spitting  ", 62},
    {"Swell 1   ", 95},
    {"Swell 2   ", 95},
    {"thnderclap", 127},
};

static const uint8_t kMt32ToGm[128] = {
      0,   1,   0,   2,   4,   4,   5,   3,  16,  17,  18,  16,  16,  19,  20,  21,
      6,   6,   6,   7,   7,   7,   8, 112,  62,  62,  63,  63,  38,  38,  39,  39,
     88,  95,  52,  98,  97,  99,  14,  54, 102,  96,  53, 102,  81, 100,  14,  80,
     48,  48,  49,  45,  41,  40,  42,  42,  43,  46,  45,  24,  25,  28,  27, 104,
     32,  32,  34,  33,  36,  37,  35,  35,  79,  73,  72,  72,  74,  75,  64,  65,
     66,  67,  71,  71,  68,  69,  70,  22,  56,  59,  57,  57,  60,  60,  58,  61,
     61,  11,  11,  98,  14,   9,  14,  13,  12, 107, 107,  77,  78,  78,  76,  76,
     47, 117, 127, 118, 118, 116, 115, 119, 115, 112,  55, 124, 123,   0,  14, 117
};

static const uint8_t kGmToMt32[128] = {
      5,   1,   2,   7,   3,   5,  16,  21,  22, 101, 101,  97, 104, 103, 102,  20,
      8,   9,  11,  12,  14,  15,  87,  15,  59,  60,  61,  62,  67,  44,  79,  23,
     64,  67,  66,  70,  68,  69,  28,  31,  52,  54,  55,  56,  49,  51,  57, 112,
     48,  50,  45,  26,  34,  35,  45, 122,  89,  90,  94,  81,  92,  95,  24,  25,
     80,  78,  79,  78,  84,  85,  86,  82,  74,  72,  76,  77, 110, 107, 108,  76,
     47,  44, 111,  45,  44,  34,  44,  30,  32,  33,  88,  34,  35,  35,  38,  33,
     41,  36, 100,  37,  40,  34,  43,  40,  63,  21,  99, 105, 103,  86,  55,  84,
    101, 103, 100, 120, 117, 113,  99, 128, 128, 128, 128, 124, 123, 128, 128, 128
};

class ProgramInstrument final : public InstrumentInternal {
public:
    ProgramInstrument(uint8_t program, uint8_t bank, bool soundTypeMt32, bool nativeMt32Device)
        : _program(program),
          _bank(bank),
          _soundTypeMt32(soundTypeMt32),
          _nativeMt32Device(nativeMt32Device) {
        if (_program > 127) {
            _program = 255;
        }
    }

    void send(MidiChannel *channel) override {
        if (!channel || _program > 127) {
            return;
        }

        uint8_t program = _program;
        if (!_nativeMt32Device && _soundTypeMt32) {
            program = kMt32ToGm[program];
        } else if (_nativeMt32Device && !_soundTypeMt32) {
            program = kGmToMt32[program];
        }

        if (_bank) {
            channel->bankSelect(_bank);
        }
        if (program < 128) {
            channel->programChange(program);
        }
        if (_bank) {
            channel->bankSelect(0);
        }
    }

    void copyTo(Instrument *dest) override {
        if (dest) {
            dest->program(_program, _bank, _soundTypeMt32);
        }
    }

    bool isValid() const override {
        if (_program >= 128) {
            return false;
        }
        if (_nativeMt32Device == _soundTypeMt32) {
            return true;
        }
        return _nativeMt32Device ? (kGmToMt32[_program] < 128) : (kMt32ToGm[_program] < 128);
    }

private:
    uint8_t _program = 255;
    uint8_t _bank = 0;
    bool _soundTypeMt32 = false;
    bool _nativeMt32Device = false;
};

class AdLibInstrument final : public InstrumentInternal {
public:
    explicit AdLibInstrument(const uint8_t *data) {
        if (data) {
            std::memcpy(_instrument.data(), data, _instrument.size());
        }
    }

    void send(MidiChannel *channel) override {
        if (channel) {
            channel->sysExCustomInstrument('ADL ', ByteView(_instrument.data(), _instrument.size()));
        }
    }

    void copyTo(Instrument *dest) override {
        if (dest) {
            dest->adlib(_instrument.data());
        }
    }

    bool isValid() const override {
        return true;
    }

private:
    std::array<uint8_t, 30> _instrument = {{}};
};

class RolandInstrument final : public InstrumentInternal {
public:
    RolandInstrument(const uint8_t *data, bool nativeMt32Device)
        : _nativeMt32Device(nativeMt32Device) {
        if (data) {
            std::memcpy(_instrument.data(), data, _instrument.size());
        }
        std::memcpy(_name.data(), _instrument.data() + 7, 10);
        _name[10] = '\0';
        if (!_nativeMt32Device && equivalentGmProgram() >= 128) {
            _name[0] = '\0';
        }
    }

    void send(MidiChannel *channel) override {
        if (!channel) {
            return;
        }

        if (_nativeMt32Device) {
            channel->sysExCustomInstrument('ROL ', ByteView(_instrument.data(), _instrument.size()));
            if (_instrument.size() >= 7) {
                uint8_t am = _instrument[5]; // Middle address is the temporary timbre index
                uint8_t program = 64 + am; // MT-32 selects temp timbres with program changes 64-127
                if (program < 128) {
                    channel->programChange(program);
                }
            }
        } else {
            const uint8_t program = equivalentGmProgram();
            if (program < 128) {
                channel->programChange(program);
            }
        }
    }

    void copyTo(Instrument *dest) override {
        if (dest) {
            dest->roland(_instrument.data());
        }
    }

    bool isValid() const override {
        return _nativeMt32Device || _name[0] != '\0';
    }

private:
    uint8_t equivalentGmProgram() const {
        {
            std::lock_guard<std::mutex> lock(g_mappingMutex);
            auto it = g_dynamicRolandToGmMap.find(_name.data());
            if (it != g_dynamicRolandToGmMap.end()) {
                return it->second;
            }
        }

        for (const RolandProgramMapEntry &entry : kRolandToGmMap) {
            if (std::memcmp(entry.name, _name.data(), 10) == 0) {
                return entry.program;
            }
        }
        return 255;
    }

    std::array<uint8_t, 258> _instrument = {{}};
    std::array<char, 11> _name = {{}};
    bool _nativeMt32Device = false;
};

class PcSpeakerInstrument final : public InstrumentInternal {
public:
    explicit PcSpeakerInstrument(const uint8_t *data) {
        if (data) {
            std::memcpy(_instrument.data(), data, _instrument.size());
        }
    }

    void send(MidiChannel *channel) override {
        if (channel) {
            channel->sysExCustomInstrument('SPK ', ByteView(_instrument.data(), _instrument.size()));
        }
    }

    void copyTo(Instrument *dest) override {
        if (dest) {
            dest->pcspk(_instrument.data());
        }
    }

    bool isValid() const override {
        return true;
    }

private:
    std::array<uint8_t, 23> _instrument = {{}};
};

} // namespace

const uint8_t Instrument::kGmRhythmMap[35] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 36, 37, 38, 39, 40, 41, 66, 47,
    65, 48, 56
};

void Instrument::clear() {
    _instrument.reset();
    _type = None;
}

void Instrument::copyTo(Instrument *dest) const {
    if (!dest) {
        return;
    }

    dest->clear();
    if (_instrument) {
        _instrument->copyTo(dest);
    }
}

void Instrument::program(uint8_t program, uint8_t bank, bool mt32SoundType) {
    clear();
    if (program > 127) {
        return;
    }
    _type = Program;
    _instrument = std::make_unique<ProgramInstrument>(program, bank, mt32SoundType, _nativeMT32Device);
}

void Instrument::adlib(const uint8_t *instrument) {
    clear();
    if (!instrument) {
        return;
    }
    _type = AdLib;
    _instrument = std::make_unique<AdLibInstrument>(instrument);
}

void Instrument::roland(const uint8_t *instrument) {
    clear();
    if (!instrument) {
        return;
    }
    _type = Roland;
    _instrument = std::make_unique<RolandInstrument>(instrument, _nativeMT32Device);
}

void Instrument::pcspk(const uint8_t *instrument) {
    clear();
    if (!instrument) {
        return;
    }
    _type = PcSpk;
    _instrument = std::make_unique<PcSpeakerInstrument>(instrument);
}

bool Instrument::isValid() const {
    return _instrument && _instrument->isValid();
}

void Instrument::send(MidiChannel *channel) const {
    if (_instrument) {
        _instrument->send(channel);
    }
}

void RegisterRolandTimbreMapping(const char *name, uint8_t gmProgram) {
    if (!name) {
        return;
    }
    
    std::string formattedName(name);
    if (formattedName.size() < 10) {
        formattedName.append(10 - formattedName.size(), ' ');
    } else if (formattedName.size() > 10) {
        formattedName = formattedName.substr(0, 10);
    }
    
    std::lock_guard<std::mutex> lock(g_mappingMutex);
    g_dynamicRolandToGmMap[formattedName] = gmProgram;
}

void ClearRolandTimbreMappings() {
    std::lock_guard<std::mutex> lock(g_mappingMutex);
    g_dynamicRolandToGmMap.clear();
}

} // namespace imwrap
