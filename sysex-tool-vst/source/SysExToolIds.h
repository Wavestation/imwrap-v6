#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace imwrap::vst3tool {

enum ParameterId : Steinberg::Vst::ParamID {
    kSendId = 1000,
    kMessageTypeId,
    kPartId,
    kChannelId,
    kUnknownId,
    kPartOnId,
    kReverbId,
    kPriorityId,
    kVolumeId,
    kPanId,
    kPercussionId,
    kTransposeId,
    kDetuneId,
    kPitchbendFactorId,
    kProgramId,
    kParamId,
    kParamValueId,
    kHookCommandId,
    kTargetTrackId,
    kTargetBeatId,
    kTargetTickId,
    kRelativeId,
    kHookValueId,
    kLoopCountId,
    kLoopToBeatId,
    kLoopToTickId,
    kLoopFromBeatId,
    kLoopFromTickId,
    kInstrumentId
};

static DECLARE_UID(SysExToolProcessorUID, 0xA6C0F3D2, 0xB4084D26, 0xA7A5F4D1, 0x8A0C92B1);
static DECLARE_UID(SysExToolControllerUID, 0x8F9D50F8, 0x4C4D4897, 0xA13D4B64, 0x717A14F6);

inline constexpr char kSendRequestMessageId[] = "imwrap.sysex.send";
inline constexpr char kSendRequestStateKey[] = "state";

} // namespace imwrap::vst3tool
