# Plugin AGS `agsimwrap`

Ce plugin intègre le moteur iMWrap v6, compatible avec l'héritage SCUMM v6/iMUSE, au moteur d'Adventure Game Studio (AGS). Il gère le transport MIDI, les changements de tempo, les boucles et les sauts interactifs (*hooks*) de manière autonome et multithreadée.

## Fonctionnement Interne

Le plugin embarque :
1. **FluidSynth** comme synthétiseur logiciel pour faire le rendu des pistes General MIDI ou Roland MT-32.
2. **miniaudio** pour ouvrir le périphérique audio de l'ordinateur en tâche de fond. Le callback de rendu audio de miniaudio se charge à chaque itération :
   - D'avancer la position temporelle dans la musique (`advanceAll` avec conversion temps réel secondes ➡️ ticks).
   - De générer les échantillons audio de manière fluide dans la sortie audio.

## API de Script AGS

Au démarrage, le plugin injecte automatiquement l'en-tête de script suivant dans l'éditeur AGS, rendant ces fonctions de contrôle et de choix de pilote disponibles partout :

```c
#define IMWRAP_PLUGIN_VERSION 101

#define IMWRAP_DRIVER_FLUIDSYNTH    0
#define IMWRAP_DRIVER_ADLIB         1
#define IMWRAP_DRIVER_HARDWARE_GM   2
#define IMWRAP_DRIVER_HARDWARE_MT32 3

// Charge une banque d'instruments/séquences iMWrap au format .ims
import void iMWrap_LoadBank(const string filename);

// Charge une SoundFont au format .sf2 (raccourci pour iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, filename))
import void iMWrap_LoadSoundFont(const string filename);

// Configure le pilote audio et le périphérique MIDI associé
// - driverType : Pilote souhaité (IMWRAP_DRIVER_FLUIDSYNTH, IMWRAP_DRIVER_ADLIB, etc.)
// - deviceOrPath : 
//   - Pour FluidSynth : le chemin vers le fichier .sf2 (ex: "music/arachno.sf2")
//   - Pour AdLib : non utilisé (passer "")
//   - Pour Hardware MIDI OUT : l'index du périphérique sous forme de chaîne (ex: "0", "1")
import void iMWrap_SetDriver(int driverType, const string deviceOrPath);

// Retourne le nombre de ports MIDI OUT matériels disponibles sur la machine
import int  iMWrap_GetMIDIDeviceCount();

// Retourne le nom d'un périphérique MIDI OUT par son index
import const string iMWrap_GetMIDIDeviceName(int index);

// Démarre la lecture d'un son par son ID
import void iMWrap_StartSound(int soundId);

// Arrête la lecture d'un son par son ID
import void iMWrap_StopSound(int soundId);

// Arrête tous les sons en cours de lecture
import void iMWrap_StopAllSounds();

// Retourne 1 si le son spécifié est actif/en cours de lecture, sinon 0
import int  iMWrap_IsSoundActive(int soundId);

// Configure un hook/saut iMWrap interactif
// - soundId : ID du son cible
// - hookClass : Classe du hook
// - hookValue : Valeur associée au hook
// - hookChannel : Canal MIDI cible
import void iMWrap_SetHook(int soundId, int hookClass, int hookValue, int hookChannel);

// --- CONTRÔLE GLOBAL DU LECTEUR ---

// Modifie le volume maître de la bibliothèque (0 à 127)
import void iMWrap_SetMasterVolume(int volume);

// Modifie le volume général du son (0 à 127)
import void iMWrap_SetSoundVolume(int soundId, int volume);

// Modifie le panoramique du son (-128 à 127)
import void iMWrap_SetSoundPan(int soundId, int pan);

// Transpose l'ensemble du son (relative: 1 ou 0, transpose: demi-tons)
import void iMWrap_SetSoundTranspose(int soundId, int relative, int transpose);

// Modifie la vitesse de lecture globale du son (0 à 255)
import void iMWrap_SetSoundSpeed(int soundId, int speed);

// Définit la priorité globale du son
import void iMWrap_SetSoundPriority(int soundId, int priority);

// --- CONTRÔLE DES PISTES (CANAUX) ---

// Modifie le volume d'un canal spécifique (0 à 127)
import void iMWrap_SetPartVolume(int soundId, int channel, int volume);

// Active ou désactive un canal spécifique (1 ou 0)
import void iMWrap_SetPartOnOff(int soundId, int channel, int onOff);

// --- FLOW CONTROL (SAUTS & BOUCLES) ---

// Effectue un saut interactif direct vers une mesure/tick donnée
import void iMWrap_Jump(int soundId, int track, int beat, int tick);

// Scanne le fichier MIDI pour avancer vers une mesure/tick sans jouer les notes
import void iMWrap_Scan(int soundId, int track, int beat, int tick);

// Définit une boucle temporelle dynamique
import void iMWrap_SetLoop(int soundId, int count, int toBeat, int toTick, int fromBeat, int fromTick);

// Efface la boucle active
import void iMWrap_ClearLoop(int soundId);

// Effectue un fade fluide du volume vers une cible sur une durée donnée (en ticks)
import void iMWrap_Fade(int soundId, int targetVolume, int timeInTicks);

// --- CONFIGURATION DIVERS ---

// Active (1) ou désactive (0) le mode natif Roland MT-32
import void iMWrap_SetNativeMt32(int enabled);

// Retourne la piste (track) de lecture actuelle du son
import int  iMWrap_GetPlaybackTrack(int soundId);

// Retourne la mesure/beat actuel du son
import int  iMWrap_GetPlaybackBeat(int soundId);

// Retourne le tick actuel du son (dans le beat)
import int  iMWrap_GetPlaybackTick(int soundId);

// --- REQUÊTES D'ÉTAT SUPPLÉMENTAIRES ---

// Retourne le statut d'un son (0 = inactif, 1 = actif/en cours, 2 = en attente/planifié)
import int  iMWrap_GetSoundStatus(int soundId);

// Retourne le nombre total de sons actuellement actifs
import int  iMWrap_GetActiveSoundCount();

// Retourne l'ID du son actif à l'index donné
import int  iMWrap_GetActiveSoundId(int index);

// Retourne le tempo actuel (en microsecondes par noire, ex: 500000 pour 120 BPM)
import int  iMWrap_GetTempo();

// --- COMPATIBILITÉ & MAPPAGE ROLAND MT-32 ---

// Définit le profil de compatibilité du moteur (0 = Generic v6 standard, 1 = SNM)
import void iMWrap_SetCompatibilityProfile(int profile);

// Retourne le profil de compatibilité actuel (0 = Generic v6 standard, 1 = SNM)
import int  iMWrap_GetCompatibilityProfile();

// Associe un nom de timbre Roland MT-32 à un programme General MIDI (GM)
import void iMWrap_RegisterRolandTimbreMapping(const string name, int gmProgram);

// Efface tous les mappages de timbres Roland MT-32 enregistrés
import void iMWrap_ClearRolandTimbreMappings();

// Définit le message d'accueil personnalisé envoyé à l'écran de l'émulateur/synthétiseur MT-32 (20 caractères max)
import void iMWrap_SetWelcomeMessage(const string message);

// --- CONFIGURATION EXTERNE ---

// Retourne 1 si un fichier de configuration externe valide (<nom_jeu>.imc) existe, sinon 0
import int  iMWrap_HasExternalConfig();

// Applique la configuration du fichier externe (<nom_jeu>.imc).
// Si FluidSynth est configuré, utilise le paramètre fallbackSoundFont.
// Retourne 1 en cas de succès, 0 en cas d'échec ou d'absence de configuration.
import int  iMWrap_ApplyExternalConfig(const string fallbackSoundFont);

// Active (1) ou désactive (0) l'écriture des logs dans imwrap_debug.log et la console (désactivé par défaut)
import void iMWrap_EnableLog(int enabled);
```

## Intégration dans AGS

1. **Placer le plugin** :
   - Copiez la DLL du plugin (`agsimwrap.dll` sur Windows, ou `libagsimwrap.so` sur Linux) dans le dossier de votre projet AGS.
   - Activez le plugin dans l'éditeur d'AGS (clic droit sur "Plugins" dans l'arborescence, puis sélectionnez "iMWrap v6 AGS Plugin").

2. **Écrire le script de configuration du pilote** :
   ```c
   // Dans game_start() :
   
   // Option 1 : Synthétiseur logiciel FluidSynth (GM)
   iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, "music/arachno.sf2");
   
   // Option 2 : Émulateur logiciel AdLib (FM OPL3 autonome)
   // iMWrap_SetDriver(IMWRAP_DRIVER_ADLIB, "");
   
   // Option 3 : Synthétiseur physique MT-32 connecté sur le port 0
   // iMWrap_SetDriver(IMWRAP_DRIVER_HARDWARE_MT32, "0");

   iMWrap_LoadBank("music/openquest-lite.ims");

   // Lancer la musique
   iMWrap_StartSound(80);

   // Optionnel : Configurer une boucle infinie de la mesure 10 à la mesure 20
   iMWrap_SetLoop(80, 0, 10, 0, 20, 0);
   ```

3. **Recevoir les déclencheurs (Triggers) de synchronisation musicale** :
   Le plugin C++ surveille la lecture et intercepte les marqueurs MIDI. Si un marqueur est atteint, il appelle une fonction dans votre Global Script AGS.
   
   ```c
   // Dans votre Global Script :
   function iMWrap_OnTrigger(int soundId, int markerId) {
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
  iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, "music/default.sf2");
  iMWrap_LoadBank("music/game.ims");
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
   iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, "$DATA$/music_data/default.sf2");
   iMWrap_LoadBank("$DATA$/music_data/game.ims");
   ```

### 3. Fonctionnement technique sous le capot
- **Fichiers `.ims` :**
  Le plugin appelle `IAGSEngine::OpenFileStream` fourni par l'interface d'AGS (disponible à partir de la version d'interface 28 / AGS 3.6.0+). Cette API recherche et lit le fichier directement depuis l'archive virtuelle AGS (dans le fichier `.vox` ou l'exécutable). Le plugin charge la totalité des octets en mémoire puis la passe au parseur d'iMWrap.
- **Fichiers SoundFont `.sf2` :**
  FluidSynth nécessite un chemin de fichier brut et n'utilise pas directement l'API de streaming d'AGS. Le plugin commence donc par appeler `IAGSEngine::ResolveFilePath` (interface 27+) pour résoudre les chemins spéciaux comme `$DATA$/...`. Si ce chemin ne correspond pas à un vrai fichier physique mais qu'AGS peut encore ouvrir la ressource via `OpenFileStream` (interface 28+), le plugin extrait automatiquement la SoundFont vers un fichier temporaire avant de l'envoyer à FluidSynth.
  *Note : ce fallback permet de charger aussi les SoundFonts empaquetées dans les données du jeu, pas seulement les fichiers loose présents à côté de l'exécutable.*

## Configuration MIDI Externe (`.imc` et outil `SetMIDI`)

Le plugin permet aux joueurs de choisir leur propre pilote MIDI (FluidSynth, AdLib, General MIDI matériel, Roland MT-32 matériel) via un fichier de configuration externe.

### 1. Format du fichier de configuration (`.imc`)
Le fichier de configuration utilise le format INI standard. Il doit porter le nom de l'exécutable du jeu avec l'extension `.imc` (ex: `MonJeu.imc`) et être placé dans le même dossier que le jeu.

Exemple de contenu pour un périphérique matériel :
```ini
[MIDI]
Driver=2
Device=loopMIDI Port
```

- **Driver** : Le type de pilote à utiliser.
  - `0` : FluidSynth (Synthétiseur logiciel, requiert une SoundFont)
  - `1` : AdLib (Émulation FM OPL3)
  - `2` : Hardware General MIDI
  - `3` : Hardware Roland MT-32
- **Device** : Nom du port MIDI de sortie (par exemple `loopMIDI Port` ou l'index du périphérique comme `0`). Ce paramètre est ignoré pour FluidSynth et AdLib.

### 2. Intégration dans le script de jeu AGS
Vous pouvez vérifier s'il existe une configuration définie par le joueur et l'appliquer. Si elle n'existe pas, vous pouvez définir un pilote par défaut (par exemple, FluidSynth avec votre Soundfont intégrée).

```c
// Dans game_start() :

if (iMWrap_HasExternalConfig()) {
    // Tente d'appliquer le choix du joueur.
    // Si le joueur a choisi FluidSynth, le plugin chargera la SoundFont passée en paramètre.
    if (!iMWrap_ApplyExternalConfig("music/arachno.sf2")) {
        // Fallback en cas d'échec d'ouverture du périphérique du joueur (ex: pas de musique)
    }
} else {
    // Le joueur n'a pas configuré de pilote.
    // Configurez le comportement par défaut (ex: FluidSynth avec la SoundFont du jeu)
    iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, "music/arachno.sf2");
}

iMWrap_LoadBank("music/game.ims");
iMWrap_StartSound(80);
```

Si le joueur n'a défini aucune configuration et que le développeur ne configure aucun pilote via `iMWrap_SetDriver`, le plugin reste silencieux (fallback par défaut sans musique ni plantage).

### 3. Utilitaire `SetMIDI`
L'utilitaire `SetMIDI.exe` (fourni avec le plugin) est un outil de configuration graphique simple écrit en Win32 natif.
- **Placement** : Il doit être copié à côté de l'exécutable de votre jeu.
- **Détection automatique** : Au lancement, il recherche l'exécutable du jeu présent dans son dossier afin d'éditer automatiquement le fichier `.imc` correspondant.
- **Localisation** : L'interface s'adapte automatiquement à la langue du système en français, espagnol ou anglais (langue par défaut).
- **Sélection des ports** : Si l'utilisateur choisit un pilote matériel (GM ou MT-32), la liste des ports MIDI OUT disponibles s'active pour lui permettre de choisir son périphérique.

	
