# Chapitre 4 : La Musique Interactive (Hooks, Callbacks...)

C'est ici que l'engine iMWrap révèle tout son potentiel. Jusqu'à présent, nous étions "esclaves" de la musique, ou bien nous la manipulions brutalement (baisser le volume n'importe quand, couper un canal instantanément).

Avec le système hérité d'iMUSE, **la musique devient actrice**. Elle décide du *moment idéal* pour que vos scripts s'exécutent, afin de préserver la magie et la fluidité.

Cependant, vous verrez que ce n'est pas la manière la plus optimale de gérer la réactivité de la musique. Anéfé, lorsque vous utilisez ces commandes, et notamment les Jumps et les Loops, directement depuis les scripts, c'est le **programmeur** qui a la main. Or, il est souvent plus opportun de laisser ces décisions entre les mains du compositeur. C'est ce que vous découvrirez au **paragraphe 4.4** et dans le **Chapitre 5**.

---

## 4.1. Le Saut Temporel (Jump)

Imaginons que la musique d'introduction de votre jeu a une séquence d'attente (vamping) qui boucle. Quand le joueur clique sur "Nouvelle Partie", vous ne voulez pas lancer une musique différente brutalement. Vous voulez que la musique d'introduction saute vers son grand final glorieux, mais **en rythme**.

C'est ce que permet `iMWrap_Jump`. Cette fonction demande au séquenceur de déplacer la tête de lecture à un point précis.

```c
// Saute le son 80 à la mesure 15 (beat), tick 0.
// Le paramètre "track" vaut presque toujours 0.
iMWrap_Jump(80, 0, 15, 0);
```

> [!CAUTION]
> Un `Jump` s'exécute **immédiatement**. Si vous l'appelez au milieu d'un temps, la batterie va rater un temps et le saut s'entendra. Pour sauter de manière musicale (par exemple, à la fin de la mesure courante), il faut utiliser les **Hooks** ou les **Boucles**.

---

## 4.2. Les Boucles Dynamiques (Loops)

Bien sûr, vous pouvez boucler vos séquences directement depuis le séquenceur MIDI (voir Chapitre 5). Mais il est parfois utile de forcer une boucle **depuis un script AGS** selon ce que fait le joueur.

La fonction `iMWrap_SetLoop` dit au séquenceur : *"Quand tu arriveras à tel point (A), rembobine jusqu'à tel point (B)"*.

```c
// iMWrap_SetLoop(soundId, count, toBeat, toTick, fromBeat, fromTick)
// "Dans la musique 80, tourne en boucle infiniment (count=0). 
// Quand tu arrives au Beat 20 (Tick 0), retourne au Beat 10 (Tick 0)."
iMWrap_SetLoop(80, 0, 10, 0, 20, 0);
```

**Pourquoi est-ce interactif ?**
Imaginons que le joueur combat un boss. Vous mettez une boucle entre la mesure 10 et 20. La musique reste frénétique. Quand le boss meurt, vous appelez `iMWrap_ClearLoop(80);`. La musique atteindra la mesure 20, franchira la boucle, et entrera dans la mesure 21 qui contient la musique de victoire !

---

## 4.3. Les Callbacks (Markers) : Quand la musique réveille le jeu

Voici la fonctionnalité la plus spectaculaire. Vous voulez qu'un personnage danse sur le tempo de la musique, ou qu'un éclair frappe exactement au moment d'un coup de cymbale. 

Le compositeur a préparé son fichier MIDI en insérant des **Markers** (Marqueurs). Un Marker est un message invisible (SysEx) placé sur la partition. Par exemple, le compositeur a placé le Marqueur "1" à la mesure 5, et le Marqueur "2" sur chaque temps fort de la batterie.

Dans AGS, c'est très simple. À chaque boucle du jeu (dans votre `repeatedly_execute`), vous allez demander au moteur "As-tu croisé un marqueur ?". C'est ce qu'on appelle du *polling*. Le moteur stocke les marqueurs rencontrés dans une file d'attente (Queue), et la fonction `iMWrap_PopMarker` permet de les dépiler un par un. Comme elle doit renvoyer deux informations (l'ID du son et l'ID du marqueur), elle renvoie un entier "packé" (combiné) qu'il faut séparer.

```c
// À placer dans votre GlobalScript.asc
function repeatedly_execute()
{
    // On dépile le plus ancien marqueur en attente
    int packed = iMWrap_PopMarker();
    while (packed > 0)
    {
        // On extrait le soundId (les bits supérieurs) et le markerId (les 8 bits de poids faible)
        int soundId = (packed >> 8) & 0xFFFFFF;
        int markerId = packed & 0xFF;

        if (soundId == 80)
        {
            if (markerId == 1)
            {
                // Le marqueur 1 a été atteint ! Lancer l'éclair !
                Display("BOOM !");
                cEgo.Say("Quel orage !");
            }
            else if (markerId == 2)
            {
                // Un temps fort de batterie, on fait danser le personnage d'une frame
                cDanseur.Animate(1, 0, eOnce, eNoBlock);
            }
        }
        
        // On passe au marqueur suivant dans la file
        packed = iMWrap_PopMarker();
    }
}
```

---

## 4.4. Les Hooks : L'action asynchrone

Le Hook ("crochet") est le concept inverse du Marker. Avec un Hook, **le jeu (AGS) donne un ordre au moteur musical, mais l'ordre est mis en pause**. L'ordre ne s'exécutera que lorsque la partition musicale l'autorisera.

Pourquoi faire ça ? Reprenons l'exemple du volume. Vous voulez que le volume des violons augmente quand on entre dans une pièce, mais vous voulez que ce soit fait *sur le premier temps de la mesure* pour que l'effet dramatique soit parfait, et non pas de manière aléatoire pendant que le joueur marche.

Le compositeur écrit des "SysEx de Hook" vides aux endroits stratégiques de sa partition (par exemple, au début de chaque mesure).

Dans AGS, vous "armez" le crochet en utilisant les *wrappers* (des fonctions spécifiques pour chaque type de Hook) :
```c
// iMWrap_SetPartVolumeHook(soundId, hookId, channel)

// On arme un Hook de Changement de Volume sur le canal 0.
// hookId = 1 : on attend spécifiquement le Hook d'ID "1" placé par le compositeur.
// (Si hookId valait 0, n'importe quel Hook de volume déclencherait l'action inconditionnellement).
// Note : Le nouveau volume n'est PAS passé dans ce code AGS. 
// L'action réelle est DÉJÀ encodée dans le SysEx du MIDI par le compositeur !
iMWrap_SetPartVolumeHook(80, 1, 0);
```

La musique tourne... elle rencontre le point préparé par le compositeur... BAM ! Le Hook se déclenche et le volume change en rythme.

**Les wrappers de Hooks disponibles dans AGS :**
- `iMWrap_SetJumpHook` : Hook de Jump (Saut asynchrone en rythme)
- `iMWrap_SetGlobalTransposeHook` : Hook de Transposition globale
- `iMWrap_SetPartOnOffHook` : Hook de Part On/Off (Allumer/éteindre un instrument)
- `iMWrap_SetPartVolumeHook` : Hook de Volume (Modifier le volume d'un canal)
- `iMWrap_SetPartProgramHook` : Hook de Program (Changer d'instrument)
- `iMWrap_SetPartTransposeHook` : Hook de Transposition d'une piste

> [!TIP]
> Si la musique que vous lisez ne contient aucun événement "Hook" codé par le compositeur, vos appels aux fonctions `iMWrap_Set...Hook` dans AGS ne s'exécuteront **jamais**, car le moteur attendra éternellement l'autorisation de la partition. C'est un vrai travail d'équipe !

### 4.5. Exemple Avancé : Combiner Hook et Callback pour une transition

Imaginons que le joueur résout une énigme. Nous voulons :
1. Que la musique actuelle (ID 80) amorce sa séquence de fin "propre" (via un Hook de Jump).
2. Qu'une fois la séquence de fin terminée, la musique signale au jeu (via un Marker) de lancer la nouvelle musique de victoire (ID 81).

**Côté Compositeur (Musique 80) :**
- À la mesure 10, le compositeur a placé un **Hook de Jump**. Dans ce SysEx, il a déjà encodé la destination (sauter à la mesure 50) !
- À la mesure 50 (la vraie fin glorieuse du morceau), il a placé un **Marker ID 10**.

**Côté Programmeur (AGS) :**
```c
// Lorsque le joueur résout l'énigme :
function resoudre_enigme()
{
    // On arme le Hook de Jump.
    // hookId = 0 (inconditionnel). Quand la musique atteindra la mesure 10,
    // elle validera ce Hook et sautera vers son final.
    iMWrap_SetJumpHook(80, 0);
}

// Plus tard, dans votre repeatedly_execute, on scrute les marqueurs :
function repeatedly_execute()
{
    int packed = iMWrap_PopMarker();
    while (packed > 0)
    {
        int soundId = (packed >> 8) & 0xFFFFFF;
        int markerId = packed & 0xFF;

        if (soundId == 80 && markerId == 10)
        {
            // La musique 80 a fini sa conclusion proprement !
            iMWrap_StopSound(80);
            
            // On lance la musique de victoire !
            iMWrap_StartSound(81);
        }
        
        packed = iMWrap_PopMarker();
    }
}
```
Ce combo est l'essence même de la musique interactive de type iMUSE : le jeu initie le changement, la musique l'orchestre musicalement pour rester en rythme, puis la musique rend la main au jeu pour l'étape d'après !

### 4.6. Exemple Avancé : Transition Parfaite avec les Queues

Dans les jeux d'aventure complexes, vous aurez souvent besoin d'exécuter un lot d'actions musicales de manière parfaitement synchronisée avec un événement musical (comme un Hook). C'est là que le système de File d'Attente (Queue) brille. L'exemple suivant montre comment gérer la transition musicale entre deux pièces :

```c
// ==========================================
// Quand le joueur entre dans la pièce :
// ==========================================
function room_Load()
{
    // 1. On nettoie la file d'attente globale au cas où de vieilles commandes traîneraient
    iMWrap_ClearQueue();
    
    // 2. On prépare la séquence d'instructions pour la musique maître (MUSIC_BASEROOM) :
    // - On lève le marqueur 64 pour signaler au jeu le changement d'état
    iMWrap_QueueTrigger(MUSIC_BASEROOM, 64);
    // - On demande le lancement de la nouvelle musique (MUSIC_HOLODECK)
    iMWrap_QueueStartSound(MUSIC_BASEROOM, MUSIC_HOLODECK);
    
    // 3. On valide cette file d'attente sur le son maître
    iMWrap_CommitQueue(MUSIC_BASEROOM);
    
    // 4. On arme le Hook de Jump. 
    // Quand la musique de la base (MUSIC_BASEROOM) atteindra son point de transition 
    // (prévu par le compositeur avec JH_BASE_TO_HOLO), elle exécutera instantanément
    // toutes les commandes que nous venons de commiter dans sa Queue !
    iMWrap_SetJumpHook(MUSIC_BASEROOM, JH_BASE_TO_HOLO);
    
    // 5. Cas de secours (anti-silence) :
    // Si la musique de la base était arrêtée (par exemple après le chargement d'une sauvegarde),
    // on lance la nouvelle musique manuellement pour éviter le silence.
    if (iMWrap_GetActiveSoundCount() == 0)
    {
        iMWrap_StartSound(MUSIC_HOLODECK);
    }
}

// ==========================================
// Quand le joueur quitte la pièce (Retour) :
// ==========================================
function room_Leave()
{
    // On répète exactement la même logique, mais dans l'autre sens !
    // Cette fois, c'est MUSIC_HOLODECK qui est la musique maître de la transition.
    
    iMWrap_ClearQueue(); 
    iMWrap_QueueTrigger(MUSIC_HOLODECK, 64);
    iMWrap_QueueStartSound(MUSIC_HOLODECK, MUSIC_BASEROOM);
    iMWrap_CommitQueue(MUSIC_HOLODECK);
    
    iMWrap_SetJumpHook(MUSIC_HOLODECK, JH_HOLO_TO_BASE);
}
```

Cet exemple est très parlant : il montre l'interaction entre le "Hook" qui attend l'autorisation de la partition (le bon moment rythmique), et la "Queue" qui exécute un lot d'actions précises au moment exact de ce Hook !

Dans le **Chapitre 5**, nous allons passer de l'autre côté du miroir : comment le compositeur prépare tous ces éléments interactifs dans son séquenceur !
