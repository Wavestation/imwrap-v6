# Chapitre 8 : Manuel de Référence de l'API AGS

Ce chapitre répertorie **l'exhaustivité des fonctions, constantes et définitions** injectées par le plugin `agsimwrap` dans le Global Script de votre projet AGS. Chaque fonction est accompagnée d'un exemple d'utilisation typique.

---

> [!IMPORTANT]
> **Indexation à zéro (0-indexed)**
> Dans toute l'API AGS d'iMWrap, les identifiants et les canaux sont indexés à 0. 
> Par exemple, si vous voulez affecter le **Canal MIDI 1** de votre musique, vous devez utiliser la valeur `0` pour le paramètre `channel`. De même, les mesures (beats) commencent à 0 (la "mesure 1" de votre DAW correspond au `beat` 0 dans le code).

## 8.1. Constantes du Plugin

Ces constantes sont disponibles partout dans vos scripts pour paramétrer le pilote audio.

```c
#define IMWRAP_PLUGIN_VERSION 101

#define IMWRAP_DRIVER_FLUIDSYNTH    0  // Synthèse logicielle moderne avec SoundFont
#define IMWRAP_DRIVER_ADLIB         1  // Émulation FM style SoundBlaster/OPL3 (Support non vérifié à 100% dans la v1.0.4)
#define IMWRAP_DRIVER_HARDWARE_GM   2  // Carte son matérielle externe (General MIDI)
#define IMWRAP_DRIVER_HARDWARE_MT32 3  // Synthétiseur Roland MT-32 matériel
```

---

## 8.2. Initialisation et Périphériques

* `import void iMWrap_LoadBank(const string filename);`
  Charge la banque `.ims` principale en mémoire. À utiliser généralement une seule fois dans `game_start()`.
  ```c
  // Charge le fichier de séquences musicales du jeu
  iMWrap_LoadBank("$DATA$/music_data/ost.ims");
  ```

* `import void iMWrap_LoadSoundFont(const string filename);`
  Raccourci pour charger une SoundFont (`.sf2`) ou `.sf3` compressée et configurer le pilote sur FluidSynth d'un seul coup.
  ```c
  iMWrap_LoadSoundFont("$DATA$/music_data/SGM-V2.01.sf2");
  ```


* `import void iMWrap_SetSFDynLoad(int enabled);`
  Active (`1`) ou désactive (`0`) le chargement dynamique pour les SoundFonts `.sf3`. Si activé, les échantillons sont streamés en mémoire à la demande plutôt qu'entièrement décompressés au chargement. Doit être appelé AVANT `iMWrap_LoadSoundFont`.
  ```c
  iMWrap_SetSFDynLoad(1); // Active le streaming dynamique pour le SF3
  iMWrap_LoadSoundFont("$DATA$/music_data/SGM-V2.01.sf3");
  ```

* `import void iMWrap_SetDriver(int driverType, const string deviceOrPath);`
  Permet de définir manuellement le pilote (`IMWRAP_DRIVER_...`). Le `deviceOrPath` sert à donner le chemin de la `.sf2` (pour FluidSynth) ou l'index du port (pour le Hardware MIDI). Laissez `""` pour AdLib.
  ```c
  // Configurer pour un Roland MT-32 branché sur le port MIDI "1" de Windows
  iMWrap_SetDriver(IMWRAP_DRIVER_HARDWARE_MT32, "1");
  ```

* `import int iMWrap_GetMIDIDeviceCount();`
  Renvoie le nombre de ports MIDI physiques connectés à l'ordinateur.
  ```c
  int nbPorts = iMWrap_GetMIDIDeviceCount();
  Display("Vous avez %d appareils MIDI connectés.", nbPorts);
  ```

* `import const string iMWrap_GetMIDIDeviceName(int index);`
  Renvoie le nom textuel du port matériel à l'index donné.
  ```c
  // Affiche le nom du premier port MIDI
  Display("Port 0 : %s", iMWrap_GetMIDIDeviceName(0));
  ```

---

## 8.3. Contrôle de la lecture (Play / Stop)

* `import void iMWrap_StartSound(int soundId);`
  Démarre la lecture de la séquence portant l'ID `soundId`.
  ```c
  // Lance la musique de la taverne (ID 50)
  iMWrap_StartSound(50);
  ```

* `import void iMWrap_StopSound(int soundId);`
  Arrête en douceur (envoi de signaux Note Off) la séquence `soundId`.
  ```c
  // Coupe la musique de la taverne
  iMWrap_StopSound(50);
  ```

* `import void iMWrap_StopAllSounds();`
  Coupe tous les sons actifs simultanément. Très utile lors d'un Game Over.
  ```c
  iMWrap_StopAllSounds();
  ```

* `import int iMWrap_IsSoundActive(int soundId);`
  Renvoie `1` si la séquence est actuellement lue, sinon `0`.
  ```c
  if (iMWrap_IsSoundActive(50)) {
      cEgo.Say("La musique est trop forte ici !");
  }
  ```

* `import int iMWrap_GetSoundStatus(int soundId);`
  Renvoie `0` (inactif), `1` (en cours de lecture), ou `2` (planifié/en attente de Hook).
  ```c
  if (iMWrap_GetSoundStatus(50) == 2) {
      // La musique est en attente d'un trigger pour démarrer
  }
  ```

* `import int iMWrap_GetActiveSoundCount();`
  Renvoie le nombre total de musiques jouant en même temps en ce moment.
  ```c
  if (iMWrap_GetActiveSoundCount() > 3) {
      // Attention à la cacophonie !
  }
  ```

* `import int iMWrap_GetActiveSoundId(int index);`
  Permet d'itérer sur les sons en cours de lecture pour récupérer leur ID.
  ```c
  int i = 0;
  while (i < iMWrap_GetActiveSoundCount()) {
      int id = iMWrap_GetActiveSoundId(i);
      iMWrap_SetSoundVolume(id, 50); // Baisse le volume de tout ce qui joue
      i++;
  }
  ```

---

## 8.4. Mixage, Effets et Tempo

* `import void iMWrap_SetMasterVolume(int volume);`
  Volume général du synthétiseur (`0` à `127`).
  ```c
  // Appelé par le slider de volume dans l'interface des options
  iMWrap_SetMasterVolume(100); 
  ```

* `import void iMWrap_SetSoundVolume(int soundId, int volume);`
  Volume spécifique d'une séquence complète (`0` à `127`).
  ```c
  // Le son 50 passe à moitié de volume (64)
  iMWrap_SetSoundVolume(50, 64);
  ```

* `import void iMWrap_SetSoundPan(int soundId, int pan);`
  Panoramique stéréo (`-128` à `127`). `0` est au centre.
  ```c
  // La musique provient de l'enceinte gauche
  iMWrap_SetSoundPan(50, -128);
  ```

* `import void iMWrap_SetSoundSpeed(int soundId, int speed);`
  Vitesse de lecture. La normale est `128`.
  ```c
  // Ralentit la musique (effet de rêve)
  iMWrap_SetSoundSpeed(50, 80);
  ```

* `import void iMWrap_SetSoundTranspose(int soundId, int relative, int transpose);`
  Transpose la hauteur (`transpose` est en demi-tons). Si `relative` vaut `1`, s'ajoute à la transpo actuelle, sinon (`0`) l'écrase.
  ```c
  // Monte la musique d'un demi-ton par rapport à sa hauteur d'origine (modulation)
  iMWrap_SetSoundTranspose(50, 0, 1);
  ```

* `import void iMWrap_SetSoundPriority(int soundId, int priority);`
  Force la priorité de la séquence en cas de saturation de polyphonie.
  ```c
  // 127 est la priorité maximale
  iMWrap_SetSoundPriority(50, 127);
  ```

* `import void iMWrap_SetPartVolume(int soundId, int channel, int volume);`
  Volume ciblé sur une piste/canal précis (ex: la piste de basse) (`0` à `127`).
  ```c
  // Baisse le canal 2 de la musique 50 à 30
  iMWrap_SetPartVolume(50, 2, 30);
  ```

* `import void iMWrap_SetPartOnOff(int soundId, int channel, int onOff);`
  Allume (`1`) ou coupe (`0`) un canal spécifique en plein vol.
  ```c
  // Mute la piste de batterie (souvent canal 9)
  iMWrap_SetPartOnOff(50, 9, 0);
  ```

* `import void iMWrap_Fade(int soundId, int targetVolume, int timeInTicks);`
  Déclenche un fondu de volume fluide jusqu'à la cible sur la durée voulue (en ticks).
  ```c
  // Fondu au silence sur environ 2 mesures (à 120 bpm = ~1000 ticks)
  iMWrap_Fade(50, 0, 1000);
  ```

* `import int iMWrap_GetTempo();`
  Récupère le tempo global brut en *microsecondes par noire*.
  ```c
  int msPerQuarter = iMWrap_GetTempo();
  int bpm = 60000000 / msPerQuarter;
  Display("Tempo actuel: %d BPM", bpm);
  ```

---

## 8.5. Sauts, Boucles et Hooks

* `import void iMWrap_Jump(int soundId, int track, int beat, int tick);`
  Saut instantané (direct) à une mesure/tick précis.
  ```c
  // Saute brutalement à la mesure 20
  iMWrap_Jump(50, 0, 20, 0);
  ```

* `import void iMWrap_Scan(int soundId, int track, int beat, int tick);`
  Avance rapide silencieuse jusqu'au point ciblé (sans jouer les notes intermédiaires, mais en appliquant les changements de programmes et volumes traversés).
  ```c
  iMWrap_Scan(50, 0, 10, 0); // Démarre virtuellement à la mesure 10
  ```

* `import void iMWrap_SetLoop(int soundId, int count, int toBeat, int toTick, int fromBeat, int fromTick);`
  Force une boucle temporelle logicielle dans AGS.
  ```c
  // Boucle 5 fois. En atteignant la mesure 30, retourne à la 10.
  iMWrap_SetLoop(50, 5, 10, 0, 30, 0);
  ```

* `import void iMWrap_ClearLoop(int soundId);`
  Efface la boucle actuelle forcée.
  ```c
  // Le joueur sort de combat, on laisse la musique continuer
  iMWrap_ClearLoop(50);
  ```

* `import void iMWrap_SetHook(int soundId, int hookClass, int hookValue, int hookChannel);`
  Arme une action asynchrone (Hook) qui s'exécutera au moment voulu par le compositeur. `hookClass` doit utiliser une constante `IMWRAP_HOOK_*`. `hookValue` est l'ID attendu du Hook dans le fichier MIDI (si `0`, le Hook s'active sur le prochain de cette classe inconditionnellement).
  ```c
  // Arme l'attente du Hook de Volume ID 1.
  iMWrap_SetHook(50, IMWRAP_HOOK_PART_VOLUME, 1, 0);
  // Équivalent avec le wrapper typé :
  iMWrap_SetPartVolumeHook(50, 1, 0);
  ```
  *(Note : des wrappers existent pour chaque type de hook : `iMWrap_SetJumpHook`, `iMWrap_SetGlobalTransposeHook`, etc.)*


---


* `import int iMWrap_PopMarker();`
  Récupère et retire le plus ancien marqueur déclenché (ou valeur de Hook) de la file d'attente. Renvoie `-1` si la file est vide. 
  La valeur renvoyée "pacte" (pack) à la fois l'ID du son (sur les bits supérieurs) et la valeur du marqueur (sur les 8 bits inférieurs).
  Voici comment dépacquer ces valeurs :
  ```c
  int packed = iMWrap_PopMarker();
  while (packed != -1)
  {
      int markerValue = packed & 0xFF;         // Les 8 bits de poids faible
      int soundId = (packed >> 8) & 0xFFFFFF;  // Le reste des bits pour le soundId
      
      Display("Marqueur %d déclenché par le son %d !", markerValue, soundId);
      packed = iMWrap_PopMarker();
  }
  ```

* `import int iMWrap_GetLastMarker();`
  Renvoie le marqueur déclenché le plus récent sans vider la file d'attente. (La valeur est pacquée de la même manière que `PopMarker`, ou vaut `-1` si vide).

## 8.6. Informations de Position (Playback)

* `import int iMWrap_GetPlaybackTrack(int soundId);`
  Renvoie la piste (Track) logique système en cours de lecture.
  ```c
  int track = iMWrap_GetPlaybackTrack(50);
  ```

* `import int iMWrap_GetPlaybackBeat(int soundId);`
  Renvoie le numéro de la mesure en cours (Beat). Très utile pour débugger vos transitions.
  ```c
  if (iMWrap_GetPlaybackBeat(50) > 40) { Display("On approche de la fin !"); }
  ```

* `import int iMWrap_GetPlaybackTick(int soundId);`
  Renvoie le numéro du tick dans la mesure en cours (ex: 0 à 479 pour une mesure 4/4 à 120 PPQN).
  ```c
  int tick = iMWrap_GetPlaybackTick(50);
  ```

---

## 8.7. Profils Matériels et MT-32

Ces fonctions servent à simuler ou forcer des comportements spécifiques aux vieux moteurs LucasArts ou Roland.

* `import void iMWrap_SetNativeMt32(int enabled);`
  Active (`1`) ou désactive (`0`) le mapping MT-32 natif pour les SysEx.
  ```c
  iMWrap_SetNativeMt32(1); // Le moteur enverra des SysEx Roland natifs
  ```

* `import void iMWrap_SetCompatibilityProfile(int profile);`
  Sélectionne le mode de compatibilité interne (0 = standard iMWrap, 1 = mode de compatibilité strict SCUMM v6 / SNM).
  ```c
  iMWrap_SetCompatibilityProfile(1);
  ```

* `import int iMWrap_GetCompatibilityProfile();`
  Renvoie le profil actuel (0 ou 1).

* `import void iMWrap_RegisterRolandTimbreMapping(const string name, int gmProgram);`
  Associe manuellement un son personnalisé Roland MT-32 (qui aurait le nom `name`) à un instrument General MIDI de secours (`gmProgram`), au cas où le joueur écoute en GM.
  ```c
  // Si le MT-32 joue le timbre "SynthBrass1", le GM jouera l'instrument 62
  iMWrap_RegisterRolandTimbreMapping("SynthBrass1", 62);
  ```

* `import void iMWrap_ClearRolandTimbreMappings();`
  Efface la table de conversion manuelle.
  ```c
  iMWrap_ClearRolandTimbreMappings();
  ```

* `import void iMWrap_SetWelcomeMessage(const string message);`
  Affiche un petit message personnalisé sur le mini écran LCD physique du synthétiseur Roland MT-32 du joueur !
  ```c
  iMWrap_SetWelcomeMessage("Bienvenue aventurier");
  ```

---

## 8.8. Configuration Externe et Log

* `import int iMWrap_HasExternalConfig();`
  Vérifie si un fichier de configuration `.imc` a été posé par le joueur dans le dossier du jeu.
  ```c
  if (iMWrap_HasExternalConfig()) { /* Le joueur veut customiser l'audio */ }
  ```

* `import int iMWrap_ApplyExternalConfig(const string fallbackSoundFont);`
  Force l'application des réglages du fichier `.imc`. Le paramètre sert de `.sf2` par défaut si le `.imc` demande FluidSynth sans préciser de banque.
  ```c
  iMWrap_ApplyExternalConfig("$DATA$/music_data/arachno.sf2");
  ```

* `import void iMWrap_EnableLog(int enabled);`
  Active (`1`) l'écriture d'un fichier `imwrap_debug.log` pour comprendre ce qui se passe sous le capot (erreurs SysEx, déclenchement des hooks...).
  ```c
  iMWrap_EnableLog(1);
  ```
