#ifndef IMUSE_INSTRUMENT_H
#define IMUSE_INSTRUMENT_H

#include <cstdint>
#include <memory>

#include "imuse/MidiChannel.h"

namespace imuse {

class Instrument;

class InstrumentInternal {
public:
    virtual ~InstrumentInternal() = default;
    virtual void send(MidiChannel *channel) = 0;
    virtual void copyTo(Instrument *dest) = 0;
    virtual bool isValid() const = 0;
};

class Instrument {
public:
    enum Type : uint8_t {
        None = 0,
        Program = 1,
        AdLib = 2,
        Roland = 3,
        PcSpk = 4
    };

    Instrument() = default;
    Instrument(const Instrument &other) : _nativeMT32Device(other._nativeMT32Device) {
        other.copyTo(this);
    }
    Instrument &operator=(const Instrument &other) {
        if (this != &other) {
            clear();
            _nativeMT32Device = other._nativeMT32Device;
            other.copyTo(this);
        }
        return *this;
    }
    Instrument(Instrument &&) noexcept = default;
    Instrument &operator=(Instrument &&) noexcept = default;
    ~Instrument() = default;

    void setNativeMT32Mode(bool isNativeMT32) { _nativeMT32Device = isNativeMT32; }
    static const uint8_t kGmRhythmMap[35];

    void clear();
    void copyTo(Instrument *dest) const;

    void program(uint8_t program, uint8_t bank, bool mt32SoundType);
    void adlib(const uint8_t *instrument);
    void roland(const uint8_t *instrument);
    void pcspk(const uint8_t *instrument);

    Type type() const { return _type; }
    bool isValid() const;
    void send(MidiChannel *channel) const;

private:
    Type _type = None;
    std::unique_ptr<InstrumentInternal> _instrument;
    bool _nativeMT32Device = false;
};

void RegisterRolandTimbreMapping(const char *name, uint8_t gmProgram);
void ClearRolandTimbreMappings();

} // namespace imuse

#endif
