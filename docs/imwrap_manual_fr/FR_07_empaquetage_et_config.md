# Chapitre 7 : Empaquetage et Déploiement

Vous avez programmé vos interactions dans AGS, et vos fichiers `.mid` regorgent de SysEx savamment construits. Il est temps d'empaqueter tout ça pour les joueurs !

Dans ce dernier chapitre, nous verrons comment fusionner vos fichiers avec `imwrappack`, et comment laisser le joueur choisir sa carte son préférée grâce au fichier de configuration `.imc`.

---

## 7.1. L'utilitaire "imwrappack"

Le moteur iMWrap ne lit pas directement des fichiers `.mid`. Il lit des banques `.ims` (iMWrap Music Set). 
Non seulement, la banque IMS contient tous les morceaux de musique du jeu, mais peut regrouper **plusieurs variantes du même morceau**. 

Pourquoi faire ça ?
Imaginons que vous ayez composé la "Musique 80" spécifiquement pour le rendu synthétique du Roland MT-32, mais vous avez fait une autre version pour le General MIDI.
Grâce à la banque IMS, le jeu AGS dira simplement `iMWrap_StartSound(80)`. Le moteur regardera la carte son du joueur et piochera automatiquement la version MT-32 ou la version GM à l'intérieur du fichier .ims !

### Utiliser imwrappack

L'outil `imwrappack` s'utilise en ligne de commande (dans l'invite de commande Windows ou le terminal Linux/Mac).

**Syntaxe basique :**
```bash
imwrappack build output.ims \
  --name=80:Foret \
  80:gmd=80_foret_generalmidi.mid \
  80:rol=80_foret_mt32.mid \
  81:gmd=81_interieur.mid
```

Dans cet exemple :
1. On crée le fichier `output.ims`.
2. `--name=80:Foret` : On donne un nom interne informatif au morceau 80 (très pratique pour le débogage).
3. `80:gmd=...` : On intègre notre fichier `.mid` comme variante **General MIDI** (`gmd`) pour le son 80.
4. `80:rol=...` : On intègre notre second fichier `.mid` comme variante **Roland MT-32** (`rol`) pour le même son 80.
5. `81:gmd=...` : On ajoute la musique 81 (variante GM uniquement).

*(Les variantes supportées sont principalement `gmd` et `rol`.)*

> [!TIP]
> Bonne nouvelle : l'utilitaire accepte désormais n'importe quel format MIDI ! Les fichiers **SMF 0** et **SMF 2** sont importés tels quels, tandis que les fichiers **SMF 1** sont joyeusement fusionnés en une seule track (façon format 0) pour vous simplifier la vie. 😎

### Configurer les priorités et volumes (`MDhd`)
Vous pouvez forcer les paramètres par défaut d'une variante (priorité, volume, vitesse, etc.) directement à l'empaquetage sans avoir à éditer le MIDI, en injectant un chunk `MDhd` :

```bash
--mdhd=80:gmd:90:127:0:0:0:128
```
*(L'ordre est : soundId : variante : priority : volume : pan : transpose : detune : speed. Ici, on force une priorité de 90 et un volume de 127).*

Si vous ne voulez pas passer par l'utilitaire en ligne de commande, vous verrez dans le **Chapitre 9** comment utiliser les outils graphiques, et notamment le **Packer**.

---

## 7.2. Configuration Joueur (Le fichier `.imc`)

Le charme d'un jeu rétro, c'est aussi de laisser le joueur bidouiller ses paramètres audio. Le plugin iMWrap gère cela via un fichier de configuration externe.

Ce fichier doit s'appeler **[NomDeVotreExe].imc** (par exemple `MonJeu.imc`) et doit être placé dans le même dossier que l'exécutable du jeu.

### Structure du fichier `.imc` (Format INI)
```ini
[MIDI]
Driver=2
Device=loopMIDI Port
```

**Explication des paramètres :**
- `Driver=0` : Le joueur veut utiliser FluidSynth (Synthé logiciel General MIDI).
- `Driver=1` : Le joueur veut l'émulation FM AdLib (bip-boup rétro).
- `Driver=2` : Hardware General MIDI (utiliser le vrai synthé matériel branché sur l'ordi).
- `Driver=3` : Hardware Roland MT-32 (vrai synthétiseur vintage).
- `Driver=4` : Émulateur MUNT (logiciel MT-32).
- `Device` : Nom du port matériel Windows (nécessaire pour Drivers 2 et 3), ou chemin vers les ROMs séparées par un `|` (pour Driver 4).

### Prise en compte dans le script AGS
Dans votre fonction `game_start()` (voir Chapitre 2), remplacez le code basique par celui-ci pour autoriser le joueur à "surcharger" vos choix :

```c
function game_start() {
    // Si le joueur a placé un fichier MonJeu.imc
    if (iMWrap_HasExternalConfig()) {
        // Tente d'appliquer ses paramètres. S'il a choisi FluidSynth (0), 
        // on lui force notre propre SoundFont par défaut en paramètre de secours.
        iMWrap_ApplyExternalConfig("$DATA$/music_data/SGM-V2.01.sf2");
    } else {
        // Sinon, aucun fichier .imc trouvé, on configure le jeu 
        // en General MIDI moderne par défaut.
        iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, "$DATA$/music_data/SGM-V2.01.sf2");
    }

    iMWrap_LoadBank("$DATA$/music_data/game.ims");
}
```

---

## 7.3. L'utilitaire graphique `SetMIDI.exe`

Plutôt que d'obliger vos joueurs à écrire un fichier texte `.imc` à la main, vous pouvez distribuer le petit utilitaire `SetMIDI.exe` fourni avec iMWrap.

Mettez `SetMIDI.exe` dans le même dossier que votre jeu. Quand le joueur double-clique dessus, une interface graphique native s'ouvre (en français, espagnol ou anglais, en fonction de la langue du système). 
L'outil scannera le dossier, trouvera l'exécutable de votre jeu, listera les vrais ports MIDI branchés sur l'ordinateur, et génèrera tout seul le fichier `.imc` correspondant au choix du joueur !

---

**Félicitations !** Vous êtes presque arrivé au bout du manuel iMWrap v6. Du chargement d'un plugin AGS jusqu'aux arcanes du SysEx hexadécimal, la musique interactive façon iMUSE n'a plus aucun secret pour vous. Les **deux chapitres suivants** sont des manuels de référence pour les **fonctions du plugin AGS iMWrap** ainsi que pour **l'utilisations des outils graphiques**.

Bon développement et bonne composition !
