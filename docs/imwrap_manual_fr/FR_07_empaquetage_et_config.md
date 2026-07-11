# Chapitre 7 : Empaquetage et Deploiement

Vous avez programme vos interactions dans AGS, et vos fichiers `.mid`
regorgent de SysEx soigneusement prepares. Il est temps d'empaqueter tout cela
pour les joueurs.

Dans ce dernier chapitre, nous verrons comment fusionner vos fichiers avec
`imwrappack`, et comment laisser le joueur choisir sa carte son preferee grace
au fichier de configuration `.imc`.

---

## 7.1. L'utilitaire `imwrappack`

Le moteur iMWrap ne lit pas directement des fichiers `.mid`. Il lit des banques
`.ims` (iMWrap Music Set). Une banque IMS peut regrouper plusieurs variantes du
meme morceau.

Pourquoi faire cela ? Si vous composez une "Musique 80" pour Roland MT-32 et
une autre pour General MIDI, le jeu AGS peut simplement appeler
`iMWrap_StartSound(80)`. Le moteur choisira ensuite la bonne variante selon la
carte son ou le profil de sortie actif.

### Utiliser imwrappack

L'outil `imwrappack` s'utilise en ligne de commande.

Le CLI actuel sait a la fois construire une banque depuis zero et editer une
banque `.ims` existante.

**Syntaxe de build de base :**

```bash
imwrappack build output.ims \
  --name=80:Foret \
  --mdhd=80:gmd:90:127:0:0:0:128 \
  80:gmd=80_foret_generalmidi.mid \
  80:rol=80_foret_mt32.mid \
  80:adl=80_foret_adlib.mid \
  81:gmd=81_interieur.mid
```

Dans cet exemple :

1. On cree le fichier `output.ims`.
2. `--name=80:Foret` donne un nom interne informatif au morceau 80.
3. `80:gmd=...` integre le `.mid` comme variante **General MIDI** (`gmd`).
4. `80:rol=...` integre une variante **Roland MT-32** (`rol`).
5. `80:adl=...` integre une variante **AdLib** (`adl`).
6. `81:gmd=...` ajoute la musique 81 en variante GM seule.

Les variantes supportees sont `gmd`, `rol` et `adl`.

Contrairement a l'ancien CLI v1, le packer maintenu accepte **SMF 0, 1 et 2** :

- SMF 0 : importe comme une piste
- SMF 1 : fusionne en une seule piste type format 0
- SMF 2 : importe chaque piste source comme une piste IMS distincte

### Configurer Les Priorites Et Volumes (`MDhd`)

Vous pouvez forcer les parametres par defaut d'une variante (priorite, volume,
vitesse, etc.) directement a l'empaquetage sans editer le MIDI, en injectant un
chunk `MDhd` :

```bash
--mdhd=80:gmd:90:127:0:0:0:128
```

L'ordre est :

```text
soundId : variante : priority : volume : pan : transpose : detune : speed
```

Le meme CLI sait aussi inspecter et editer une banque existante :

```bash
imwrappack inspect output.ims
imwrappack import-midi output.ims 80 gmd remplacement.mid
imwrappack move-track output.ims 80 gmd 1 up
imwrappack export-track output.ims 80 gmd 0 foret_track0.mid
```

Si vous preferez un workflow visuel, le **Chapitre 9** couvre les outils
graphiques, et notamment le **Packer**.

---

## 7.2. Configuration Joueur (Le Fichier `.imc`)

Le charme d'un jeu retro, c'est aussi de laisser le joueur regler ses
parametres audio. Le plugin iMWrap gere cela via un fichier de configuration
externe.

Ce fichier doit s'appeler **[NomDeVotreExe].imc** (par exemple `MonJeu.imc`) et
doit etre place dans le meme dossier que l'executable du jeu.

### Structure Du Fichier `.imc` (Format INI)

```ini
[MIDI]
Driver=2
Device=loopMIDI Port
```

Explication des parametres :

- `Driver=0` : le joueur veut utiliser FluidSynth
- `Driver=1` : le joueur veut l'emulation FM AdLib
- `Driver=2` : le joueur veut du General MIDI materiel
- `Driver=3` : le joueur veut du Roland MT-32 materiel
- `Device` : nom du port MIDI Windows, utile pour les drivers 2 et 3

### Prise En Compte Dans Le Script AGS

Dans votre fonction `game_start()` (voir le chapitre 2), remplacez le code
basique par celui-ci pour permettre au joueur de surcharger vos choix :

```c
function game_start() {
    if (iMWrap_HasExternalConfig()) {
        iMWrap_ApplyExternalConfig("$DATA$/music_data/SGM-V2.01.sf2");
    } else {
        iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, "$DATA$/music_data/SGM-V2.01.sf2");
    }

    iMWrap_LoadBank("$DATA$/music_data/game.ims");
}
```

---

## 7.3. L'utilitaire Graphique `SetMIDI.exe`

Plutot que de demander au joueur d'ecrire manuellement un fichier `.imc`, vous
pouvez distribuer le petit utilitaire `SetMIDI.exe` fourni avec iMWrap.

Placez `SetMIDI.exe` dans le meme dossier que votre jeu. Quand le joueur le
lance, l'interface detecte l'executable du jeu, liste les ports MIDI presents
sur la machine, puis genere automatiquement le fichier `.imc` correspondant a
son choix.

---

Vous avez presque termine le manuel iMWrap v6. Les chapitres suivants servent de
references pour les fonctions AGS et pour l'usage des outils graphiques.
