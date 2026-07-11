# imwrappack

`imwrappack` is the maintained command-line packer for `.ims` banks.

It now covers both packaging and post-packaging bank editing:

- inspect an existing bank
- create an empty bank
- build a bank from MIDI sources
- add, rename, or delete sounds
- add or delete variants
- import MIDI into an existing sound/variant
- set or clear `MDhd`
- delete, reorder, and export tracks

The original v1 implementation has been moved to `legacy-tools/cli/imwrappack_v1.cpp`.

## Supported Variants

- `gmd`
- `rol`
- `adl`

## Build From MIDI

```text
imwrappack build output.ims \
  --name=80:Forest \
  --mdhd=80:gmd:90:127:0:0:0:128 \
  80:gmd=80_forest_gm.mid \
  80:rol=80_forest_mt32.mid \
  80:adl=80_forest_adlib.mid
```

Variant entries still use:

```text
soundId:variant=path.mid
```

MIDI import follows the same rules as the GUI packer:

- SMF 0: imported as one track
- SMF 1: merged into one format-0 style track
- SMF 2: each SMF track is imported as its own IMS track

## Edit Existing Banks

Common commands:

```text
imwrappack inspect bank.ims
imwrappack add-sound bank.ims 80 gmd --name=Forest
imwrappack import-midi bank.ims 80 gmd forest.mid
imwrappack set-mdhd bank.ims 80 gmd 90 127 0 0 0 128
imwrappack move-track bank.ims 80 gmd 1 up
imwrappack export-track bank.ims 80 gmd 0 forest_track0.mid
```

## Sound Names

```text
--name=80:Woodtick
```

This assigns the debug `NAME` chunk of a sound.

## `MDhd`

```text
--mdhd=soundId:variant:priority:volume:pan:transpose:detune:speed
```

If `MDhd` is not set for a variant, the bank stores the default runtime values.
