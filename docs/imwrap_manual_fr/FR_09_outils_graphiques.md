# Chapitre 9 : Les Outils Graphiques (GUI)

Travailler avec de la musique interactive implique de manipuler des données techniques : des paramètres de mixage, de la logique binaire et des SysEx hexadécimaux. 
Le projet **iMWrap v6** fournit donc trois outils graphiques (codées en Qt) pour vous permettre de travailler visuellement : le **SysEx Generator**, le **Packer**, et l'**Explorer**.

Voici la "substantifique moelle" et le mode d'emploi de ces outils, basés sur leur code et leur utilité profonde dans le workflow de composition.

---

## 9.1. SysEx Generator : le couteau-suisse du compositeur

<img width="802" height="801" alt="image" src="https://github.com/user-attachments/assets/8e27fbbb-bea2-44b0-8440-aa112e59ab7d" />

Oubliez les convertisseurs hexadécimaux sur internet, ou même votre bonne vieille TI-85. L'application **SysEx Generator** est conçue pour être ouverte à côté de votre séquenceur (Cubase, Reaper, Logic, Digital Performer) pendant que vous composez.

Grâce à un outil tel que (LoopMIDI)[https://www.tobias-erichsen.de/software/loopmidi.html] de Tobias Erichsen, vous pourrez établir un câble MIDI virtuel entre le SysEx Generator et votre DAW, et y enregistrer les messages SysEx comme s'il s'agissait de n'importe quel autre événement MIDI.

### Onglet "Generator" (Création de SysEx)
C'est ici que vous fabriquez les messages interactifs à coller dans votre DAW.
1. **Liste déroulante des types** : Sélectionnez ce que vous voulez générer (*Allocate Part*, *Hook Jump*, *Marker*, *Set Loop*, etc.).
2. **Le formulaire dynamique** : Selon votre choix, l'interface affiche des champs précis :
   - Pour *Allocate Part* : Des cases à cocher `Part On`, `Reverb`, `Percussion`, et des curseurs rotatifs (spinboxes) pour le `Volume`, le `Pan` (qui s'affiche ici de manière claire, de -64 à +63), le `Detune` et le `Program` MIDI. 
   - Pour *Hook* : Les champs pour cibler la *Track*, le *Beat* (mesure cible), le *Tick*, et la valeur *Relative* du hook.
   - Pour *Set Loop* : Les 4 champs vitaux : *Loop To Beat*, *To Tick*, *From Beat*, *From Tick*, et le *Count* (nombre de boucles).
3. **Le panneau AdLib** : Si vous choisissez d'envoyer des paramètres AdLib natifs (`0x10`), l'outil débloque un éditeur Hex complet et permet même l'**Import / Export de fichiers SBI** (le vieux format SoundBlaster Instrument) !
4. **Envoi Direct (Direct Send)** : La fonctionnalité magique. L'outil peut se connecter directement à la sortie MIDI de votre PC (`sendMidiSysEx`)., ou Vous pourrez donc envoyer directement le SysEc à votre DAW, préalablement mis en mode enregistrement, via un câble MIDI virtuel, comme expliqué ci-dessus.
5. **Génération Hexadécimale** : Au bas de la fenêtre, le logiciel mouline toutes vos données, gère l'encodage en "nibbles", et recrache la ligne exacte (ex: `F0 7D 00 01 05 0A... F7`). Cliquez sur **Copier** et collez-la sur la grille de votre DAW !
### Onglet "Decoder"
L'inverse exact : copiez-collez une bouillie hexadécimale que vous avez trouvée dans un vieux fichier MIDI iMUSE, et l'outil vous décrira en texte clair ce qu'elle fait ("*Cette ligne alloue le canal 2 avec un volume de 127*").

---

## 9.2. Packer : L'atelier de création de la Banque (.ims)

<img width="1002" height="782" alt="image" src="https://github.com/user-attachments/assets/b4137e92-a762-4989-97c5-de6afc16f7bb" />


> [!TIP]
> **Le Packer existe aussi sous forme de plugin pour l'éditeur AGS !**
> C'est la méthode de travail la plus souple : le plugin rajoute un nœud directement dans l'arborescence de votre projet AGS, un panneau d'édition dédié (intégré à votre espace de travail) et un menu "iMWrap" dans la barre supérieure de l'éditeur AGS. Vous pouvez ainsi manipuler et voir le contenu de vos banques audio directement sans jamais quitter AGS !
> <img width="1325" height="816" alt="image" src="https://github.com/user-attachments/assets/7eb7139a-3377-443e-a0ee-fe70d8124d7b" />

L'outil en ligne de commande `imwrappack` est très puissant, mais gérer ses gros fichiers `.mid` dans un script `.bat` est fatiguant. Le **Packer GUI** remplace la ligne de commande par une interface visuelle où vous construisez l'architecture de votre bande-son.

### Comment l'utiliser ?
1. **Liste des Sons (Sound List)** : À gauche, vous ajoutez vos morceaux. Par exemple, cliquez sur *Add Sound*, attribuez-lui l'ID `80`, et le nom `Taverne`.
2. **Gestion des Variantes (Variant Box)** : Un même son (ID 80) peut contenir deux musiques physiques (le fichier pour General MIDI, et celui pour MT-32). Sélectionnez `GMD` dans la liste déroulante et cliquez sur **Import MIDI** pour chercher `taverne_gm.mid`.
3. **Le tableau des Pistes (Tracks Table)** : En important un `.mid`, l'outil analyse le fichier et affiche toutes les pistes (Tracks) qu'il contient. Vous pouvez les supprimer, ou les réorganiser (Boutons *Move Up* / *Move Down*) si votre DAW a exporté les pistes dans un mauvais ordre.
4. **L'en-tête MDhd dynamique** : C'est la grande force du Packer. Au lieu d'écrire du code AGS comme `iMWrap_SetSoundVolume(80, 50)`, vous pouvez cocher `Include MDHD` dans le Packer et régler la **Priorité**, le **Volume**, le **Pan**, la **Transposition** et la **Vitesse par défaut** *directement dans le fichier de la banque*. L'interface fournit des "SpinBox" pour chaque paramètre.
5. **Sauvegarde (Save)** : Enregistrez le tout (Fichier `->` Save Project). L'outil enregistre **directement** au format de banque final `.ims` ! Vous n'avez pas de fichier projet intermédiaire à gérer.
6. Le fichier sauvegardé est robuste et prêt à être glissé dans le dossier d'AGS. Si vous le rouvrez plus tard dans le Packer ou l'Explorer, toute votre architecture (sons, variantes, MDhd) sera restaurée.

Notez que l'ajout de fichier MIDI prend en charge **tous les formats de SMF**, avec les comportements suivants selon les cas : 
* SMF Format 0 : Le fichier est directement rajouté comme **Track** du fichier SMF 2 incorporé dans la **Song**.
* SMF Format 1 : Toutes les pistes du fichier sont fusionnées, et le tout est importé comme **Track** du fichier SMF 2 incorporé dans la **Song**.
* SMF Format 2 : Toutes les pistes du fichier sont ajoutées individuellement comme **Tracks** du fichier SMF 2 incorporé dans la **Song**.

---

## 9.3. Explorer : L'outil d'analyse et d'édition détaillée des projets .ims

<img width="1402" height="1008" alt="image" src="https://github.com/user-attachments/assets/5e258643-6046-4571-8160-f4a169b96eed" />

L'**Explorer** est l'outil ultime. Il ne sert pas qu'à regarder dans les fichiers `.ims`, il permet de les écouter et même de les modifier chirurgicalement *après* leur compilation.

### Audit et Visualisation
- **Arbre de contenu (Tree Widget)** : À gauche, vous naviguez dans l'arborescence : `Son (ID) -> Variante (GMD/ROL) -> Piste (Track)`.
- **Détails Hexadécimaux** : Cliquez sur n'importe quel événement pour voir sa structure binaire décortiquée (Panneau `DetailsEdit_`).
- **Tableau des Événements** : En cliquant sur une piste, un énorme tableau central liste l'intégralité des notes, Control Changes, et SysEx de la piste chronologiquement !

### Le Lecteur Intégré (Preview)
Pas besoin d'AGS pour écouter votre banque ! L'Explorer intègre le **Core iMWrap** entier. 
- Choisissez votre *Device* et votre *Profile* (WinMM, Émulation Adlib, mode strict SNM).
- Vous avez des boutons *Play*, *Stop*, et un timer temporel (*Playback Position Label*) qui défile (ex: `M:1 B:2 T:120`) pour voir le temps s'écouler en direct. Vous pouvez même tester le comportement en boucle (Loop Check).

### Remastering et Modification à la volée
L'intention derrière l'Explorer est de pouvoir "patcher" une erreur d'authoring sans avoir à relancer son séquenceur (DAW) et recompiler tout le projet :
- **Add iMWrap Command / Edit Selected Event** : Vous pouvez cliquer sur un événement MIDI dans la grille (par exemple, le premier temps de la mesure 10) et injecter directement un Hook ou un Marker au milieu de la piste ! 
- L'interface vous fournit des menus déroulants pour choisir le hook et valide l'injection de l'événement dans la structure de données.
- Il gère mathématiquement la conversion entre l'affichage humain `Mesure:Temps:Tick` (MBT) et les horloges internes, pour que vous sachiez exactement où vous placez votre événement.

Une fois vos retouches faites sur les événements ou les pistes (vous pouvez aussi *Importer/Supprimer* des pistes ici), faites "Save" et votre fichier `.ims` est mis à jour sur le disque, corrigé et prêt.

Notez que ces outils peuvent être améliorés au fur et à mesure de l'avancée du développement du système iMWrap !