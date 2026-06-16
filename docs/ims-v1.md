# IMS v1

Le fichier `.ims` est une banque iMUSE pour AGS. Le format est chunked, big-endian, et proche de l'esprit des ressources `SOUN` SCUMM.

## Regles globales

- Chaque chunk commence par `FourCC(4)` puis `u32be size`.
- `size` inclut les 8 octets d'en-tete.
- Le fichier complet est un chunk racine `IMSB`.
- Les morceaux transportes dans les variantes `GMD ` ou `ROL ` sont des SMF type 2 contenant les SysEx iMUSE d'origine.

## Structure canonique

```text
IMSB
  IHDR
  SDIR
  SOUN
  SOUN
  ...
```

## `IHDR`

Corps de 16 octets :

```text
u16be version_major
u16be version_minor
u32be sound_count
u32be flags
u32be reserved
```

Pour la v1 :

- `version_major = 1`
- `version_minor = 0`
- `flags = 0`
- `reserved = 0`

## `SDIR`

Table d'index compacte. Une entree = 12 octets :

```text
u16be sound_id
u16be variant_mask
u32be sound_offset
u32be sound_size
```

Masque de variantes :

- `0x0001` = `GMD `
- `0x0002` = `ROL `

## `SOUN`

```text
SOUN
  SIDN
  NAME   ; optionnel
  GMD    ; optionnel, mais au moins une variante requise
  ROL    ; optionnel
```

## `SIDN`

Corps de 4 octets :

```text
u16be sound_id
u16be reserved
```

## `NAME`

Texte UTF-8 brut, sans terminateur obligatoire. Purement informatif.

## `GMD ` / `ROL `

Le corps d'une variante peut contenir :

- un chunk `MDhd` optionnel ;
- puis un flux MIDI complet commençant par `MThd`.

Le loader v1 preserve le flux MIDI byte-for-byte a partir de `MThd`.

## `MDhd`

Corps de 8 octets :

```text
u8  reserved0
u8  reserved1
u8  priority
u8  volume
s8  pan
s8  transpose
s8  detune
u8  speed
```

Valeurs par defaut quand `MDhd` est absent :

- `priority = 128`
- `volume = 127`
- `pan = 0`
- `transpose = 0`
- `detune = 0`
- `speed = 128`
