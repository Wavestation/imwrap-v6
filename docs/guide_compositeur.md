# Guide du Compositeur iMWrap
## Intégration et Utilisation des Messages SysEx dans MOTU Digital Performer

Ce guide est destiné aux compositeurs et concepteurs sonores qui intègrent des instructions interactives **iMUSE** dans leurs séquences MIDI de jeux d'aventure (comme dans les moteurs de jeux d'aventure utilisant la librairie `imwrap-v6` ou ScummVM). Il détaille la structure des messages SysEx iMUSE, les principales commandes, et explique comment utiliser l'outil **Générateur de SysEx iMWrap** en le reliant à **MOTU Digital Performer (DP)** via **loopMIDI**.

---

## 1. Comprendre la Structure des Messages SysEx iMUSE

Tous les messages iMUSE sont transmis sous la forme de messages système exclusifs (SysEx) standard MIDI.

### Format général d'un message iMUSE :
```text
F0 7D [Code_Commande] [ID_Partie_Logique] [Données_en_Nibbles...] F7
```

- **`F0`** : Indicateur de début de message exclusif.
- **`7D`** : ID Manufacturier. iMUSE utilise l'ID standard `0x7D` (réservé aux projets éducatifs et de recherche) comme identifiant.
- **`Code_Commande`** : Spécifie l'action à réaliser (ex: `0x00` pour Allocate Part).
- **`ID_Partie_Logique`** : Identifie la partie (le canal virtuel/track de la séquence) ciblée par l'instruction. Les valeurs vont de `0x00` à `0x0F` (canaux 1 à 16).
- **`Données_en_Nibbles`** : Les paramètres associés à la commande. iMUSE sépare chaque octet de données (8 bits) en **deux demi-octets (nibles de 4 bits)** transmis consécutivement (poids fort puis poids faible), chaque nibble occupant les 4 bits inférieurs d'un octet MIDI valide.
  *Exemple* : Pour envoyer la valeur `0x5A` (90 en décimal), on transmettra deux octets : `0x05` suivi de `0x0A`.
- **`F7`** : Indicateur de fin de message exclusif (EOX).

---

## 2. Tableau des Commandes iMUSE les plus Courantes

### A. Allocate Part (`0x00`)
Cette commande configure et active une partie virtuelle (canal iMUSE). Elle initialise l'instrument, la priorité relative, le volume, la panoramique, la transposition et le comportement du pitch bend.
*Format d'encodage des nibbles de paramètres (16 nibbles / 8 octets décodés)* :
1. **Octet 0** : Bit 0 = Partie active (1=Oui, 0=Non) | Bit 1 = Reverb active (1=Oui, 0=Non).
2. **Octet 1** : Priorité de la partie (0 à 255).
3. **Octet 2** : Volume de la partie (0 à 127).
4. **Octet 3** : Panoramique (0 à 127, centré à 64).
5. **Octet 4** : Bit 7 = Percussion (1=Oui, 0=Non) | Bits 0-6 = Transposition (-127 à +127, encodée en complément à deux).
6. **Octet 5** : Désaccordage (Detune, -128 à 127).
7. **Octet 6** : Facteur de Pitch Bend (1 à 12 demi-tons).
8. **Octet 7** : Numéro de programme (Instrument de base, 0 à 127).

> [!NOTE]
> Notre moteur révisé applique dynamiquement toutes ces valeurs (volume, pan, etc.) et recharge le timbre personnalisé Roland MT-32 associé à la partie, même si celle-ci est déjà en cours de lecture.

### B. Shutdown Part (`0x01`)
Désactive et réinitialise une partie virtuelle. Elle éteint toutes les notes actives sur le canal physique associé.
```text
F0 7D 01 [ID_Partie] F7
```

### C. Parameter Adjust (`0x21`)
Modifie dynamiquement les paramètres iMUSE internes ou les contrôles MIDI standard pour une partie en cours de lecture.

### D. Hook Jump (`0x30`)
Permet d'insérer un saut conditionnel (Smart Jump) dans la séquence de lecture MIDI. Le moteur de jeu peut déclencher cette transition à la fin d'une mesure ou d'un battement pour synchroniser la musique avec les actions du joueur.
```text
F0 7D 30 [ID_Partie] [Nibbles de Commande, Piste, Mesure et Tick cible] F7
```

### E. Marker (`0x40`)
Envoie un marqueur textuel au moteur de script du jeu pour lui notifier qu'un point précis de la musique a été atteint (ex: déclencher une animation ou un dialogue sur un battement précis).
```text
F0 7D 40 [ID_Partie] [Texte du Marqueur en ASCII] F7
```

### F. Set Loop (`0x50`) & Clear Loop (`0x51`)
Définit ou annule une boucle de lecture dynamique sur une section de la séquence (indiquée en mesures et ticks).

---

## 3. Configuration et Routage via loopMIDI

Pour simplifier la composition et tester en temps réel vos commandes SysEx directement dans votre projet de musique, vous pouvez relier l'outil **Générateur de SysEx iMWrap** à **Digital Performer** en passant par des câbles MIDI virtuels.

### Étape 1 : Créer le port virtuel dans loopMIDI
1. Téléchargez et lancez le logiciel gratuit **loopMIDI** (de Tobias Erichsen).
2. Dans le panneau de configuration, cliquez sur le bouton `+` pour ajouter un nouveau port.
3. Nommez-le par exemple : `iMWrap Generator`.
4. loopMIDI tourne en arrière-plan et crée un port d'entrée et de sortie MIDI virtuel visible par toutes vos applications.

### Étape 2 : Connecter le Générateur de SysEx
1. Lancez le Générateur de SysEx (`imwrap_sysex_gui.exe`).
2. Dans la section **Envoi MIDI Direct** (à droite), repérez la liste déroulante **Périphérique**.
3. Sélectionnez le port virtuel créé : `iMWrap Generator`.
4. Désormais, chaque clic sur le bouton **Envoyer le SysEx** transmettra la trame MIDI sur ce canal virtuel.

### Étape 3 : Configurer Digital Performer pour l'enregistrement en temps réel
Si vous préférez enregistrer en temps réel vos manipulations ou vos commandes iMUSE dans Digital Performer :
1. Ouvrez Digital Performer.
2. Allez dans **Setup > Input Filter...** et assurez-vous que la case **System Exclusive** est bien cochée (pour autoriser DP à enregistrer les SysEx).
3. Dans votre projet DP, créez une nouvelle piste MIDI.
4. Réglez l'entrée MIDI de cette piste sur `iMWrap Generator`.
5. Armez la piste MIDI en enregistrement.
6. Lancez l'enregistrement dans DP, puis cliquez sur **Envoyer le SysEx** dans le Générateur.
7. DP enregistre précisément le message SysEx généré sous la forme d'un événement dans la piste !

---

## 4. Insérer manuellement des SysEx iMUSE dans Digital Performer

Si vous souhaitez saisir ou copier-coller manuellement les messages hexadécimaux iMUSE dans vos pistes MIDI existantes :

1. Dans le Générateur de SysEx, configurez vos curseurs pour obtenir le message désiré.
2. Cliquez sur le bouton **Copier dans le presse-papiers**.
3. Dans Digital Performer, ouvrez l'**Event List** (Liste d'Événements) de la piste MIDI ciblée.
4. Placez votre curseur de lecture à la position temporelle exacte (Mesure|Temps|Tick) où l'instruction doit s'exécuter.
5. Insérez un nouvel événement de type **System Exclusive (SysEx)** :
   - Dans DP, utilisez le raccourci d'insertion ou sélectionnez `SysEx` dans le menu d'ajout d'événements.
6. Une boîte de dialogue d'édition s'ouvre. Double-cliquez sur l'événement SysEx créé pour ouvrir l'éditeur de texte hexadécimal.
7. **Collez** le texte hexadécimal copié depuis le Générateur (il est déjà pré-formaté avec les balises `F0` et `F7`).
8. Validez. Le message fait désormais partie intégrante de votre séquence MIDI et sera exécuté à la microseconde près par le moteur iMUSE.

> [!TIP]
> **Recommandation pour le Roland MT-32** :
> Lorsque vous utilisez des timbres personnalisés Roland, veillez à ce que les messages SysEx Roland (les dumps de timbres) soient positionnés au moins **50 ticks** avant la commande `Allocate Part` ou les premières notes de musique. Cela donne le temps physique au MT-32 (ou à l'émulateur Munt) d'enregistrer le patch en mémoire et d'éviter les notes tronquées ou jouées avec un mauvais instrument.
