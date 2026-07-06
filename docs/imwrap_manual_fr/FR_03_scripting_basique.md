# Chapitre 3 : Le Scripting AGS (Contrôle direct)

Maintenant que vous savez lancer une musique (`iMWrap_StartSound`), nous allons apprendre à la manipuler en temps réel pendant sa lecture. C'est ce qu'on appelle le **contrôle direct**.

Dans iMWrap, toutes les modifications que vous apportez à la musique se font sans bégaiement. Vous pouvez agir sur le volume, le panoramique (gauche/droite), la vitesse, ou même couper le son de certains instruments en plein vol.
---

## 3.1. Contrôle du Volume et Panoramique

Le moteur propose trois niveaux pour gérer le volume, de l'échelle macro (le jeu entier) à l'échelle micro (un instrument spécifique).

### Le Volume Principal
Il définit le niveau sonore global du jeu. Utile pour l'écran des options où le joueur règle ses préférences sonores. La valeur va de `0` (silence) à `127` (maximum).
```c
iMWrap_SetMasterVolume(100); 
```

### Le Volume d'un son
Il modifie le volume de la séquence musicale actuellement lue, de manière relative.
```c
// Baisse la musique 80 pour laisser place aux dialogues
iMWrap_SetSoundVolume(80, 40); 
```

### Le Panoramique (Pan)
Vous pouvez déplacer l'origine du son dans l'espace stéréo (très utile si une radio joue la musique dans le décor à gauche de l'écran).
La valeur va de `-63` (complètement à gauche) à `64` (complètement à droite). `0` est au centre.
```c
// Place la musique complètement sur l'enceinte droite
iMWrap_SetSoundPan(80, 64);
```

---

## 3.2. Les Fondus (Fades)

Changer le volume d'un coup sec manque parfois d'élégance. iMWrap intègre un système de fondus automatisé (*Fade*) extrêmement fluide.

> [!IMPORTANT]
> **La notion de Tick**
> Dans iMWrap, le temps ne se mesure pas en secondes, mais en "Ticks". C'est l'unité rythmique de la partition. Souvent, la résolution d'un fichier MIDI est de **120 ticks par noire**. À un tempo moyen de 120 BPM, un tick dure environ `4 millisecondes`. Un fondu sur `120` ticks dure donc le temps d'un battement (une noire). Un fondu sur `480` ticks dure une mesure entière (4 temps).

Pour baisser doucement la musique de la forêt (ID 80) vers le silence (volume 0) sur une durée de 2 mesures environ (ex: `1000` ticks) :
```c
iMWrap_Fade(80, 0, 1000);
```
*(Vous pouvez évidemment l'utiliser pour un fondu entrant (Fade In) en démarrant la musique à volume 0, puis en appelant `iMWrap_Fade(80, 127, 1000)`.)*

---

## 3.3. Contrôler les instruments séparément (Les Parts)

Chaque musique est composée de pistes (appelées "**Parts**", et assignées dynamiquement à des **canaux MIDI**). Habituellement, le canal 0 est la mélodie, le canal 1 la basse, le canal 9 la batterie. iMWrap vous permet de piloter ces canaux individuellement !

Imaginons une scène d'infiltration. Vous lisez la musique 80. Quand le joueur est caché, seule la basse et la batterie jouent. S'il est repéré par un garde, vous pouvez activer la piste des cuivres (sur le canal 3) instantanément pour ajouter de la tension !

**Désactiver / Activer un canal (Mute) :**
```c
// Coupe le canal 3 (0 = inactif) de la musique 80
iMWrap_SetPartOnOff(80, 3, 0);

// Réactive le canal 3 (1 = actif)
iMWrap_SetPartOnOff(80, 3, 1);
```

**Modifier le volume d'un seul canal :**
```c
// Baisse discrètement la basse (canal 1) à 30/127
iMWrap_SetPartVolume(80, 1, 30);
```

---

## 3.4. Vitesse et Transposition

### Transposition (Transpose)
Vous pouvez transposer tout un morceau pour le rendre plus aigu ou plus grave. Très utile si le joueur rapetisse ou tombe dans un rêve ! La transposition se fait par demi-tons (comme sur un clavier de piano).
```c
// Paramètre 2: 0 (absolu) ou 1 (relatif)
// Paramètre 3: Nombre de demi-tons (-12 à 12, etc.)
// Descend la musique d'un octave (-12 demi-tons)
iMWrap_SetSoundTranspose(80, 0, -12); 
```

### Vitesse (Speed)
La vitesse (tempo) normale du morceau correspond à la valeur de base `128`. Des valeurs inférieures ralentissent, des valeurs supérieures accélèrent la lecture. L'accélération d'une séquence n'affecte pas sa hauteur (contrairement à l'accélération d'un fichier audio).
```c
// Accélère drastiquement la musique 80 (ex: scène de course-poursuite)
iMWrap_SetSoundSpeed(80, 180);

// Revient à la vitesse normale
iMWrap_SetSoundSpeed(80, 128);
```

---

## 3.5. Savoir où l'on en est

Si votre script a besoin de savoir précisément à quel endroit de la partition se trouve la lecture (par exemple, pour déclencher une animation exactement au début de la mesure 10), vous pouvez interroger la "tête de lecture".

La position musicale est définie par trois notions (Track, Beat, Tick) :
* **Track** : La piste "séquentielle" globale du fichier (souvent 0).
* **Beat** : La mesure en cours.
* **Tick** : La subdivision temporelle à l'intérieur de la mesure.

```c
int mesureActuelle = iMWrap_GetPlaybackBeat(80);

if (mesureActuelle >= 10) {
    // La lecture a dépassé la mesure 10
}
```

Cependant, *scruter* (poller) constamment la fonction `iMWrap_GetPlaybackBeat` dans la fonction `repeatedly_execute` d'AGS n'est pas la méthode la plus élégante ni la plus précise. 

Dans le **Chapitre 4**, nous allons voir comment utiliser les **Hooks** et les **Markers**, qui permettent à la musique de vous notifier de manière asynchrone et avec une précision à la milliseconde près !
