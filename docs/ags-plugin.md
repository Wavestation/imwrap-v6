# Plugin AGS `agsimuse`

Ce plugin intègre le moteur d'iMUSE Classic compatible SCUMM v6 au moteur d'Adventure Game Studio (AGS). Il gère le transport MIDI, les changements de tempo, les boucles et les sauts interactifs (*hooks*) de manière autonome et multithreadée.

## Fonctionnement Interne

Le plugin embarque :
1. **FluidSynth** comme synthétiseur logiciel pour faire le rendu des pistes General MIDI ou Roland MT-32.
2. **miniaudio** pour ouvrir le périphérique audio de l'ordinateur en tâche de fond. Le callback de rendu audio de miniaudio se charge à chaque itération :
   - D'avancer la position temporelle dans la musique (`advanceAll` avec conversion temps réel secondes ➡️ ticks).
   - De générer les échantillons audio de manière fluide dans la sortie audio.

## API de Script AGS

Au démarrage, le plugin injecte automatiquement l'en-tête de script suivant dans l'éditeur AGS, rendant ces fonctions de contrôle et de choix de pilote disponibles partout :

```c
#define IMUSE_PLUGIN_VERSION 101

#define IMUSE_DRIVER_FLUIDSYNTH    0
#define IMUSE_DRIVER_ADLIB         1
#define IMUSE_DRIVER_HARDWARE_GM   2
#define IMUSE_DRIVER_HARDWARE_MT32 3

// Charge une banque d'instruments/séquences iMUSE au format .ims
import void iMuse_LoadBank(const string filename);

// Charge une SoundFont au format .sf2 (raccourci pour iMuse_SetDriver(IMUSE_DRIVER_FLUIDSYNTH, filename))
import void iMuse_LoadSoundFont(const string filename);

// Configure le pilote audio et le périphérique MIDI associé
// - driverType : Pilote souhaité (IMUSE_DRIVER_FLUIDSYNTH, IMUSE_DRIVER_ADLIB, etc.)
// - deviceOrPath : 
//   - Pour FluidSynth : le chemin vers le fichier .sf2 (ex: "music/arachno.sf2")
//   - Pour AdLib : non utilisé (passer "")
//   - Pour Hardware MIDI OUT : l'index du périphérique sous forme de chaîne (ex: "0", "1")
import void iMuse_SetDriver(int driverType, const string deviceOrPath);

// Retourne le nombre de ports MIDI OUT matériels disponibles sur la machine
import int  iMuse_GetMIDIDeviceCount();

// Retourne le nom d'un périphérique MIDI OUT par son index
import const string iMuse_GetMIDIDeviceName(int index);

// Démarre la lecture d'un son par son ID
import void iMuse_StartSound(int soundId);

// Arrête la lecture d'un son par son ID
import void iMuse_StopSound(int soundId);

// Arrête tous les sons en cours de lecture
import void iMuse_StopAllSounds();

// Retourne 1 si le son spécifié est actif/en cours de lecture, sinon 0
import int  iMuse_IsSoundActive(int soundId);

// Configure un hook/saut iMUSE interactif
// - soundId : ID du son cible
// - hookClass : Classe du hook
// - hookValue : Valeur associée au hook
// - hookChannel : Canal MIDI cible
import void iMuse_SetHook(int soundId, int hookClass, int hookValue, int hookChannel);

// --- CONTRÔLE GLOBAL DU LECTEUR ---

// Modifie le volume maître de la bibliothèque (0 à 127)
import void iMuse_SetMasterVolume(int volume);

// Modifie le volume général du son (0 à 127)
import void iMuse_SetSoundVolume(int soundId, int volume);

// Modifie le panoramique du son (-128 à 127)
import void iMuse_SetSoundPan(int soundId, int pan);

// Transpose l'ensemble du son (relative: 1 ou 0, transpose: demi-tons)
import void iMuse_SetSoundTranspose(int soundId, int relative, int transpose);

// Modifie la vitesse de lecture globale du son (0 à 255)
import void iMuse_SetSoundSpeed(int soundId, int speed);

// Définit la priorité globale du son
import void iMuse_SetSoundPriority(int soundId, int priority);

// --- CONTRÔLE DES PISTES (CANAUX) ---

// Modifie le volume d'un canal spécifique (0 à 127)
import void iMuse_SetPartVolume(int soundId, int channel, int volume);

// Active ou désactive un canal spécifique (1 ou 0)
import void iMuse_SetPartOnOff(int soundId, int channel, int onOff);

// --- FLOW CONTROL (SAUTS & BOUCLES) ---

// Effectue un saut interactif direct vers une mesure/tick donnée
import void iMuse_Jump(int soundId, int track, int beat, int tick);

// Scanne le fichier MIDI pour avancer vers une mesure/tick sans jouer les notes
import void iMuse_Scan(int soundId, int track, int beat, int tick);

// Définit une boucle temporelle dynamique
import void iMuse_SetLoop(int soundId, int count, int toBeat, int toTick, int fromBeat, int fromTick);

// Efface la boucle active
import void iMuse_ClearLoop(int soundId);

// Effectue un fade fluide du volume vers une cible sur une durée donnée (en ticks)
import void iMuse_Fade(int soundId, int targetVolume, int timeInTicks);

// --- CONFIGURATION DIVERS ---

// Active (1) ou désactive (0) le mode natif Roland MT-32
import void iMuse_SetNativeMt32(int enabled);

// Retourne la piste (track) de lecture actuelle du son
import int  iMuse_GetPlaybackTrack(int soundId);

// Retourne la mesure/beat actuel du son
import int  iMuse_GetPlaybackBeat(int soundId);

// Retourne le tick actuel du son (dans le beat)
import int  iMuse_GetPlaybackTick(int soundId);

// --- REQUÊTES D'ÉTAT SUPPLÉMENTAIRES ---

// Retourne le statut d'un son (0 = inactif, 1 = actif/en cours, 2 = en attente/planifié)
import int  iMuse_GetSoundStatus(int soundId);

// Retourne le nombre total de sons actuellement actifs
import int  iMuse_GetActiveSoundCount();

// Retourne l'ID du son actif à l'index donné
import int  iMuse_GetActiveSoundId(int index);

// Retourne le tempo actuel (en microsecondes par noire, ex: 500000 pour 120 BPM)
import int  iMuse_GetTempo();

// --- COMPATIBILITÉ & MAPPAGE ROLAND MT-32 ---

// Définit le profil de compatibilité du moteur (0 = Generic v6 standard, 1 = Sam & Max Hit the Road)
import void iMuse_SetCompatibilityProfile(int profile);

// Retourne le profil de compatibilité actuel (0 = Generic v6 standard, 1 = Sam & Max)
import int  iMuse_GetCompatibilityProfile();

// Associe un nom de timbre Roland MT-32 à un programme General MIDI (GM)
import void iMuse_RegisterRolandTimbreMapping(const string name, int gmProgram);

// Efface tous les mappages de timbres Roland MT-32 enregistrés
import void iMuse_ClearRolandTimbreMappings();

// Définit le message d'accueil personnalisé envoyé à l'écran de l'émulateur/synthétiseur MT-32 (20 caractères max)
import void iMuse_SetWelcomeMessage(const string message);
```

## Intégration dans AGS

1. **Placer le plugin** :
   - Copiez la DLL du plugin (`agsimuse.dll` sur Windows, ou `libagsimuse.so` sur Linux) dans le dossier de votre projet AGS.
   - Activez le plugin dans l'éditeur d'AGS (clic droit sur "Plugins" dans l'arborescence, puis sélectionnez "iMUSE Classic v6 AGS Plugin").

2. **Écrire le script de configuration du pilote** :
   ```c
   // Dans game_start() :
   
   // Option 1 : Synthétiseur logiciel FluidSynth (GM)
   iMuse_SetDriver(IMUSE_DRIVER_FLUIDSYNTH, "music/arachno.sf2");
   
   // Option 2 : Émulateur logiciel AdLib (FM OPL3 autonome)
   // iMuse_SetDriver(IMUSE_DRIVER_ADLIB, "");
   
   // Option 3 : Synthétiseur physique MT-32 connecté sur le port 0
   // iMuse_SetDriver(IMUSE_DRIVER_HARDWARE_MT32, "0");

   iMuse_LoadBank("music/openquest-lite.ims");

   // Lancer la musique
   iMuse_StartSound(80);

   // Optionnel : Configurer une boucle infinie de la mesure 10 à la mesure 20
   iMuse_SetLoop(80, 0, 10, 0, 20, 0);
   ```

3. **Recevoir les déclencheurs (Triggers) de synchronisation musicale** :
   Le plugin C++ surveille la lecture et intercepte les marqueurs MIDI. Si un marqueur est atteint, il appelle une fonction dans votre Global Script AGS.
   
   ```c
   // Dans votre Global Script :
   function iMuse_OnTrigger(int soundId, int markerId) {
       if (soundId == 80 && markerId == 12) {
           // Synchronisation d'une animation avec la musique !
           Display("Marqueur 12 déclenché !");
       }
   }
   ```

## Gestion des ressources dans le paquet AGS (.vox)

Pour distribuer votre jeu avec les fichiers musicaux (`.ims`) et les SoundFonts (`.sf2`), vous pouvez utiliser les méthodes suivantes dans AGS :

### 1. Fichiers externes (Loose Files)
La méthode la plus simple consiste à placer vos fichiers dans le dossier de compilation (par exemple, dans `Compiled/Windows/music/`).
- **Chargement dans le script :**
  ```c
  iMuse_SetDriver(IMUSE_DRIVER_FLUIDSYNTH, "music/default.sf2");
  iMuse_LoadBank("music/game.ims");
  ```
- **Avantages :** Facile à mettre à jour et aucun impact sur le temps de compilation du jeu.
- **Inconvénients :** Les fichiers musicaux sont directement visibles par les joueurs dans le dossier d'installation.

### 2. Intégration via "Package custom data folder(s)"
À partir d'AGS 3.6.0+, vous pouvez intégrer n'importe quel dossier de ressources directement dans le paquet de jeu compilé (`.vox` ou l'exécutable) :

1. Créez un dossier (ex: `music_data`) à la racine de votre projet AGS et placez-y vos fichiers `.ims` et `.sf2`.
2. Dans l'éditeur d'AGS, ouvrez **General Settings**, puis trouvez la section **Compiler**.
3. Dans la propriété **Package custom data folder(s)**, entrez le nom de votre dossier (ex: `music_data`).
4. Dans vos scripts, référencez ces ressources à l'aide du préfixe `$DATA$` :
   ```c
   iMuse_SetDriver(IMUSE_DRIVER_FLUIDSYNTH, "$DATA$/music_data/default.sf2");
   iMuse_LoadBank("$DATA$/music_data/game.ims");
   ```

### 3. Fonctionnement technique sous le capot
- **Fichiers `.ims` :**
  Le plugin appelle `IAGSEngine::OpenFileStream` fourni par l'interface d'AGS (disponible à partir de la version d'interface 28 / AGS 3.6.0+). Cette API recherche et lit le fichier directement depuis l'archive virtuelle AGS (dans le fichier `.vox` ou l'exécutable). Le plugin charge la totalité des octets en mémoire puis la passe au parseur d'iMUSE.
- **Fichiers SoundFont `.sf2` :**
  Étant donné que FluidSynth nécessite un chemin de fichier brut pour son chargement et n'utilise pas l'API de streaming d'AGS, le plugin fait appel à `IAGSEngine::ResolveFilePath` (disponible depuis l'interface 27). Cette fonction résout les chemins spéciaux comme `$DATA$/...` en chemins d'accès système absolus valides sur la machine de l'utilisateur, permettant à FluidSynth de charger la SoundFont directement depuis le disque.
  *Note : Pour un chargement optimal, assurez-vous que les SoundFonts sont déployées en tant que fichiers physiques accessibles, ou gérées par le mécanisme de répertoires personnalisés d'AGS.*
	