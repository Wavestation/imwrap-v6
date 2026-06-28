# MusicManager — Guide d'intégration

Module AGS pour la gestion de musique interactive de style **États et Séquences** (iMUSE / LucasArts),  
basé sur le plugin **agsimwrap**.

---

## Fichiers

| Fichier | Rôle |
|---|---|
| `MusicManager.ash` | Header — déclarations publiques, enums, structs |
| `MusicManager.asc` | Script — logique complète, callback `iMWrap_OnTrigger` |

---

## Philosophie

Le module **ne gère pas les boucles depuis le code AGS**.  
Les boucles musicales sont définies dans le fichier `.ims` par le compositeur via le SysEx `0x50` :

```
F0 7D 50 00 [COUNT16] [TO_BEAT16] [TO_TICK16] [FROM_BEAT16] [FROM_TICK16] F7
```

Le script se contente de :
- **Effacer** la boucle précédente → `iMWrap_ClearLoop()`
- **Sauter** au point d'entrée de l'état → `iMWrap_Jump()`

Le moteur iMWrap lit le SysEx `0x50` et arme la boucle automatiquement.

---

## Installation

1. Copiez `MusicManager.ash` et `MusicManager.asc` dans le dossier de votre projet AGS.
2. Dans l'éditeur : clic droit sur **Scripts** → *Add existing script* → sélectionnez les deux fichiers.

---

## Configuration dans `game_start()`

```c
function game_start()
{
    // 1. Démarrer le moteur audio
    iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, "music/arachno.sf2");
    iMWrap_LoadBank("music/game.ims");
    iMWrap_StartSound(80);

    // 2. Initialiser le MusicManager
    //    soundId          = ID du son dans la banque .ims
    //    transitionMarkerId = octet du SysEx 0x40 placé en fin de chaque pont
    //                         F0 7D 40 00 [transitionMarkerId] F7
    MusicManager.Init(80, 99);

    // 3. Enregistrer les états
    //    RegisterState(stateId, trackId, entryBeat, entryTick)
    //    trackId   = track MIDI de la section (0 si architecture mono-track)
    //    entryBeat = première mesure de la section dans le MIDI
    MusicManager.RegisterState(STATE_MENU,        0,  1,  0);
    MusicManager.RegisterState(STATE_EXPLORATION, 0,  9,  0);
    MusicManager.RegisterState(STATE_TENSION,     0, 25,  0);
    MusicManager.RegisterState(STATE_COMBAT,      0, 37,  0);
    MusicManager.RegisterState(STATE_VICTORY,     0, 53,  0);
    MusicManager.RegisterState(STATE_SAD,         0, 57,  0);

    // 4. Enregistrer les séquences de transition
    //    RegisterSequence(seqId, trackId, startBeat, startTick, hookValue)
    //    trackId   = track MIDI du pont
    //    hookValue = valeur CMD du SysEx Hook Jump 0x30 (saut synchronisé)
    //                passez 0 pour un saut direct immédiat sans Hook
    MusicManager.RegisterSequence(SEQ_MENU_TO_EXPLORATION,    0, 65, 0, 1);
    MusicManager.RegisterSequence(SEQ_EXPLORATION_TO_TENSION, 0, 69, 0, 2);
    MusicManager.RegisterSequence(SEQ_TENSION_TO_COMBAT,      0, 73, 0, 3);
    MusicManager.RegisterSequence(SEQ_COMBAT_TO_VICTORY,      0, 77, 0, 4);
    MusicManager.RegisterSequence(SEQ_COMBAT_TO_EXPLORATION,  0, 81, 0, 5);
    MusicManager.RegisterSequence(SEQ_ANY_TO_SAD,             0, 85, 0, 6);

    // 5. Démarrer sur l'état initial
    MusicManager.SetState(STATE_MENU);
}
```

---

## API publique

### `MusicManager.Init(int soundId, int transitionEndMarkerId)`
Initialise le module. À appeler une seule fois dans `game_start()`, après `iMWrap_StartSound()`.

### `MusicManager.RegisterState(int stateId, int trackId, int entryBeat, int entryTick)`
Enregistre un état musical. `entryBeat` est la mesure où `iMWrap_Jump()` amènera la tête de lecture, sur `trackId`.
Passez `trackId = 0` si tous vos états sont sur la même track principale.
La boucle est gérée par le SysEx `0x50` dans le `.ims` — le module n'en a pas connaissance.

### `MusicManager.RegisterSequence(int seqId, int trackId, int startBeat, int startTick, int hookValue)`
Enregistre une séquence de transition.

| `hookValue` | Mode | Comportement |
|---|---|---|
| `> 0` | **Hook synchronisé** | `iMWrap_SetHook()` est armé ; le SysEx `0x30` dans le MIDI déclenche le saut (incl. la piste cible). `trackId` est ignoré côté script. |
| `0` | **Saut direct** | `iMWrap_Jump()` vers `trackId:startBeat` immédiatement, sans attendre de Hook dans le MIDI. |

### `MusicManager.SetState(int stateId)`
Bascule immédiatement vers un état enregistré (saut direct, sans attendre la fin de mesure).

### `MusicManager.GetCurrentState()`
Retourne l'ID de l'état actuellement actif, ou `STATE_NONE` si aucun.

### `MusicManager.Transition(int seqId, int targetStateId)`
Déclenche une transition synchronisée. Arme le Hook iMUSE (`iMWrap_SetHook`) et attend que le moteur exécute le Hook Jump SysEx dans le MIDI. À la fin du pont, `SetState(targetStateId)` est appelé automatiquement.

### `MusicManager.IsTransitionPending()`
Retourne `true` si une transition est en cours (utile pour éviter d'en empiler deux).

### `MusicManager.Stop()`
Coupe la musique et remet le module dans son état initial.

---

## Utilisation en jeu

### Changer d'ambiance directement
```c
// En entrant dans une pièce :
function room_AfterFadeIn()
{
    MusicManager.SetState(STATE_TENSION);
}
```

### Déclencher une transition musicale
```c
// Quand le combat démarre :
function btnAttack_Click(GUIControl *control, MouseButton button)
{
    if (!MusicManager.IsTransitionPending()) {
        MusicManager.Transition(SEQ_TENSION_TO_COMBAT, STATE_COMBAT);
    }
}
```

### Conditionner un comportement à l'état musical
```c
if (MusicManager.GetCurrentState() == STATE_COMBAT) {
    // Accélérer l'IA, changer l'éclairage, etc.
}
```

### Couper la musique
```c
// Avant une cinématique, un fondu au noir, etc.
MusicManager.Stop();
```

---

## Ajouter des synchronisations gameplay

`iMWrap_OnTrigger` est déjà déclaré dans `MusicManager.asc`.  
Ajoutez vos propres marqueurs dans la **zone libre** de cette fonction :

```c
function iMWrap_OnTrigger(int soundId, int markerId)
{
    // Bloc MusicManager (ne pas modifier)
    if (soundId == _soundId && markerId == _transitionMarkerId) { ... return; }

    // Vos synchronisations :
    if (soundId == 80 && markerId == 10) {
        cMonster.Animate(2, 3, eOnce, eNoBlock);
    }
}
```

---

## Responsabilités

| Qui | Quoi |
|---|---|
| **MusicManager.ash / .asc** | Logique générique — ne pas modifier pour ajouter des données |
| **Compositeur** (fichier `.ims`) | SysEx `0x50` (boucles), `0x30` (Hook Jumps), `0x40` (marqueurs de fin) |
| **`game_start()`** | `Init()` + `RegisterState()` + `RegisterSequence()` |
| **Scripts de pièces / GUI** | `SetState()` ou `Transition()` selon le contexte |

---

## Extension du projet

1. Ajoutez un état → une ligne dans l'enum `MusicStateId` (dans `MusicManager.ash`) + un appel `RegisterState()` dans `game_start()`.
2. Ajoutez une séquence → une ligne dans `MusicSequenceId` + un appel `RegisterSequence()`.
3. Si vous avez plus de 32 états ou séquences, augmentez `MUSIC_MAX_STATES` / `MUSIC_MAX_SEQUENCES` dans `MusicManager.ash`.

## Architectures MIDI multi-tracks

### Architecture A — Mono-track (classique)
Tout le contenu est sur la track `0`. C'est la configuration la plus simple.

```c
// Tous les trackId à 0
MusicManager.RegisterState(STATE_COMBAT, 0, 37, 0);
MusicManager.RegisterSequence(SEQ_TENSION_TO_COMBAT, 0, 73, 0, 3); // Hook
```

```
Track 0 : [boucle STATE_MENU] [boucle STATE_EXPL] ... [pont 1] [pont 2] ...
```

---

### Architecture B — États + ponts sur des tracks séparées
Les états bouclent sur la track `0`, les ponts de transition sont sur la track `1`.

```c
// États sur track 0
MusicManager.RegisterState(STATE_MENU,    0, 1,  0);
MusicManager.RegisterState(STATE_COMBAT,  0, 37, 0);

// Ponts sur track 1, mode Hook (le SysEx 0x30 encode lui-même la track cible)
MusicManager.RegisterSequence(SEQ_MENU_TO_EXPLORATION, 1, 65, 0, 1);
```

> **Note :** en mode Hook, le script ne connaît pas la track du pont — c'est
> le SysEx `0x30` dans le MIDI qui l'encode : `F0 7D 30 00 01 [TRACK=00 01] [BEAT] [TICK] F7`.
> `trackId = 1` est passé pour cohérence documentaire, mais non utilisé par le script.

```
Track 0 : [boucle STATE_MENU] ──Hook 0x30──►  Track 1 : [pont 1] [pont 2] ...
                                                          ↓ Marker 99
                  ◄──── SetState() iMWrap_Jump(track=0) ──────────────────
```

---

### Architecture C — Chaque état sur sa propre track
Variante avancée : chaque état a une track dédiée, les ponts aussi.

```c
MusicManager.RegisterState(STATE_MENU,    0, 1, 0);  // track 0
MusicManager.RegisterState(STATE_COMBAT,  2, 1, 0);  // track 2
MusicManager.RegisterState(STATE_VICTORY, 3, 1, 0);  // track 3

// Pont sur track 4, saut direct (hookValue=0) — pas de Hook SysEx dans le MIDI
MusicManager.RegisterSequence(SEQ_COMBAT_TO_VICTORY, 4, 1, 0, 0);
```

> **Mode saut direct (`hookValue = 0`)** : `iMWrap_Jump()` saute immédiatement
> vers `trackId:startBeat` sans attendre de SysEx Hook dans le MIDI.
> Adapteé si vos ponts n'ont pas de Hook Jump SysEx, ou si la synchronisation
> exacte à la mesure n'est pas requise.

---

```
Mesure  1 : F0 7D 50 ...          ← Boucle de STATE_MENU  (SysEx 0x50)
Mesure  9 : F0 7D 50 ...          ← Boucle de STATE_EXPLORATION
...
Mesure 65 : F0 7D 30 00 01 ...    ← Hook Jump CMD=1 vers le pont (SEQ_MENU_TO_EXPLORATION)
            ... notes du pont ...
Mesure 68 : F0 7D 40 00 63 F7     ← Marker ID=99 → déclenche SetState automatiquement
```

> **Règle** : les ponts de transition ne contiennent **pas** de SysEx `0x50` (pas de boucle).  
> Chaque `hookValue` doit être **unique** pour éviter qu'un Hook Jump déclenche la mauvaise séquence.
