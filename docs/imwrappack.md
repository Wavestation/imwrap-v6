# imwrappack

`imwrappack` construit une banque `.ims` à partir d'un ensemble de MIDIs type 2.

## Syntaxe v1

```text
imwrappack build output.ims \
  [--name=80:Woodtick] \
  [--mdhd=80:gmd:90:127:0:0:0:128] \
  80:gmd=80_woodtick.mid \
  80:rol=80_woodtick_mt32.mid \
  81:gmd=81_interior.mid
```

## Entrées de variante

Format :

```text
soundId:variant=path.mid
```

Variantes supportées :

- `gmd`
- `rol`

## Nom de debug

```text
--name=80:Woodtick
```

Ajoute un chunk `NAME` au `SOUN` visé.

## Paramètres `MDhd`

```text
--mdhd=soundId:variant:priority:volume:pan:transpose:detune:speed
```

Exemple :

```text
--mdhd=80:gmd:90:127:0:0:0:128
```

Si aucune option `--mdhd` n'est fournie pour une variante, le chunk `MDhd` n'est pas écrit.
