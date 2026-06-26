# Format des messages SysEx iMWrap pour les séquences MIDI

Pour piloter les fonctionnalités avancées d'iMWrap (boucles, marqueurs, allocations de pistes interactives) directement depuis vos fichiers MIDI (`.mid` / `.smf`), vous devez y insérer des événements **System Exclusive (SysEx)** spécialement formatés.

Ce document détaille la structure binaire attendue par le moteur iMWrap.

---

## 1. Structure générale de l'enveloppe SysEx

Tous les messages SysEx iMWrap suivent une structure stricte. En hexadécimal, la forme globale est la suivante :

```text
F0 7D [CODE] [PAYLOAD...] F7
```

- `F0` : Début du message SysEx (Standard MIDI).
- `7D` : Identifiant constructeur (ID non-commercial souvent utilisé par les jeux éducatifs ou prototypes, réutilisé par iMUSE/iMWrap).
- `[CODE]` : Un octet identifiant l'action à réaliser (ex: `0x40` pour un marqueur, `0x50` pour définir une boucle).
- `[PAYLOAD...]` : Les arguments de la commande (certaines données sont encodées en "nibbles", voir section suivante).
- `F7` : Fin du message SysEx (Standard MIDI).

### L'encodage "Nibbles" (Demi-octets)
Pour éviter qu'un octet de donnée MIDI ne dépasse `0x7F` (ce qui casserait le protocole MIDI), iMWrap encode la plupart des valeurs binaires de charge utile (payload) sous forme de **nibbles**. 
Un octet original (ex: `0xAB`) est scindé en deux octets envoyés successivement : `0x0A` puis `0x0B`.

---

## 2. Liste des Commandes (Codes SysEx)

Voici la syntaxe précise à utiliser dans votre éditeur MIDI pour les commandes les plus courantes.

### `0x02` : Start Song (Démarrer le morceau)
Déclare le véritable début du morceau interactif.
```text
F0 7D 02 F7
```

### `0x40` : Text Marker (Marqueur de synchronisation)
Place un marqueur temporel permettant de déclencher du code dans le jeu (via `MarkerCallback`).
- **Format** : `F0 7D 40 [ID] [TEXTE ASCII] F7`
- `[ID]` : Identifiant (généralement `0x00`).
- `[TEXTE ASCII]` : Le texte du marqueur, encodé en ASCII.
- **Exemple** pour le marqueur "INTRO" :
```text
F0 7D 40 00 49 4E 54 52 4F F7
```

### `0x50` : Set Loop (Définir une boucle temporelle)
Ordonne au séquenceur de boucler d'un point B (To) vers un point A (From), un certain nombre de fois.
- **Format** : `F0 7D 50 [ID] [10 octets originaux encodés en 20 nibbles] F7`
- `[ID]` : Généralement `0x00`.
- **Payload décodé (10 octets en Big-Endian)** :
  1. `LoopCount` (2 octets) : Nombre de boucles (0 = infini).
  2. `ToBeat` (2 octets) : Beat de fin de boucle.
  3. `ToTick` (2 octets) : Tick de fin de boucle.
  4. `FromBeat` (2 octets) : Beat de retour de boucle.
  5. `FromTick` (2 octets) : Tick de retour de boucle.
- **Exemple** : Boucle infinie (`0x0000`) depuis le beat 10 (`0x000A`), tick 0 (`0x0000`) vers le beat 4 (`0x0004`), tick 0 (`0x0000`).
  - Données d'origine : `00 00` | `00 0A` | `00 00` | `00 04` | `00 00`
  - Encodage Nibbles : `00 00 00 00` | `00 00 00 0A` | `00 00 00 00` | `00 00 00 04` | `00 00 00 00`
```text
F0 7D 50 00 00 00 00 00 00 00 00 0A 00 00 00 00 00 00 00 04 00 00 00 00 F7
```

### `0x51` : Clear Loop (Annuler la boucle)
Annule la boucle actuellement configurée. La musique continuera de manière linéaire.
```text
F0 7D 51 F7
```

### `0x00` : Allocate Part (Allouer et configurer une piste interactive)
Déclare une piste, sa priorité, son panoramique, son instrument, etc. C'est l'équivalent d'un super *Program Change* combiné à d'autres contrôles.
- **Format** : `F0 7D 00 [PART_ID] [Max 8 octets originaux encodés en 16 nibbles] F7`
- `[PART_ID]` : ID de la partie (0 à 15, soit le canal MIDI ciblé).
- **Payload décodé (jusqu'à 8 octets)** :
  1. `Flags` : bit 0 = Part On, bit 1 = Reverb.
  2. `Priority` : Priorité de la piste (ex: 90).
  3. `Volume` : 0 à 127.
  4. `Pan` : Panoramique.
  5. `Trans/Perc` : Bit 7 = Percussion, Bits 0-6 = Transpose.
  6. `Detune` : Désaccordage.
  7. `PitchBendFactor` : Facteur de PitchBend (défaut = 2).
  8. `Program` : Numéro de l'instrument.

### `0x01` : Shutdown Part (Éteindre une piste)
Coupe définitivement une piste allouée.
- **Format** : `F0 7D 01 [PART_ID] F7`

### `0x60` : Set Instrument (Changer l'instrument interne)
Permet de mapper un canal vers un identifiant d'instrument iMUSE complet.
- **Format** : `F0 7D 60 [CHAN] [4 Nibbles pour uint16_t Instrument] F7`
- **Exemple** : Canal 2 (`0x02`), Instrument 124 (`0x007C` en nibbles `00 00 07 0C`) :
```text
F0 7D 60 02 00 00 07 0C F7
```

### Les Commandes "Hook" (`0x30` à `0x35`)
Ces SysEx servent à déclencher des événements automatiques.
- `0x30` : Hook Jump (Saut conditionnel).
- `0x31` / `0x35` : Hook Global Transpose / Hook Set Transpose.
- `0x32` / `0x33` / `0x34` : Hook Part On/Off, Hook Volume, Hook Program.
Elles partagent toutes un même principe : le premier nibble contient l'ID de la sous-commande Hook, suivi des paramètres de l'action, le tout encodé en nibbles.

---

## 3. Conseil d'intégration

Si vous utilisez un séquenceur externe (comme Cubase, REAPER ou FL Studio), ajoutez un **événement SysEx** sur la piste principale (Track 0) au tout début du morceau (Tick 0) pour initialiser vos boucles ou marqueurs, en veillant à toujours inclure l'enveloppe complète avec le header `F0 7D` et le footer `F7`.
