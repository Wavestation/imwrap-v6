# Chapitre 1 : Introduction à iMWrap et historique d'iMUSE

Bienvenue dans le manuel complet de **iMWrap v6 (version actuelle : 1.0.4)**. Si vous êtes ici, c'est que vous avez probablement une ambition : que la musique de votre jeu réagisse au doigt et à l'œil du joueur, de la même manière que la bande originale d'un film s'adapte à ce qui se passe à l'écran. 

Dans ce chapitre, nous allons démystifier les concepts de base, comprendre d'où vient cette technologie, et voir comment elle s'intègre au moteur d'Adventure Game Studio (AGS).

---

## 1.1. L'héritage d'iMUSE

Au début des années 90, les compositeurs Michael Land et Peter McConnell ont créé pour LucasArts un système révolutionnaire appelé **iMUSE** (Interactive Music Streaming Engine). Sa première apparition magistrale s'est faite dans *Monkey Island 2: LeChuck's Revenge*.

À l'époque, la musique dans les jeux vidéo était souvent une simple boucle qui tournait en arrière-plan. Avec iMUSE, la musique devenait "intelligente" :
* En passant d'une pièce à l'autre (par exemple, entrer dans un bâtiment), la musique changeait d'instrumentation ou de mélodie en douceur, sans jamais perdre le rythme.
* Des événements du jeu (comme une blague ou une action soudaine) pouvaient déclencher des accents musicaux (des *stingers* ou des coups de cymbale) qui tombaient toujours parfaitement sur le temps fort de la mesure.

**iMWrap v6** est l'héritier direct de ce système. Il recrée la logique des sauts interactifs, des transitions fluides et de la synchronisation au rythme (les fameux *Hooks* et *Markers*), tout en les mettant à disposition des développeurs utilisant AGS aujourd'hui.

---

## 1.2. Qu'est-ce que iMWrap v6 ?

iMWrap n'est pas un simple "lecteur MP3" ou "lecteur OGG". C'est un **séquenceur MIDI interactif**. 

Contrairement à un fichier audio (MP3, WAV) qui est figé, un fichier MIDI est une partition numérique. Il dit au système : *"Joue un Do majeur avec un son de piano pendant 1 seconde"*. 
Parce que la musique n'est pas figée, **le moteur peut la manipuler en temps réel** :
- Il peut couper le son de la batterie à la volée.
- Il peut attendre la fin de la mesure pour sauter à un autre passage.
- Il peut envoyer un signal au jeu à l'instant précis où une note est jouée.

Pour fonctionner de manière moderne sans dépendre des vielles cartes son des années 90, le plugin `agsimwrap` (que vous allez utiliser) embarque plusieurs technologies :

1. **Le Core iMWrap** : C'est le cerveau. Il lit la partition, calcule le tempo, interprète les commandes d'interactivité (boucles, sauts) et décide des notes à jouer.
2. **Le Synthétiseur** : C'est l'orchestre. iMWrap intègre **FluidSynth**, un synthétiseur logiciel très performant qui utilise des *SoundFonts* (fichiers `.sf2` contenant des échantillons audio de vrais instruments) pour produire le son final. Il supporte aussi l'émulation FM (AdLib) et les cartes matérielles (Roland MT-32).
3. **Le Moteur Audio (miniaudio)** : C'est le haut-parleur. Il s'assure que le son est généré en tâche de fond de l'ordinateur, de manière fluide, sans être interrompu par les chargements ou les baisses de *framerate* d'AGS.

> [!TIP]
> Pourquoi v6 dans le nom, alors que la version officielle est en 1.0.4 ? En fait ce v6 est un clin d'œil à la v6 de SCUMM, celle qui a vu iMuse s'y stabiliser. Une sorte d'hommage en somme...

---

## 1.3. Les différents profils sonores

Selon l'esthétique que vous souhaitez donner à votre jeu, iMWrap gère plusieurs profils MIDI. Il est crucial de les comprendre, car cela influencera la manière dont vous préparez votre musique :

### General MIDI (GM) (FluidSynth ou Matériel)
C'est le standard actuel. Le General MIDI garantit que l'instrument numéro 1 est un piano, le 25 une guitare, etc. En fournissant une *SoundFont* (.sf2) avec votre jeu, vous garantissez via le pilote FluidSynth que tous les joueurs entendront exactement la même chose. Mais iMWrap permet aussi d'envoyer le flux General MIDI vers un vrai synthétiseur matériel (tel un Roland SC-55 ou un Yamaha MU128 pour les plus connus) branché à l'ordinateur !

### Roland MT-32 (Matériel ou Émulé)
Le Roland MT-32 était la Rolls-Royce du son PC à la fin des années 80. Contrairement au GM, il permet de sculpter des sons personnalisés (Timbres) par synthèse soustractive. iMWrap sait envoyer les données "SysEx" spécifiques au MT-32 pour recréer fidèlement l'ambiance des jeux d'époque. Si le joueur ne dispose pas d'un véritable MT-32, il peut utiliser l'émulation open-source proposée par le projet MUNT. 

### AdLib / OPL
La célèbre carte son AdLib (et SoundBlaster) générait du son par "Synthèse FM". C'est le son "bip-boup" métallique et nostalgique caractéristique du début des années 90. iMWrap possède un pilote AdLib natif pour ce rendu très spécifique. *(Note : Dans la version 1.0.4 actuelle, le support AdLib est inclus expérimentalement mais n'est pas encore vérifié à 100%).*

---

## 1.4. Architecture globale du flux

Pour résumer, voici ce qui se passe sous le capot de votre jeu AGS :

1. **AGS (Le chef d'orchestre global)** dit : *"Je charge la room du cimetière, joue la musique 80 !"*
2. **iMWrap Core** lit le fichier musical `.ims` (le fichier compilé contenant tous les fichiers musicaux du jeu).
3. **iMWrap** rencontre un événement dans la musique : *"À la mesure 4, dis à AGS d'afficher un éclair."* -> Envoi d'un **Marker** vers AGS.
4. **iMWrap** envoie les notes MIDI à **FluidSynth**.
5. **FluidSynth** lit les sons dans la SoundFont et génère le signal audio.
6. Le flux audio sort par les haut-parleurs via **miniaudio**.

Maintenant que le cadre est posé, passons au **Chapitre 2** pour installer tout cela dans votre projet AGS !
