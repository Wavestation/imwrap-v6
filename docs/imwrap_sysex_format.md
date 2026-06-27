# Format des messages SysEx iMWrap

Ce document est une référence courte et fiable pour les SysEx reconnus par `imwrap-v6`.

Pour la version détaillée, voir [imwrap_sysex_reference_detaillee_fr.md](./imwrap_sysex_reference_detaillee_fr.md).

## 1. Forme générale

Tous les SysEx iMWrap suivent cette forme :

```text
F0 7D [CODE] [PAYLOAD...] F7
```

- `F0` : début du SysEx
- `7D` : identifiant iMWrap/iMUSE
- `CODE` : code de commande
- `PAYLOAD` : paramètres
- `F7` : fin du SysEx

### Encodage en nibbles

Beaucoup de données sont encodées en nibbles :

```text
5A -> 05 0A
AB -> 0A 0B
000A -> 00 00 00 0A
```

## 2. Messages principaux

### `0x00` Allocate Part

Format canonique :

```text
F0 7D 00 [PART] [8 octets encodés en 16 nibbles] F7
```

Payload décodé :

1. `Flags` : bit 0 = part on, bit 1 = reverb
2. `Priority`
3. `Volume`
4. `Pan`
5. `Trans/Perc` : bit 7 = percussion, bits 0-6 = transpose
6. `Detune`
7. `PitchBendFactor`
8. `Program`

Effet : alloue et configure une part logique.

Note importante : dans le moteur actuel, `pan` est interprété comme une valeur signée centrée sur `0`, pas comme `0..127` centré sur `64`.

### `0x01` Shutdown Part

```text
F0 7D 01 [PART] F7
```

Effet : coupe la part et remet son état à zéro.

### `0x02` Start Song

```text
F0 7D 02 F7
```

Effet : réinitialise toutes les parts du son courant.

Usage recommandé : au tout début du morceau, au tick `0`.

### `0x21` Parameter Adjust

```text
F0 7D 21 [PART] [UNKNOWN] [PARAM16] [VALUE16] F7
```

Effet actuel dans `imwrap-v6` : aucun.

Le message est parsé, mais ignoré par le moteur runtime.

### `0x30` Hook Jump

```text
F0 7D 30 [UNKNOWN] [CMD] [TRACK16] [BEAT16] [TICK16] F7
```

Effet : saut conditionnel intelligent.

### `0x31` Hook Global Transpose

```text
F0 7D 31 [UNKNOWN] [CMD] [RELATIVE] [SIGNED_VALUE] F7
```

Effet : transpose le son entier.

### `0x32` Hook Part On/Off

```text
F0 7D 32 [CHAN] [CMD] [VALUE] F7
```

Effet :

- `VALUE = 00` : éteint la part
- `VALUE != 00` : allume la part

### `0x33` Hook Set Volume

```text
F0 7D 33 [CHAN] [CMD] [VALUE] F7
```

Effet : change le volume d'une part.

### `0x34` Hook Set Program

```text
F0 7D 34 [CHAN] [CMD] [VALUE] F7
```

Effet : change l'instrument d'une part.

### `0x35` Hook Set Transpose

```text
F0 7D 35 [CHAN] [CMD] [RELATIVE] [SIGNED_VALUE] F7
```

Effet : change la transposition d'une part.

### `0x40` Marker

```text
F0 7D 40 [UNKNOWN] [MARKER_BYTES...] F7
```

Effet réel du moteur : un trigger par octet utile.

Exemple robuste :

```text
F0 7D 40 00 01 F7
```

Si vous envoyez un texte comme `INTRO`, le moteur déclenche cinq marqueurs successifs, un par caractère.

### `0x50` Set Loop

```text
F0 7D 50 [UNKNOWN] [COUNT16] [TO_BEAT16] [TO_TICK16] [FROM_BEAT16] [FROM_TICK16] F7
```

Effet réel :

- quand la lecture atteint `FROM_BEAT:FROM_TICK`
- elle saute vers `TO_BEAT:TO_TICK`

Important :

- dans le code actuel, `COUNT = 0` ne veut pas dire "infini"
- `COUNT = 0` désactive la boucle
- il faut `FROM` strictement plus loin que `TO`

Exemple :

```text
F0 7D 50 00 00 00 00 02 00 00 00 04 00 00 00 00 00 00 00 0A 00 00 00 00 F7
```

Ici :

- boucle `2` fois
- quand on atteint `beat 10 tick 0`
- on saute à `beat 4 tick 0`

### `0x51` Clear Loop

```text
F0 7D 51 F7
```

Effet : annule la boucle courante.

### `0x60` Set Instrument

```text
F0 7D 60 [CHAN] [N1] [N2] [N3] [N4] F7
```

Les 4 nibbles forment un `uint16`, interprété comme :

- octet haut = `bank`
- octet bas = `program`

Exemple :

```text
F0 7D 60 02 00 00 07 0C F7
```

correspond à :

- `bank = 0x00`
- `program = 0x7C`

## 3. Messages AdLib

### `0x10` AdLib Part Instrument

```text
F0 7D 10 [PART] [UNKNOWN] [DONNÉES_ADLIB en nibbles] F7
```

### `0x11` AdLib Global Instrument

```text
F0 7D 11 [UNKNOWN] [VALUE] [PROGRAM] [DONNÉES_ADLIB en nibbles] F7
```

En authoring, il faut considérer qu'un instrument AdLib canonique fait `30` octets.

Voir l'annexe AdLib dans la référence détaillée pour le layout complet.

## 4. Messages Roland MT-32

Le moteur sait aussi transporter des SysEx Roland MT-32 natifs, distincts des SysEx `7D`.

Ces SysEx ne font pas partie de la famille `0x00..0x60`, mais ils sont utilisés pour :

- les timbres personnalisés
- l'initialisation MT-32
- la mise à jour de patch memory

Voir l'annexe MT-32 dans la référence détaillée pour la structure exacte.

## 5. Ce qui est vraiment obligatoire

Pour produire du son, aucun SysEx iMWrap n'est strictement obligatoire.

Pour un morceau iMUSE proprement authoré, recommandé :

1. `0x02` au tick `0`
2. un `0x00` pour chaque part logique utilisée

Ensuite :

- `0x40` si vous avez besoin de triggers
- `0x50` / `0x51` si vous avez besoin de boucles
- `0x30..0x35` si vous utilisez les hooks
- `0x10` / `0x11` si vous authoriez pour AdLib

## 6. Début minimal de morceau

```text
F0 7D 02 F7
F0 7D 00 00 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 00 F7
F0 7D 00 01 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 01 F7
```

## 7. Écarts importants avec certaines docs simplifiées

- `0x21 Parameter Adjust` : reconnu, mais sans effet runtime actuel
- `0x40 Marker` : un octet utile = un trigger
- `0x50 Set Loop` : `count = 0` ne fait pas de boucle infinie dans ce code
- `0x00 Allocate Part` : `pan` est traité comme une valeur signée centrée sur `0`
