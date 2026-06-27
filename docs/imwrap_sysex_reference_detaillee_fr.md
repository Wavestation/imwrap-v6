# Référence détaillée des SysEx iMUSE / iMWrap

Ce document sert de référence technique détaillée pour les messages SysEx reconnus par `imwrap-v6`.

Il est fondé en priorité sur le comportement réel du code source, en particulier :

- `src/IMWrapSysex.cpp` pour le parsing et l'encodage
- `src/IMWrapEngine.cpp` pour les effets runtime
- `src/Instrument.cpp` et `src/AgsIMWrapPlugin.cpp` pour les cas particuliers MT-32 et AdLib

Quand la documentation historique et le code divergent, ce document privilégie le code.

## 1. Portée

Ce document couvre :

- le format général des SysEx iMWrap/iMUSE
- la syntaxe détaillée de chaque message reconnu
- l'effet réel de chaque message dans le moteur
- le statut pratique de chaque message : obligatoire, recommandé ou facultatif
- les particularités iMWrap liées aux formats MT-32 et AdLib

Il ne couvre pas en détail l'ensemble des événements MIDI standard (`Note On`, `CC`, `Program Change`, Meta tempo, etc.), sauf quand ils interagissent avec les SysEx.

## 2. Structure générale

La forme canonique d'un SysEx iMWrap est :

```text
F0 7D [CODE] [PAYLOAD...] F7
```

- `F0` : début SysEx MIDI standard
- `7D` : identifiant fabricant non commercial utilisé ici par iMWrap/iMUSE
- `CODE` : identifiant de commande
- `PAYLOAD` : arguments de la commande
- `F7` : fin SysEx MIDI standard

### 2.1. Tolérance du parseur

Le parseur interne retire `F0` en tête et `F7` en fin s'ils sont présents.

En pratique, il accepte donc :

- un message complet `F0 ... F7`
- ou directement le corps `7D [CODE] [PAYLOAD...]`

Cette tolérance est un détail d'implémentation. Pour l'authoring MIDI, il faut toujours écrire la forme complète avec `F0` et `F7`.

### 2.2. Encodage en nibbles

Beaucoup de paramètres sont encodés en nibbles pour rester compatibles avec le transport MIDI 7 bits.

Exemple :

```text
octet 5A -> 05 0A
octet AB -> 0A 0B
uint16 000A -> 00 00 00 0A
```

Le parseur reconstruit chaque octet à partir des 4 bits faibles de deux octets successifs.

## 3. Conventions générales

### 3.1. Indices de part et de canal

- `part` : part logique iMUSE/iMWrap, `0..15`
- `chan` : même idée côté part/canal logique, `0..15`

Dans ce moteur, les événements de séquence MIDI sont dispatchés par indice de part logique, pas par "canal GM" au sens strict.

### 3.2. Endianness

Les entiers 16 bits décodés depuis des nibbles sont lus en big-endian.

Exemple :

```text
00 0A -> 10
01 2C -> 300
```

### 3.3. Valeurs signées

Plusieurs champs sont signés :

- `pan` dans `0x00`
- `detune`
- `signedValue` dans `0x31` et `0x35`

Le code ne les traite pas toujours comme la doc "séquenceur" habituelle pourrait le laisser penser. Les détails sont précisés message par message.

## 4. Vue d'ensemble des messages reconnus

| Code | Nom | Effet runtime | Statut pratique |
| --- | --- | --- | --- |
| `0x00` | Allocate Part | Alloue/configure une part | Quasi obligatoire pour un vrai morceau iMUSE |
| `0x01` | Shutdown Part | Coupe et réinitialise une part | Facultatif |
| `0x02` | Start Song | Réinitialise toutes les parts du son | Recommandé au tick 0 |
| `0x10` | AdLib Part Instrument | Charge un instrument AdLib sur une part | Facultatif |
| `0x11` | AdLib Global Instrument | Diffuse un instrument AdLib à toutes les parts actives | Facultatif |
| `0x21` | Parameter Adjust | Aucun effet runtime actuel | Facultatif, sans effet actuel |
| `0x30` | Hook Jump | Saut conditionnel intelligent | Facultatif |
| `0x31` | Hook Global Transpose | Transpose le son entier | Facultatif |
| `0x32` | Hook Part On/Off | Allume/éteint une part | Facultatif |
| `0x33` | Hook Set Volume | Change le volume d'une part | Facultatif |
| `0x34` | Hook Set Program | Change l'instrument d'une part | Facultatif |
| `0x35` | Hook Set Transpose | Change la transposition d'une part | Facultatif |
| `0x40` | Marker | Déclenche des marqueurs/queues de commandes | Facultatif |
| `0x50` | Set Loop | Arme une boucle temporelle | Facultatif |
| `0x51` | Clear Loop | Annule la boucle courante | Facultatif |
| `0x60` | Set Instrument | Affecte un identifiant d'instrument `bank<<8 | program` | Facultatif |

## 5. Référence détaillée message par message

### 5.1. `0x00` Allocate Part

#### Syntaxe canonique

```text
F0 7D 00 [PART] [8 octets encodés en 16 nibbles] F7
```

Le parseur accepte aussi une forme plus courte, mais la forme canonique utile est celle à 8 octets.

#### Structure décodée

| Octet | Nom | Interprétation |
| --- | --- | --- |
| `b0` | flags | bit 0 = `partOn`, bit 1 = `reverb` |
| `b1` | priority | priorité brute |
| `b2` | volume | volume part |
| `b3` | pan | signé 8 bits |
| `b4` | trans/perc | bit 7 = percussion, bits 0-6 = transpose signé sur 7 bits |
| `b5` | detune | signé 8 bits |
| `b6` | pitchbendFactor | portée de pitch bend |
| `b7` | program | programme de base |

#### Exemple

```text
F0 7D 00 03 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 00 F7
```

Cet exemple configure la part `03` avec :

- `partOn = 1`
- `reverb = 0`
- priorité brute `0x5A`
- volume `0x7F`
- pan `0`
- transpose `0`
- detune `0`
- pitch bend factor `2`
- programme `0`

#### Effet runtime

Le moteur :

- crée ou récupère la part logique
- la marque présente
- applique on/off, reverb, volume, transpose, pan, priorité, percussion, detune, pitch bend factor
- appelle ensuite `partSetProgram()`
- alloue un canal physique si nécessaire

#### Pièges

- Le `pan` est lu comme un `int8_t`, puis clampé sur `[-64, +63]`.
  `00` est donc le centre logique dans le moteur.
  La convention "0..127 centré sur 64" n'est pas ce que fait réellement ce code.
- Le champ priorité n'est pas utilisé tel quel : le moteur convertit `priority` en `priority - 0x40` pour obtenir une priorité interne signée.
  En pratique, `0x40` agit comme un point neutre.
- Le nibble haut de `b0` peut être émis par l'encodeur, mais le runtime ne lui donne pas de sémantique dédiée.

#### Statut pratique

Pour un morceau iMUSE réellement contrôlé par parts, `0x00` est le message le plus important.

Il n'est pas "strictement" obligatoire pour produire du son, car les parts peuvent s'auto-initialiser sur les premiers `Note On`, mais il est pratiquement indispensable pour un authoring sérieux.

### 5.2. `0x01` Shutdown Part

#### Syntaxe

```text
F0 7D 01 [PART] F7
```

#### Exemple

```text
F0 7D 01 03 F7
```

#### Effet runtime

Le moteur :

- coupe la part
- arrête ses notes
- libère son canal physique
- remet son état à zéro

#### Statut pratique

Facultatif. Utile pour des transitions ou des extinctions explicites.

### 5.3. `0x02` Start Song

#### Syntaxe

```text
F0 7D 02 F7
```

#### Effet runtime

Réinitialise toutes les parts du son courant :

- notes coupées
- canaux libérés
- états de parts remis à zéro

#### Pièges

- Si vous placez ce message au milieu d'un morceau, il efface l'état de toutes les parts.
- Le parseur tolère des octets supplémentaires, mais ils n'ont pas de sémantique utile.

#### Statut pratique

Recommandé au tick `0` pour rendre le début de séquence explicite et robuste.

### 5.4. `0x10` AdLib Part Instrument

#### Syntaxe

```text
F0 7D 10 [PART] [UNKNOWN] [données AdLib en nibbles...] F7
```

#### Effet runtime

- si les données décodées font au moins `11` octets, le moteur considère qu'il s'agit d'un instrument AdLib et l'applique à la part
- si les données sont plus courtes mais non vides, le moteur peut faire un fallback limité en `Program Change`

#### Recommandation d'authoring

Même si ce chemin runtime accepte `>= 11` octets, il faut considérer qu'un instrument AdLib iMUSE canonique fait `30` octets.

Voir l'annexe AdLib pour la structure détaillée.

#### Statut pratique

Facultatif. Réservé aux morceaux/variants AdLib.

### 5.5. `0x11` AdLib Global Instrument

#### Syntaxe

```text
F0 7D 11 [UNKNOWN] [VALUE] [PROGRAM] [données AdLib en nibbles...] F7
```

#### Effet runtime

Si les données décodées font au moins `11` octets, le moteur diffuse cet instrument AdLib à toutes les parts actives du son.

#### Notes

- `UNKNOWN`, `VALUE` et `PROGRAM` sont bien parsés
- leur rôle concret dans le runtime actuel est secondaire par rapport au payload AdLib lui-même

#### Statut pratique

Facultatif.

### 5.6. `0x21` Parameter Adjust

#### Syntaxe

```text
F0 7D 21 [PART] [UNKNOWN] [PARAM16 en 4 nibbles] [VALUE16 en 4 nibbles] F7
```

#### Exemple

```text
F0 7D 21 03 00 00 00 01 05 00 00 06 04 F7
```

#### Effet runtime

Le message est correctement parsé, mais le moteur retourne immédiatement sans action.

#### Conclusion

Dans l'état actuel de `imwrap-v6`, ce message est sans effet runtime.

#### Statut pratique

Facultatif, non recommandé si vous cherchez un effet réel aujourd'hui.

### 5.7. `0x30` Hook Jump

#### Syntaxe

```text
F0 7D 30 [UNKNOWN] [CMD] [TRACK16] [BEAT16] [TICK16] F7
```

Les 7 octets `[CMD][TRACK16][BEAT16][TICK16]` sont encodés en nibbles.

#### Exemple

```text
F0 7D 30 00 00 07 00 00 00 00 00 00 00 0A 00 00 00 00 F7
```

#### Effet runtime

Déclenche un saut conditionnel intelligent :

- vérifie la condition de hook
- programme les note-offs restants si nécessaire
- effectue le saut
- relâche les pédales de sustain si besoin

#### Statut pratique

Facultatif. À utiliser si le jeu arme les hooks côté runtime.

### 5.8. `0x31` Hook Global Transpose

#### Syntaxe

```text
F0 7D 31 [UNKNOWN] [CMD] [RELATIVE] [SIGNED_VALUE] F7
```

Les 3 octets `[CMD][RELATIVE][SIGNED_VALUE]` sont encodés en nibbles.

#### Effet runtime

Transpose le son entier.

- `RELATIVE = 00` : valeur absolue
- `RELATIVE != 00` : transposition relative

#### Pièges

- En relatif, le moteur replie d'abord la valeur dans une fenêtre musicale autour de `[-7, +7]`, puis applique les clamps internes.
- Le moteur recalcule bien les transpositions effectives des parts, mais ce chemin ne pousse pas ici explicitement une mise à jour MIDI instantanée part par part.

#### Statut pratique

Facultatif.

### 5.9. `0x32` Hook Part On/Off

#### Syntaxe

```text
F0 7D 32 [CHAN] [CMD] [VALUE] F7
```

`[CMD][VALUE]` sont encodés en nibbles.

#### Effet runtime

- `VALUE = 00` : coupe la part
- `VALUE != 00` : allume la part

#### Statut pratique

Facultatif.

### 5.10. `0x33` Hook Set Volume

#### Syntaxe

```text
F0 7D 33 [CHAN] [CMD] [VALUE] F7
```

#### Effet runtime

Change le volume de la part ciblée.

La valeur utile est `0..127`.

#### Statut pratique

Facultatif.

### 5.11. `0x34` Hook Set Program

#### Syntaxe

```text
F0 7D 34 [CHAN] [CMD] [VALUE] F7
```

#### Effet runtime

Change le programme de la part ciblée.

En profil MT-32, cela peut entraîner :

- une mise à jour mémoire du patch MT-32
- des Program Changes supplémentaires de rafraîchissement

#### Statut pratique

Facultatif.

### 5.12. `0x35` Hook Set Transpose

#### Syntaxe

```text
F0 7D 35 [CHAN] [CMD] [RELATIVE] [SIGNED_VALUE] F7
```

#### Effet runtime

Transpose la part ciblée.

- `RELATIVE = 00` : valeur absolue
- `RELATIVE != 00` : valeur relative

#### Notes

En relatif, le moteur fait d'abord un repli de type musical sur `[-7, +7]`, puis un clamp final sur `[-24, +24]`.

#### Statut pratique

Facultatif.

### 5.13. `0x40` Marker

#### Syntaxe

```text
F0 7D 40 [UNKNOWN] [MARKER_BYTES...] F7
```

#### Exemple robuste

```text
F0 7D 40 00 01 F7
```

#### Effet runtime réel

Le moteur ne traite pas la chaîne entière comme un seul marqueur.

Il parcourt chaque octet de `MARKER_BYTES` et appelle `handleMarker()` une fois par octet.

#### Conséquence pratique

- Si vous envoyez `"INTRO"`, vous obtenez cinq triggers successifs : `I`, `N`, `T`, `R`, `O`.
- Si vous voulez un identifiant de trigger unique, utilisez un seul octet utile.

#### Statut pratique

Facultatif, mais très utile pour la synchronisation jeu/musique.

### 5.14. `0x50` Set Loop

#### Syntaxe

```text
F0 7D 50 [UNKNOWN] [COUNT16] [TO_BEAT16] [TO_TICK16] [FROM_BEAT16] [FROM_TICK16] F7
```

Les 10 octets sont encodés en 20 nibbles.

#### Sémantique runtime

Le moteur boucle :

- quand il atteint `FROM_BEAT:FROM_TICK`
- en sautant vers `TO_BEAT:TO_TICK`

Autrement dit :

- `FROM` = point de déclenchement de la boucle
- `TO` = point de retour

#### Exemple

```text
F0 7D 50 00 00 00 00 02 00 00 00 04 00 00 00 00 00 00 00 0A 00 00 00 00 F7
```

Exemple ci-dessus :

- `count = 2`
- quand on atteint `beat 10 tick 0`
- on saute à `beat 4 tick 0`

#### Pièges importants

- Contrairement à certaines docs historiques, `count = 0` ne signifie pas "boucle infinie" dans ce code.
  Avec `count = 0`, il n'y a pas de boucle active.
- Le moteur refuse les boucles si `toBeat + 1 >= fromBeat`.
  En pratique, il faut un `FROM` strictement plus loin que `TO`.

#### Statut pratique

Facultatif.

### 5.15. `0x51` Clear Loop

#### Syntaxe

```text
F0 7D 51 F7
```

#### Effet runtime

Met `loopCounter` à `0` et annule la boucle courante.

#### Notes

Le parseur tolère un payload supplémentaire, mais celui-ci n'a pas de rôle utile.

#### Statut pratique

Facultatif.

### 5.16. `0x60` Set Instrument

#### Syntaxe

```text
F0 7D 60 [CHAN] [N1] [N2] [N3] [N4] F7
```

Les 4 nibbles reconstituent un `uint16` :

```text
instrument = (N1 << 12) | (N2 << 8) | (N3 << 4) | N4
```

#### Interprétation runtime

Le moteur fait :

- `bank = high byte`
- `program = low byte`

Donc :

```text
0x007C -> bank 0x00, program 0x7C
0x1234 -> bank 0x12, program 0x34
```

#### Exemple

```text
F0 7D 60 02 00 00 07 0C F7
```

#### Statut pratique

Facultatif.

## 6. Hooks : règle générale d'usage

Tous les hooks `0x30..0x35` reposent sur le même principe :

- le jeu arme un état de hook via `PlayerSetHook` / `iMWrap_SetHook`
- la séquence contient ensuite un SysEx hook conditionnel
- si l'état courant correspond au `CMD`, l'action s'exécute

### 6.1. Valeurs de `CMD`

- `00` : inconditionnel
- `01..7F` : conditionnel et consommable
- `80..FF` : conditionnel non consommé

Dans l'état initial du moteur, les hooks sont reset à `FF`.

En pratique, un hook séquencé avec `CMD = FF` se comporte donc comme un hook persistant armé par défaut.

## 7. Quels messages sont obligatoires ?

### 7.1. Lecture MIDI simple

Aucun SysEx iMUSE n'est strictement obligatoire pour qu'un fichier MIDI produise du son via ce moteur.

Les parts peuvent s'auto-initialiser à la première activité MIDI.

### 7.2. Morceau iMUSE correctement authoré

En pratique :

- `0x02` au tick `0` : recommandé
- `0x00` pour chaque part logique utilisée : fortement recommandé

### 7.3. Messages optionnels selon le besoin

- `0x40` si vous avez besoin de triggers
- `0x50` / `0x51` si vous avez besoin de boucles
- `0x30..0x35` si vous avez besoin d'interactivité pilotée par hooks
- `0x10` / `0x11` si vous travaillez en AdLib
- `0x60` si vous utilisez l'identifiant instrument `bank/program`

### 7.4. Message reconnu mais sans effet actuel

- `0x21 Parameter Adjust`

## 8. Templates prêts à copier

### 8.1. Début minimal de morceau iMUSE

```text
F0 7D 02 F7
F0 7D 00 00 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 00 F7
F0 7D 00 01 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 01 F7
```

### 8.2. Marker simple à un octet

```text
F0 7D 40 00 01 F7
```

### 8.3. Boucle comptée

```text
F0 7D 50 00 00 00 00 03 00 00 00 01 00 00 00 00 00 00 00 08 00 00 00 00 F7
```

Exemple ci-dessus :

- boucle `3` fois
- déclenchement au `beat 8`
- retour au `beat 1`

Adaptez évidemment `TO` et `FROM` selon votre structure musicale.

## Annexe A. MT-32 : format et adaptations spécifiques iMWrap

Cette annexe couvre deux choses distinctes :

- les vrais SysEx Roland MT-32 transportés dans la séquence
- les adaptations internes iMWrap autour des timbres personnalisés

### A.1. Le chemin "vrai SysEx Roland"

Si un événement SysEx n'est pas reconnu comme SysEx iMWrap `7D`, le moteur le traite comme un SysEx externe.

S'il commence par `41`, il est considéré comme un SysEx Roland potentiel.

Le moteur essaie alors d'associer ce SysEx à une part logique en utilisant le bas nibble du deuxième octet du payload.

Exemple de forme générale :

```text
F0 41 [xx] 16 12 [addr1] [addr2] [addr3] [data...] [checksum] F7
```

### A.2. Convention iMWrap sur l'association à une part

Le code fait :

```text
logicalPart = payload[1] & 0x0F
```

Ceci signifie, par inférence depuis le code runtime, que le bas nibble du deuxième octet du SysEx Roland est utilisé comme identifiant de part logique quand iMWrap tente de rattacher le dump à une part.

Ce n'est pas une convention MIDI Roland universelle ; c'est une convention d'usage iMWrap telle qu'on peut la déduire du moteur.

### A.3. Stockage interne

Quand cette association réussit, le moteur stocke le payload brut Roland dans :

```text
part.customRolandSysex
```

Si la part est déjà routée sur un canal physique MT-32 mélodique (`1..8`), le moteur peut réémettre immédiatement une version adaptée de ce timbre.

### A.4. Format MT-32 généré par iMWrap

Quand iMWrap fabrique lui-même un SysEx MT-32, il utilise cette structure :

```text
41 10 16 12 [addr_hi] [addr_mid] [addr_lo] [data...] [checksum] F7
```

Le checksum est calculé comme la somme négative des octets à partir de l'adresse incluse, puis masquée sur 7 bits.

### A.5. Réécriture des timbres personnalisés

Quand un dump Roland stocké est réappliqué à une part MT-32, le moteur attend :

- au moins `254` octets de payload brut
- éventuellement un `F7` final toléré
- un bloc utile de `246` octets de données de timbre après l'en-tête/adressage/checksum

Le traitement est :

1. extraction des `246` octets de timbre
2. écriture dans le slot `Memory Timbre` du canal physique cible :

```text
addr = 0x20000 + (physicalChannel << 8)
```

3. mise à jour de la Patch Memory du canal physique :

```text
addr = 0x14000 + (physicalChannel << 3)
data = 02 [physicalChannel]
```

4. envoi de Program Changes "dummy puis réel" pour forcer le MT-32 à recharger le patch

### A.6. Timbres usine MT-32

Quand il n'y a pas de `customRolandSysex`, iMWrap écrit simplement la sélection de timbre usine dans la Patch Memory :

```text
addr = 0x14000 + (physicalChannel << 3)
data = [program >> 6] [program & 0x3F]
```

Puis il envoie un Program Change de rafraîchissement.

### A.7. Instrument Roland interne iMWrap

En plus des vrais SysEx Roland présents dans la séquence, iMWrap possède une abstraction interne de timbre Roland :

- type interne `ROL `
- blob interne de `258` octets
- nom du timbre lu sur `10` caractères à partir de l'offset `7`

Ce format interne n'est pas un SysEx MIDI à écrire tel quel dans une piste.

Il sert à l'échange entre l'objet `Instrument` et les `MidiSink`.

### A.8. Conseils pratiques MT-32

- Placez les dumps SysEx Roland personnalisés avant `Allocate Part`.
- Laissez une marge temporelle, idéalement une cinquantaine de ticks, avant les premières notes ou la première allocation de part.
- Si vous voulez que le moteur rattache un dump à une part logique, veillez à respecter la convention d'association observée sur le deuxième octet du payload.

## Annexe B. AdLib : syntaxe iMWrap et layout utile

### B.1. Deux mécanismes différents

Le support AdLib dans iMWrap apparaît sous deux formes :

- les SysEx iMWrap `0x10` et `0x11`
- une abstraction interne d'instrument custom de type `ADL `

### B.2. Format canonique d'un instrument AdLib iMUSE

Le commentaire du sink AdLib documente un layout canonique de `30` octets pour un instrument OPL2 iMUSE :

| Octets | Rôle |
| --- | --- |
| `0..4` | opérateur modulateur : `avekf`, `ksl_tl`, `ar_dr`, `sl_rr`, `waveform` |
| `5..9` | opérateur porteur : `avekf`, `ksl_tl`, `ar_dr`, `sl_rr`, `waveform` |
| `10` | registre `C0` : feedback + connection |
| `11..29` | flags / extra / données additionnelles |

Le sink AdLib basique de `imwrap-v6` n'utilise explicitement que la configuration 2-op mélodique standard.

### B.3. `0x10` AdLib Part Instrument

Format :

```text
F0 7D 10 [PART] [UNKNOWN] [30 octets AdLib en nibbles] F7
```

Recommandation :

- considérez `30` octets décodés comme le format auteur canonique
- même si certains chemins du moteur sont plus permissifs, n'utilisez pas de payload tronqué en authoring réel

### B.4. `0x11` AdLib Global Instrument

Format :

```text
F0 7D 11 [UNKNOWN] [VALUE] [PROGRAM] [30 octets AdLib en nibbles] F7
```

Usage :

- même famille de données qu'en `0x10`
- diffusion à toutes les parts actives

### B.5. Ce que fait le sink AdLib iMWrap

Quand l'instrument interne `ADL ` est envoyé vers le backend AdLib :

- iMWrap crée un bank custom dédié
- `MSB = 0x7D`
- `LSB = channel`
- l'instrument est stocké dans le slot `0` de cette banque
- la part bascule ensuite sur cette banque et ce patch

Autrement dit, côté implémentation :

- banque custom iMWrap AdLib = `MSB 0x7D`
- sous-banque = numéro de canal logique
- patch utilisé = `0`

### B.6. `0x60 Set Instrument` et AdLib

`0x60` n'est pas le même mécanisme que `0x10` / `0x11`.

- `0x10` / `0x11` : transportent un vrai payload d'instrument OPL
- `0x60` : affecte un identifiant `bank/program`

Pour un authoring AdLib orienté registre OPL, les messages centraux sont `0x10` et `0x11`, pas `0x60`.

## Annexe C. Divergences importantes entre doc simplifiée et code

### C.1. `0x21 Parameter Adjust`

- doc simplifiée : présenté comme dynamique
- code actuel : reconnu, mais sans effet runtime

### C.2. `0x40 Marker`

- doc simplifiée : peut laisser penser à un marqueur texte unique
- code actuel : traite un octet de payload utile = un trigger

### C.3. `0x50 Set Loop`

- doc simplifiée : peut suggérer `count = 0` pour l'infini
- code actuel : `count = 0` désactive la boucle

### C.4. `0x00 Allocate Part` et pan

- doc simplifiée : souvent présentée en logique `0..127`
- code actuel : lit `pan` comme signé centré sur `0`

## 9. Recommandation finale d'authoring

Pour un morceau iMWrap stable et prévisible :

1. placez `0x02` au tout début
2. placez un `0x00` par part utilisée
3. n'utilisez `0x21` que si vous acceptez qu'il n'ait aucun effet aujourd'hui
4. utilisez `0x40` avec un seul octet utile par trigger
5. pour `0x50`, utilisez `count >= 1`
6. pour MT-32 custom, placez les dumps suffisamment tôt avant l'activation musicale
7. pour AdLib, considérez `30` octets comme taille canonique d'instrument
