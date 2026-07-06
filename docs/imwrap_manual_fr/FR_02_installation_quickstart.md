# Chapitre 2 : Plugin AGS - Installation et Prise en main

Dans ce chapitre, nous allons mettre en place le moteur iMWrap au sein de votre projet AGS. Nous allons l'installer, configurer le pilote audio, et jouer votre première note de musique. 

Même si vous n'avez jamais utilisé de plugins dans AGS, suivez ce guide pas à pas.

---

## 2.1. Prérequis et Installation du Plugin

L'intégration d'iMWrap dans AGS repose sur un simple plugin de type DLL (Dynamic Link Library) sous Windows. Plus précisément, il y a deux plugins disticts : un pour le run-time (le moteur iMWrap lui-même) et un pour l'éditeur, qui facilite la manipulation des fichiers ressources propres à iMWrap.

### Étape 1 : Obtenir les fichiers
Vous devriez être en possession de ces éléments :
- La DLL runtime : `agsimwrap-x32.dll` (le cœur du moteur pour Windows)
- Le plugin d'éditeur : `AGS.Plugin.IMWrap.Editor.dll` et `imwrap_shim.dll` (nécessaires pour l'interface de l'éditeur AGS)
- Une banque musicale : un fichier `.ims` contenant votre musique. Il y en a une fournie dans le dossier "examples" dans l'archive de la release.
- (Optionnel mais recommandé) Une SoundFont : un fichier `.sf2` contenant les instruments si vous utilisez le profil General MIDI. Si vous n'en possédez pas, vous en trouverez une de très bonne qualité via ce lien : [https://archive.org/details/SGM-V2.01](https://archive.org/details/SGM-V2.01)

### Étape 2 : Ajouter les plugins à l'éditeur AGS
1. Pour que le système fonctionne, glissez ces 3 fichiers DLL (`agsimwrap-x32.dll`, `AGS.Plugin.IMWrap.Editor.dll`, et `imwrap_shim.dll`) **dans le dossier d'installation principal d'AGS** (là où se trouve `AGSEditor.exe`).
2. Ouvrez votre projet avec l'éditeur AGS.
3. Dans l'arborescence à droite (le *Project Tree*), Développez le nœud **Plugins**.
4. Trouvez **iMWrap v6 AGS Plugin**, faites un clic-droit dessus, et cochez la case pour l'activer.

<img width="303" height="111" alt="image" src="https://github.com/user-attachments/assets/724ddb34-f295-4c61-a9c1-59d3f6cdd3fa" />

Vous noterez que le plugin éditeur, lui, n'a pas besoin d'être activé, et son icône (une petite note de musique) apparaît directement dans l'arborescence. Nous reviendrons dans un chapitre ultérieur sur l'utilisation de ce plugin.

> [!TIP]
> **Vérification** : Une fois activé, l'éditeur AGS intègre automatiquement toutes les fonctions du plugin (comme `iMWrap_StartSound`) dans son système d'auto-complétion ! Vous n'avez pas de script `.ash` à importer manuellement.

---

## 2.2. L'organisation des fichiers (Le dossier Data)

Pour que la musique joue, le moteur a besoin de trouver vos fichiers `.ims` et `.sf2`. 
La manière moderne et propre de gérer cela dans AGS (version 3.6.0+) est d'utiliser un dossier de "Custom Data".

1. Créez un dossier appelé, par exemple, `music_data` directement dans le dossier de votre jeu.
2. Placez-y vos fichiers (par exemple `game.ims` et `SGM-V2.01.sf2`).
3. Dans l'éditeur AGS, ouvrez le nœud **General Settings**, descendez dans la catégorie **Compiler**.
4. Dans le champ **Package custom data folder(s)**, tapez `music_data`.

Lors de la compilation, AGS empaquettera discrètement votre musique dans l'exécutable ou le fichier `.vox`, protégeant ainsi vos ressources des regards curieux !

---

## 2.3. Initialisation : Le script `game_start()`

C'est ici que la magie commence. Nous allons dire à AGS d'allumer le moteur audio dès le lancement du jeu. Ouvrez le **GlobalScript.asc**, et trouvez la fonction `game_start()`.

Ajoutez-y ces quelques lignes :

```c
function game_start() {
    
    // 1. Définir le pilote audio (Synthétiseur)
    // Nous utilisons le synthétiseur intégré (FluidSynth) et nous lui donnons notre SoundFont.
    // Le mot clé $DATA$ indique à AGS de chercher dans le "Package custom data folder" configuré précédemment.
    iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, "$DATA$/music_data/SGM-V2.01.sf2");
    
    // 2. Charger notre partition (la banque IMS)
    iMWrap_LoadBank("$DATA$/music_data/game.ims");
    
    // Le moteur est prêt !
}
```

### Comprendre `iMWrap_SetDriver`
La fonction prend deux paramètres : le type de pilote, et une chaîne de caractères (qui sert de chemin de fichier, ou d'identifiant selon le pilote).
- `IMWRAP_DRIVER_FLUIDSYNTH` : Utilise le moteur `.sf2`. Le paramètre 2 est le chemin vers la SoundFont.
- `IMWRAP_DRIVER_ADLIB` : Émule la puce OPL. Le paramètre 2 est ignoré (laisser `""`).
- `IMWRAP_DRIVER_HARDWARE_MT32` : Envoie le signal vers un vrai synthétiseur Roland branché sur votre PC. Le paramètre 2 est l'index du port MIDI de Windows (ex: `"0"`).

*(Note : Nous verrons au Chapitre 7 comment laisser le joueur choisir lui-même son pilote favori).*

---

## 2.4. Lire et Arrêter un son

Maintenant que la banque est chargée, vous pouvez déclencher la musique n'importe où dans votre code de jeu (quand le joueur entre dans une pièce, clique sur un bouton, etc.).

Dans iMWrap, les musiques n'ont pas de "nom de fichier" direct. Elles sont stockées dans la banque IMS sous forme de **numéros (Sound ID)**. Par tradition LucasArts, on donne des numéros de dizaines ou vingtaines (ex: 80 pour la musique de forêt, 81 pour l'intérieur d'une cabane), mais vous pouvez vous organiser comme vous le souhaitez.

Pour lancer la musique 80 :
```c
iMWrap_StartSound(80);
```

Pour arrêter la musique 80 en douceur :
```c
iMWrap_StopSound(80);
```

Pour tout couper (utile lors d'un Game Over brutal ou d'un saut au menu principal) :
```c
iMWrap_StopAllSounds();
```

Pour vérifier si une musique est en train de tourner :
```c
if (iMWrap_IsSoundActive(80)) {
    Display("La musique de la forêt joue actuellement.");
}
```

> [!NOTE]
> La fonction `iMWrap_StopSound(80)` n'est pas une "pause" au sens audio du terme. Elle coupe proprement les notes en cours (en envoyant des signaux MIDI *Note Off* pour que le son s'estompe naturellement) et rembobine la partition. 

Vous savez maintenant initialiser le moteur et lire une musique ! Dans le **Chapitre 3**, nous apprendrons à manipuler ce son en temps réel (changer le volume, le tempo, et faire des fondus cinématographiques).
