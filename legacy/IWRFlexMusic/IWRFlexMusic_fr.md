# IWRFlexMusic - Guide d'intégration

Module AGS pour la gestion interactive de la musique via le design pattern **États et Séquences** (façon iMUSE / LucasArts), reposant sur le plugin **agsimwrap**.

## Philosophie

L'architecture est pensée pour les jeux où **chaque pièce ou ambiance possède son propre `Sound ID`** (sa propre séquence MIDI).

Dans cette séquence MIDI :
* **Track 0** : La boucle principale de la pièce.
* **Track > 0** : Les pistes de transition *sortantes* vers d'autres pièces.

Le passage de la Track 0 aux transitions se fait grâce à un **Hook Jump** intégré au fichier MIDI par le compositeur.

## Démarrage Rapide

Dans `game_start()` :
```ags
// On charge la banque son globale
iMWrap_LoadBank("music/game.ims");

// On lance la musique de la première room (ex: séquence 90)
IWRFlexMusic.Play(90);
```

En jeu, pour passer de la room de base (Seq 90) vers la room Hologramme (Seq 91) avec une transition synchronisée :
```ags
// Paramètres : (Seq Actuelle, Seq Cible, HookID de la transition dans la Seq Actuelle)
IWRFlexMusic.SetTransition(90, 91, 1);
```

## Que se passe-t-il sous le capot ?

1. `SetTransition(90, 91, 1)` dit à iMWrap de libérer la boucle actuelle et d'armer le **Hook 1** sur la Séquence 90.
2. Lorsque la tête de lecture MIDI croise le Hook Jump correspondant, la Séquence 90 saute sur la piste de transition.
3. À la fin de cette piste de transition, le compositeur a placé le **Marker 64**.
4. Le callback global `iMWrap_OnTrigger` intercepte ce Marker 64, arrête automatiquement la Séquence 90, et lance la Séquence 91 (qui commence sur sa Track 0).

## Enfilades de transitions

Grâce à ce système, vous pouvez chainer les transitions simplement. Si le joueur fait demi-tour depuis la room Hologramme vers la room de base :

```ags
// La Seq 91 joue. On arme son Hook 1 pour déclencher sa piste de transition de retour.
// Au Marker 64, le système relancera la Seq 90.
IWRFlexMusic.SetTransition(91, 90, 1);
```
