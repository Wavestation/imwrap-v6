#ifndef IMUSE_INSTRUMENT_STATE_H
#define IMUSE_INSTRUMENT_STATE_H

#include <cstdint>
#include <vector>

namespace imuse {

enum class InstrumentKind {
    None,
    Program,
    ImuseSetInstrument,
    RolandSysEx,
    AdlibPart,
    AdlibGlobal
};

struct InstrumentState {
    InstrumentKind kind = InstrumentKind::None;
    uint8_t program = 0;
    uint8_t bank = 0;
    uint16_t imuseInstrument = 0;
    std::vector<uint8_t> rawData;
    bool dirty = false;

    void clear() {
        kind = InstrumentKind::None;
        program = 0;
        bank = 0;
        imuseInstrument = 0;
        rawData.clear();
        dirty = false;
    }
};

} // namespace imuse

#endif
