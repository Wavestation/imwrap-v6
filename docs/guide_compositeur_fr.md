# Guide compositeur iMWrap

Ce guide est une version courte, orientee pratique, pour authorer des SysEx iMWrap dans un sequenceur.

Pour la specification complete, voir :

- [imwrap_sysex_format.md](./imwrap_sysex_format.md)
- [imwrap_sysex_reference_detaillee_fr.md](./imwrap_sysex_reference_detaillee_fr.md)

## 1. La regle simple

Si vous composez un morceau interactif pour `imwrap-v6`, partez sur cette logique :

1. au tout debut, envoyez `Start Song`
2. allouez chaque part utile avec `Allocate Part`
3. placez ensuite vos notes MIDI normales
4. ajoutez si besoin des `Marker`, `Set Loop` et `Hooks`

La forme generale est toujours :

```text
F0 7D [CODE] [PAYLOAD] F7
```

## 2. Le kit minimal

### 2.1. Reset du morceau

```text
F0 7D 02 F7
```

Placez ce message au tick `0`.

### 2.2. Allocation d'une part

Exemple de part minimale :

```text
F0 7D 00 00 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 00 F7
```

Cela signifie en pratique :

- part logique `0`
- part active
- priorite `0x5A`
- volume `0x7F`
- pan centre
- transpose `0`
- detune `0`
- pitch bend factor `2`
- programme `0`

Faites ensuite une allocation similaire pour chaque part logique dont vous avez besoin.

## 3. Les messages les plus utiles en composition

### 3.1. Marker

Format :

```text
F0 7D 40 00 [ID] F7
```

Exemple :

```text
F0 7D 40 00 01 F7
```

Conseil important :

- utilisez un seul octet utile si vous voulez un seul trigger
- n'utilisez pas une chaine ASCII complete si vous pensez obtenir un seul marqueur logique

Dans le code actuel, chaque octet utile declenche un marqueur separe.

### 3.2. Set Loop

Format :

```text
F0 7D 50 00 [COUNT16] [TO_BEAT16] [TO_TICK16] [FROM_BEAT16] [FROM_TICK16] F7
```

Exemple :

```text
F0 7D 50 00 00 00 00 02 00 00 00 04 00 00 00 00 00 00 00 0A 00 00 00 00 F7
```

Ici :

- boucle `2` fois
- quand on atteint `beat 10 tick 0`
- on saute a `beat 4 tick 0`

Points a retenir :

- `count = 0` ne donne pas une boucle infinie dans ce moteur
- utilisez `count >= 1`
- le point `FROM` doit etre plus loin que le point `TO`

Pour annuler une boucle :

```text
F0 7D 51 F7
```

### 3.3. Hooks

Les hooks servent a conditionner une action a un etat arme par le jeu.

Les plus utiles sont :

- `0x30` : jump
- `0x32` : part on/off
- `0x33` : volume
- `0x34` : programme
- `0x35` : transpose de part

En pratique :

- `cmd = 00` : action inconditionnelle
- `cmd = 01..7F` : action conditionnelle
- `cmd = FF` : hook persistant arme par defaut dans l'etat reset du moteur

Si vous ne pilotez pas les hooks cote jeu, vous pouvez ignorer cette famille.

## 4. Ce qu'il vaut mieux eviter

### `0x21` Parameter Adjust

Ne comptez pas dessus aujourd'hui.

Le moteur actuel le parse, mais n'en fait rien.

### Marker texte long

Evitez :

```text
F0 7D 40 00 49 4E 54 52 4F F7
```

si vous voulez un seul trigger logique.

Dans ce code, cela declenche cinq marqueurs successifs :

- `I`
- `N`
- `T`
- `R`
- `O`

## 5. Conseils par profil sonore

### General MIDI

Le plus simple :

- `Start Song`
- `Allocate Part`
- notes MIDI classiques
- `Marker` / `Loop` si besoin

### Roland MT-32

Si vous utilisez des timbres personnalises Roland :

- placez les dumps SysEx Roland avant `Allocate Part`
- laissez une marge, idealement une cinquantaine de ticks, avant les premieres notes

Le moteur sait ensuite :

- stocker le dump par part
- le recharger sur la memoire timbre MT-32
- rafraichir le patch actif

### AdLib

Si vous travaillez avec des instruments OPL :

- utilisez `0x10` pour une part
- utilisez `0x11` pour une diffusion globale
- considerez `30` octets comme taille canonique d'un instrument AdLib iMUSE

## 6. Workflow conseille dans un sequenceur

### Piste de controle

Utilisez une piste dediee, souvent la piste `0`, pour les SysEx :

- `Start Song`
- `Allocate Part`
- `Marker`
- `Set Loop`
- `Hooks`

### Pistes musicales

Gardez ensuite les pistes musicales normales pour :

- notes
- CC standards
- Program Changes si necessaire

## 7. Exemple de structure simple

### Intro puis boucle

Tick `0` :

```text
F0 7D 02 F7
F0 7D 00 00 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 00 F7
F0 7D 00 01 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 01 F7
```

Au point d'entree de boucle :

```text
F0 7D 40 00 01 F7
```

Au point de sortie de section :

```text
F0 7D 50 00 00 00 00 03 00 00 00 08 00 00 00 00 00 00 00 10 00 00 00 00 F7
```

Ce dernier exemple signifie :

- boucle `3` fois
- quand on atteint `beat 16`
- on saute a `beat 8`

## 8. En cas de doute

Si vous voulez authorer vite et proprement :

1. utilisez `Start Song` au debut
2. utilisez `Allocate Part` pour chaque part
3. utilisez `Marker` avec un seul octet utile
4. utilisez `Set Loop` avec `count >= 1`
5. laissez `Parameter Adjust` de cote

Si vous avez besoin du niveau "octet par octet", reportez-vous a :

- [imwrap_sysex_format.md](./imwrap_sysex_format.md)
- [imwrap_sysex_reference_detaillee_fr.md](./imwrap_sysex_reference_detaillee_fr.md)
