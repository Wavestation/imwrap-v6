# imwrappack

`imwrappack` builds an `.ims` bank from a set of type-2 MIDI files.

## v1 Syntax

```text
imwrappack build output.ims \
  [--name=80:Woodtick] \
  [--mdhd=80:gmd:90:127:0:0:0:128] \
  80:gmd=80_woodtick.mid \
  80:rol=80_woodtick_mt32.mid \
  81:gmd=81_interior.mid
```

## Variant Entries

Format:

```text
soundId:variant=path.mid
```

Supported variants:

- `gmd`
- `rol`

## Debug Name

```text
--name=80:Woodtick
```

Adds a `NAME` chunk to the target `SOUN`.

## `MDhd` Parameters

```text
--mdhd=soundId:variant:priority:volume:pan:transpose:detune:speed
```

Example:

```text
--mdhd=80:gmd:90:127:0:0:0:128
```

If no `--mdhd` option is provided for a variant, the `MDhd` chunk is not written.
