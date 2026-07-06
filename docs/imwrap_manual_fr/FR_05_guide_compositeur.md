# Chapitre 5 : Le Guide du Compositeur iMWrap

Bienvenue de l'autre côté de l'écran. Si vous êtes le compositeur, c'est vous qui avez le pouvoir de rendre la musique véritablement interactive. Le programmeur AGS ne pourra déclencher de magie que si vous avez préparé le terrain.

Dans iMWrap (comme dans l'iMUSE d'origine), l'interactivité se code en insérant des événements **SysEx (System Exclusive)** dans votre séquenceur habituel (Cubase, Reaper, Logic, Digital Performer, etc.). Ne vous inquiétez pas, vous n'aurez pas l'obligation absolue d'entrer ces messages à la main octet par octet (c'est ce que j'ai pourtant fait au tout début du développement). En effet, j'ai écrit un outil, dont il sera question au **Chapitre 9**, qui permet de générer les messages directement, en y rentrant les paramètres nécessaires.

---

## 5.1. Réfléchir et échafauder la musique du jeu en se focalisant sur l'interactivité

Contrairement à un morceau linéaire (couplet, refrain, fin), une musique de jeu d'aventure interactive doit être pensée en "blocs" ou en "strates".

- **Les Stems (Strates / Canaux)** : Pensez votre musique par groupes d'instruments séparés sur des canaux MIDI distincts. Le programmeur, ou vous-même via des hooks, pourra allumer et éteindre ces canaux (ex: n'activer les percussions que si un ennemi est proche).
- **Les Blocs** : Préparez de longues pistes qui contiennent différentes variations d'un même thème, alignées sur la grille rythmique (ex: Thème calme de la mesure 1 à 20, Thème d'action de la mesure 21 à 40). Vous utiliserez des boucles et des sauts pour naviguer entre ces blocs. Vous pourrez aussi utiliser la notion de "Tracks" pour mettre dans la même "Song" plusieurs transition et/ou variations qui seront lues séquentiellement, grâce au pouvoir du format de fichier **MIDI 2**.

### Le Workflow de Piste de Contrôle
Pour garder votre session DAW (Digital Audio Workstation) claire, **réservez toujours une piste MIDI distincte (souvent la première ou la dernière) pour tous vos événements SysEx d'interactivité**. N'y mettez aucune note musicale. 
Gardez vos autres pistes (1, 2, 3...) purement musicales (Notes, CC, Program Changes).
Petite exception, concernant les messages de déclaration de Part (Part Allocation), il est plus clair et opportun de les positionner sur les pistes concernées.

<img width="1034" height="353" alt="image" src="https://github.com/user-attachments/assets/04e578ac-7f1d-452b-865c-1ebd5708d039" />


---

## 5.2. L'anatomie d'un SysEx iMWrap

Dans le monde MIDI, un message SysEx sert traditionnellement à envoyer des commandes spécifiques à un synthétiseur matériel. iMWrap utilise ce canal pour écouter vos instructions.

Tous les messages iMWrap suivent ce format brut (en hexadécimal) :
```text
F0 7D [CODE_COMMANDE] [PARAMÈTRES...] F7
```
- `F0` : Début obligatoire de tout SysEx MIDI.
- `7D` : L'identifiant "iMUSE/iMWrap". Le moteur ignore tous les autres SysEx (sauf spécificité MT-32).
- `CODE_COMMANDE` : Ce que vous voulez faire (Allouer, Boucler, etc.).
- `PARAMÈTRES` : Les options (sur plusieurs octets).
- `F7` : Fin obligatoire.

> [!IMPORTANT]
> **Règle d'Or : Tout est indexé à zéro !**
> Dans l'écosystème iMWrap (comme en programmation C), les valeurs commencent toujours à `0`. 
> Ainsi, la **Partie 0** ou le **Canal 0** dans vos commandes correspond au **Canal MIDI 1** dans votre séquenceur DAW. Gardez toujours ce décalage d'une unité en tête lors de la rédaction de vos SysEx et de vos scripts AGS !

---

## 5.3. Le Kit de Survie : Reset et Allocation

Pour que votre musique joue dans le jeu, **vous devez absolument utiliser deux commandes** au tout début de votre fichier MIDI (au Tick 0).

### 1. Start Song (Reset)
Au tout premier tick, envoyez ce message pour nettoyer la mémoire du moteur et réinitialiser toutes les pistes.
```text
F0 7D 02 F7
```

### 2. Allocate Part (Ouvrir un canal)
Dans iMWrap, les notes d'un canal MIDI ne seront entendues que si ce canal a été explicitement "alloué" par un SysEx. Cela permet au moteur d'affecter une priorité et des réglages fins à chaque instrument.
Le code est `00`. 
Exemple pour allouer le canal 0 (en hexadécimal) :
```text
F0 7D 00 00 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 00 F7
```
*(Nous décortiquerons la signification exacte de chaque paire de chiffres dans le **Chapitre 6**. Pour l'instant, sachez juste que vous devez copier-coller et adapter ce SysEx pour chaque canal utilisé, en changeant le 4ème octet `00` par `01`, `02`, etc.).*

> [!CAUTION]
> Si la Song contient plusieurs Tracks sur lesquelles se trouvent des séquences qui doivent être enchaînées, il ne faut pas mettre de message Song Start ailleurs que sur la première track, au risque de voir la musique brutalement se couper lors de la transition.

---

## 5.4. Ajouter l'Interactivité dans le DAW

Voici les 3 outils les plus importants pour un compositeur.

### 1. Le Marker (Déclencheur vers AGS)
Vous voulez que AGS affiche un éclair sur la grosse caisse de la mesure 4.
Allez à la mesure 4 de votre séquenceur, sur votre piste de contrôle, et insérez un SysEx de type `40` (Marker).
```text
F0 7D 40 00 01 F7
```
Cela envoie l'ID `1` au script AGS.

> [!CAUTION]
> N'écrivez jamais de texte littéral dans un Marker iMWrap (ex: `F0 7D 40 00 "INTRO" F7`). Le moteur traiterait chaque lettre comme un marqueur différent (il enverrait 'I', puis 'N', puis 'T'...). Utilisez **uniquement un identifiant numérique à un seul octet** (ex: 01, 02, 0A, 0B...).

### 2. Les Boucles (Set Loop)
Si vous voulez que la musique boucle infiniment sans l'aide du développeur AGS, utilisez le code `50`.
Par exemple, pour boucler 3 fois de la mesure 16 vers la mesure 8 :
```text
F0 7D 50 00 00 00 00 03 00 00 00 08 00 00 00 00 00 00 00 10 00 00 00 00 F7
```
*(Voir **Chapitre 6** pour la construction exacte)*.

Pour annuler la boucle en cours (Clear Loop), c'est le code `51` :
```text
F0 7D 51 F7
```

### 3. Préparer les Hooks (Crochets)
Rappelez-vous du Chapitre 4 : le programmeur AGS veut envoyer un ordre, mais il doit attendre que la musique le permette. 
Pour lui donner le "feu vert", vous (le compositeur) devez placer des SysEx de Hook (famille `3X`) sur les temps forts.

Si vous voulez autoriser AGS à changer le volume de la batterie (canal 9) au début de chaque mesure, placez ce SysEx au début de la mesure :
```text
// 33 = Hook Volume, 09 = Canal, 01 = ID du Hook (hookValue)
// (Suivi du reste des paramètres requis par le SysEx de volume)
F0 7D 33 09 01 00 F7
```
Lorsque la tête de lecture MIDI passe sur cet événement, si le programmeur avait appelé `iMWrap_SetHook(..., IMWRAP_HOOK_PART_VOLUME, 1, ...)` auparavant, l'action s'exécute enfin !

---

## 5.5. Le gabarit parfait d'un morceau

Voici à quoi devrait ressembler la structure de votre piste "Contrôle" (0) :

* **Mesure 1, Tick 0** : `Start Song` (`F0 7D 02 F7`)
* **Mesure 1, Tick 1** : `Allocate Part` pour le piano (Canal 0)
* **Mesure 1, Tick 2** : `Allocate Part` pour la basse (Canal 1)
* **Mesure 1, Tick 3** : `Allocate Part` pour les cordes (Canal 2)
* **Mesure 2, Temps 1** : *(Début de la musique, notes sur les autres pistes)*
* **Mesure 8, Temps 1** : Placement d'un `Marker 01` (pour animer un élément du décor).
* **Mesure 12, Temps 4** : Placement d'un `Set Loop` (pour boucler sur la mesure 2).

Dans le **Chapitre 6**, nous allons faire de l'arithmétique hexadécimale pour comprendre comment forger manuellement les paramètres de chaque message SysEx sans se tromper !
