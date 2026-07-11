# Chapitre 9 : Les Outils Graphiques (GUI)

Travailler avec de la musique interactive implique de manipuler des donnees
techniques : parametres de mixage, logique binaire, et SysEx hexadecimaux.
Le projet **iMWrap v6** fournit donc trois outils graphiques Qt :
le **SysEx Generator**, le **Packer**, et l'**Explorer**.

---

## 9.1. SysEx Generator : le couteau suisse du compositeur

<img width="802" height="801" alt="image" src="https://github.com/user-attachments/assets/8e27fbbb-bea2-44b0-8440-aa112e59ab7d" />

Le **SysEx Generator** est pense pour rester ouvert a cote de votre sequenceur
pendant la composition.

Grace a un outil comme [LoopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html),
vous pouvez etablir un cable MIDI virtuel entre le generateur et votre DAW, puis
y enregistrer les SysEx comme de simples evenements MIDI.

### Onglet "Generator"

1. Choisissez le type de message a generer : *Allocate Part*, *Hook Jump*,
   *Marker*, *Set Loop*, etc.
2. Le formulaire change selon le type choisi.
3. Les messages AdLib natifs debloquent un panneau specialise avec import/export
   de fichiers SBI.
4. L'outil peut envoyer le SysEx directement a une sortie MIDI.
5. Le resultat hexadecimal final peut etre copie et colle dans votre DAW.

### Onglet "Decoder"

L'onglet inverse l'operation : on colle un SysEx iMUSE existant, et l'outil le
decrit en clair.

---

## 9.2. Packer : l'atelier de creation de la banque `.ims`

<img width="1002" height="782" alt="image" src="https://github.com/user-attachments/assets/b4137e92-a762-4989-97c5-de6afc16f7bb" />

> [!TIP]
> **Le Packer existe aussi comme plugin pour l'editeur AGS.**
> Cela permet de manipuler directement vos banques audio depuis l'environnement
> AGS.

Le `imwrappack` maintenu couvre maintenant une grande partie du meme workflow
d'edition de banque, mais le **Packer GUI** reste le compagnon visuel le plus
confortable pour construire l'architecture de votre bande-son.

### Comment l'utiliser ?

1. **Liste des sons** : a gauche, ajoutez vos morceaux, leur ID, et leur nom.
2. **Gestion des variantes** : un meme son peut contenir plusieurs musiques
   physiques, par exemple `GMD`, `ROL` et `ADL`.
3. **Tableau des pistes** : lors de l'import d'un `.mid`, l'outil analyse le
   fichier et affiche les pistes. Vous pouvez les supprimer ou les reordonner.
4. **En-tete MDhd** : vous pouvez regler la priorite, le volume, le pan, la
   transposition, le detune et la vitesse directement dans la banque.
5. **Sauvegarde** : le Packer enregistre directement au format final `.ims`.
6. Si vous rouvrez ensuite la banque dans le Packer ou l'Explorer, sa structure
   est restauree.

L'ajout de fichier MIDI prend en charge tous les formats SMF :

- SMF 0 : importe comme une piste
- SMF 1 : fusionne toutes les pistes du fichier en une seule piste importee
- SMF 2 : importe chaque piste source comme une piste IMS

---

## 9.3. Explorer : l'outil d'analyse et d'edition detaillee

<img width="1402" height="1008" alt="image" src="https://github.com/user-attachments/assets/5e258643-6046-4571-8160-f4a169b96eed" />

L'**Explorer** ne sert pas seulement a regarder dans les fichiers `.ims` :
il permet aussi de les ecouter et de les modifier apres compilation.

### Audit et visualisation

- **Arbre de contenu** : `Son (ID) -> Variante (GMD/ROL/ADL) -> Piste`
- **Details hexadecimaux** : affichage decode d'un evenement selectionne
- **Tableau des evenements** : liste chronologique des notes, CC et SysEx

### Lecteur integre

L'Explorer embarque le core iMWrap :

- choix du *Device* et du *Profile*
- boutons *Play*, *Stop*, *Stop All*
- position de lecture temps reel
- verification des boucles

### Retouches et remastering

L'Explorer permet de corriger une banque sans repasser par le DAW :

- injection de Hook ou de Marker
- edition d'un evenement iMWrap reconnu
- conversion entre affichage `Mesure:Temps:Tick` et horloges internes
- import, suppression et reordonnancement de pistes

Une fois les retouches terminees, un simple "Save" reecrit la banque `.ims`
sur le disque.
