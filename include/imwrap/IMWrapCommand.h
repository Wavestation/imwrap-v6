/* ==========================================================================
 *
 * iMWrap v6 - A modern iMWrap implementation attempt with Adventure Game Studio Companion Plugin
 *
 * This program is the legal property of Masami Komuro and few other contributors,
 * Please refer to the COPYRIGHT file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ========================================================================== */

#ifndef IMWRAP_IMWRAP_COMMAND_H
#define IMWRAP_IMWRAP_COMMAND_H

#include <array>
#include <cstdint>

namespace imwrap {

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

} // namespace imwrap

#endif
