#include "SysExToolController.h"
#include "SysExToolIds.h"
#include "SysExToolProcessor.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory_constexpr.h"

#define stringPluginName "iMWrap SysEx Tool"

BEGIN_FACTORY_DEF(stringCompanyName, stringCompanyWeb, stringCompanyEmail, 3)

DEF_CLASS(imwrap::vst3tool::SysExToolProcessorUID, Steinberg::PClassInfo::kManyInstances,
          kVstAudioEffectClass, stringPluginName, Steinberg::Vst::kDistributable,
          Steinberg::Vst::PlugType::kInstrumentSynth, FULL_VERSION_STR, kVstVersionString,
          imwrap::vst3tool::SysExToolProcessor::createInstance, nullptr)

DEF_CLASS(imwrap::vst3tool::SysExToolControllerUID, Steinberg::PClassInfo::kManyInstances,
          kVstComponentControllerClass, stringPluginName " Controller", 0, "",
          FULL_VERSION_STR, kVstVersionString,
          imwrap::vst3tool::SysExToolController::createInstance, nullptr)

END_FACTORY
