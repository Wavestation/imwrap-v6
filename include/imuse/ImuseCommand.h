#ifndef IMUSE_IMUSE_COMMAND_H
#define IMUSE_IMUSE_COMMAND_H

#include <array>
#include <cstdint>

namespace imuse {

enum class Command : uint16_t {
    SetMasterVolume    = 0x0006,
    StartSound         = 0x0008,
    StopSound          = 0x0009,
    StopAllSounds      = 0x000B,
    GetSoundStatus     = 0x000D,
    SetVolchan         = 0x0010,
    SetChannelVolume   = 0x0011,
    SetVolchanEntry    = 0x0012,
    ClearTrigger       = 0x0013,

    PlayerGetParam     = 0x0100,
    PlayerSetPriority  = 0x0101,
    PlayerSetVolume    = 0x0102,
    PlayerSetPan       = 0x0103,
    PlayerSetTranspose = 0x0104,
    PlayerSetDetune    = 0x0105,
    PlayerSetSpeed     = 0x0106,
    PlayerJump         = 0x0107,
    PlayerScan         = 0x0108,
    PlayerSetLoop      = 0x0109,
    PlayerClearLoop    = 0x010A,
    PlayerSetOnOff     = 0x010B,
    PlayerSetHook      = 0x010C,
    PlayerFade         = 0x010D,
    QueueTrigger       = 0x010E,
    QueueCommand       = 0x010F,
    ClearQueue         = 0x0110,
    PlayerSetPartVolume = 0x0116,
    QueryQueue         = 0x0117
};

struct CommandPacket {
    uint16_t argc = 0;
    std::array<int16_t, 8> args = {{0, 0, 0, 0, 0, 0, 0, 0}};
};

} // namespace imuse

#endif
