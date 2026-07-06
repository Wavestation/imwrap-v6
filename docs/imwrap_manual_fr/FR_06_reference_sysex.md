# Chapitre 6 : Référence SysEx Détaillée

Si vous lisez ce chapitre, c'est que vous voulez forger vos propres messages SysEx (System Exclusive) dans votre séquenceur MIDI avec une précision chirurgicale. Préparez-vous à jongler avec l'hexadécimal et les *nibbles*.

Rappel général de la syntaxe :
```text
F0 7D [CODE] [PARAMÈTRES_DÉCODÉS] F7
```

---

## 6.1. Le concept du "Nibble" (Demi-octet)

Pour contourner la limitation du standard MIDI (qui interdit les valeurs supérieures à 127, soit `7F`, dans les données SysEx), iMWrap encode souvent les gros paramètres en **Nibbles**.
Un Nibble est un demi-octet (4 bits).

Pour envoyer la valeur hexadécimale `5A` (90 en décimal), on l'encode en deux nibbles : `05 0A`.
Pour la valeur `000A` (un nombre sur 16 bits), on l'encode en quatre nibbles : `00 00 00 0A`.

---

## 6.2. Décorticage du `0x00` Allocate Part

C'est le SysEx le plus complexe et le plus important. Il alloue et configure un canal (Part).

Format brut :
```text
F0 7D 00 [PART] [16_NIBBLES_DE_PARAMS] F7
```
Exemple :
`F0 7D 00 02 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 00 F7`

Explication pas-à-pas de l'exemple :
- `F0 7D` : En-tête iMWrap
- `00` : Commande "Allocate Part"
- `02` : Canal visé (Part 2)
- Ensuite viennent 8 octets de paramètres encodés en 16 nibbles (paires de chiffres) :
  1. `00 01` -> `01` : **Flags**. Bit 0 = 1 (Part allumée), Bit 1 = Reverb.
  2. `05 0A` -> `5A` : **Priority**. La priorité de la piste face à une limitation matérielle (90).
  3. `07 0F` -> `7F` : **Volume**. Volume max (127).
  4. `00 00` -> `00` : **Pan**. Panoramique (0 = centré). *Attention : c'est une valeur signée autour de 0, pas de 64.*
  5. `00 00` -> `00` : **Trans/Perc**. Bit 7 = Canal Percussion (1 si c'est la batterie). Bits 0-6 = Transposition.
  6. `00 00` -> `00` : **Detune**. Désaccordage fin.
  7. `00 02` -> `02` : **PitchBendFactor**. Amplitude du pitch bend (2 demi-tons).
  8. `00 00` -> `00` : **Program**. Numéro d'instrument MIDI par défaut (0).

---

## 6.3. Le Contrôle du Flux et les Boucles

### `0x40` Marker (Le Callback vers AGS)
```text
F0 7D 40 00 [MARKER_ID] F7
```
L'ID doit être un octet utile, par exemple `01`. Le `00` avant le marker est un identifiant de canal ignoré/inconnu historique.
*Exemple : `F0 7D 40 00 0C F7` envoie l'ID 12 à AGS.*

### `0x50` Set Loop (Créer une boucle)
C'est un long message nécessitant l'encodage de valeurs 16 bits (en 4 nibbles) pour cibler les mesures (Beats) et les Ticks.
```text
F0 7D 50 [UNKNOWN] [COUNT] [TO_BEAT] [TO_TICK] [FROM_BEAT] [FROM_TICK] F7
```
Exemple : 
`F0 7D 50 00 00 00 00 02 00 00 00 04 00 00 00 00 00 00 00 0A 00 00 00 00 F7`
- `[UNKNOWN]` : `00`
- `[COUNT]` (4 nibbles) : `00 00 00 02` -> Boucle 2 fois. *(0 désactive la boucle !)*
- `[TO_BEAT]` (4 nibbles) : `00 00 00 04` -> Vers la mesure 4.
- `[TO_TICK]` (4 nibbles) : `00 00 00 00` -> Vers le tick 0.
- `[FROM_BEAT]` (4 nibbles) : `00 00 00 0A` -> Depuis la mesure 10 (0A en hexa = 10).
- `[FROM_TICK]` (4 nibbles) : `00 00 00 00` -> Depuis le tick 0.

---

## 6.4. La Famille des Hooks (0x30 à 0x35)

Rappel : un Hook placé par le compositeur avec `CMD = 00` n'est qu'un "point d'attente". Il donne l'autorisation d'exécuter un Hook qui aurait été armé par le programmeur AGS. 
*(Si `CMD != 00`, c'est un hook inconditionnel, mais leur utilisation en authoring moderne est plus rare car supplantée par le scripting AGS).*

Format général :
```text
F0 7D [TYPE_HOOK] [PART] [CMD] [VALEURS...] F7
```
Exemple de Hook de Changement d'instrument (`0x34` Set Program) sur la piste 1 :
```text
F0 7D 34 01 00 00 F7
```

---

## 6.5. Cas Particuliers : L'Héritage Rétro

iMWrap n'oublie pas d'où il vient et supporte les instructions natives des cartes son de 1990.

### Les Messages AdLib (OPL3)
La synthèse FM AdLib (utilisée avant le General MIDI) a besoin de recevoir les paramètres directs du synthétiseur (Attack, Decay, Sustain, Release, etc.).
- `0x10` : Applique un patch AdLib (30 octets encodés en nibbles) à un canal spécifique.
- `0x11` : Diffuse un patch AdLib global (les 30 octets canoniques d'instrument).

### Les Messages Roland MT-32 natifs
Le MT-32 de Roland était capricieux et nécessitait des transferts mémoire via SysEx propres à Roland, et non `7D` (iMUSE).
Si vous composez pour MT-32, placez vos `SysEx Dump` Roland normaux au tout début de votre piste de contrôle, **avant** les `Allocate Part`.
iMWrap intercepte ces dumps mémoire spécifiques et sait les re-router et les recharger quand c'est nécessaire.

---

Maintenant que le cœur du système est à nu, passez au **Chapitre 7** pour apprendre comment empaqueter vos merveilleux fichiers MIDI dans une banque logicielle `.ims` distribuable avec votre jeu.
