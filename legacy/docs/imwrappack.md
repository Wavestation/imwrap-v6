# imwrappack

`imwrappack` est le packer en ligne de commande maintenu pour les banques `.ims`.

Il couvre maintenant a la fois la creation et l'edition d'une banque :

- inspection d'une banque existante
- creation d'une banque vide
- build d'une banque a partir de fichiers MIDI
- ajout, renommage ou suppression de sons
- ajout ou suppression de variantes
- import MIDI dans une variante existante
- reglage ou suppression du `MDhd`
- suppression, reordonnancement et export de pistes

L'implementation v1 d'origine a ete deplacee dans
`legacy-tools/cli/imwrappack_v1.cpp`.

## Variantes Supportees

- `gmd`
- `rol`
- `adl`

## Build A Partir De MIDI

```text
imwrappack build output.ims \
  --name=80:Foret \
  --mdhd=80:gmd:90:127:0:0:0:128 \
  80:gmd=80_foret_gm.mid \
  80:rol=80_foret_mt32.mid \
  80:adl=80_foret_adlib.mid
```

Les entrees de variante gardent le format :

```text
soundId:variant=path.mid
```

L'import MIDI suit les memes regles que le Packer GUI :

- SMF 0 : importe comme une piste
- SMF 1 : fusionne en une seule piste type format 0
- SMF 2 : chaque piste du SMF devient une piste IMS

## Editer Une Banque Existante

Commandes courantes :

```text
imwrappack inspect bank.ims
imwrappack add-sound bank.ims 80 gmd --name=Foret
imwrappack import-midi bank.ims 80 gmd foret.mid
imwrappack set-mdhd bank.ims 80 gmd 90 127 0 0 0 128
imwrappack move-track bank.ims 80 gmd 1 up
imwrappack export-track bank.ims 80 gmd 0 foret_track0.mid
```

## Nom De Debug

```text
--name=80:Woodtick
```

Assigne le chunk `NAME` de debug du son cible.

## `MDhd`

```text
--mdhd=soundId:variant:priority:volume:pan:transpose:detune:speed
```

Si `MDhd` n'est pas defini pour une variante, la banque conserve les valeurs
runtime par defaut.
