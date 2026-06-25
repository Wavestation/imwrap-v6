# Third-Party Licenses

This file is a maintainer-facing index of the third-party license texts that are
currently present in the repository. It is not legal advice.

## License texts bundled in the tree

- FluidSynth source:
  - `third_party/fluidsynth-master/LICENSE`
  - `third_party/fluidsynth-master/sf2/COPYRIGHT.txt`
- libADLMIDI core:
  - `third_party/libadlmidi/LICENSE`
  - `third_party/libadlmidi/LICENSE.GPL-3.txt`
  - `third_party/libadlmidi/LICENSE.LGPL-2.1.txt`
- libADLMIDI FM bank data:
  - `third_party/libadlmidi/fm_banks/LICENSE-AIL2.txt`
  - `third_party/libadlmidi/fm_banks/LICENSE-DMXOPL.txt`
  - `third_party/libadlmidi/fm_banks/LICENSE-FMSynth.txt`
  - `third_party/libadlmidi/fm_banks/LICENSE-TheFatMan.txt`
  - `third_party/libadlmidi/fm_banks/ibk_files/license.txt`
  - `third_party/libadlmidi/fm_banks/ibk_files/ibk/IBK_License.txt`
- libADLMIDI embedded chip implementations and model data:
  - `third_party/libadlmidi/src/chips/LICENSE.txt`
  - `third_party/libadlmidi/src/chips/esfmu/LICENSE`
  - `third_party/libadlmidi/src/chips/nuked/LICENSE`
  - `third_party/libadlmidi/src/chips/nuked_cqm/LICENSE`
  - `third_party/libadlmidi/src/chips/nuked_fast/LICENSE`
  - `third_party/libadlmidi/src/chips/opal/LICENSE.txt`
  - `third_party/libadlmidi/src/chips/opal/old/LICENSE.txt`
  - `third_party/libadlmidi/src/chips/vpc_opl3/LICENSE`
  - `third_party/libadlmidi/src/chips/ymf262_lle/LICENSE`
  - `third_party/libadlmidi/src/midiseq/LICENSE.txt`
  - `third_party/libadlmidi/src/models/LICENSE.txt`

## Follow-up still needed

- `third_party/ags/agsplugin.h` is vendored as a standalone header. No adjacent
  license file is currently present in this repository snapshot.
- `third_party/miniaudio/miniaudio.h` is vendored as a standalone header. No
  adjacent license file is currently present in this repository snapshot.
- The build and packaging scripts refer to Munt / mt32emu sources under
  `third_party/munt`, so the vendored source layout and corresponding license
  text should be verified before a public release.
