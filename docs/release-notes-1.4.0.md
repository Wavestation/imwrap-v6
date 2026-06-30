# iMWrap 1.4.0 (Révision majeure)

Date : 2026-06-30

Cette version majeure 1.4.0 corrige un problème historique et critique de mapping d'opérateurs OPL2 dans l'émulation AdLib, qui provoquait auparavant un rendu sonore catastrophique et corrompu.

## Nouveautés et corrections majeures

- **Correction de l'émulation AdLib (OPL)** : Résolution d'une inversion critique entre le Modulateur et le Porteur (Carrier) lors de la conversion des timbres iMUSE vers la structure `ADL_Instrument` de **libADLMIDI**.
- **Alignement du mapping** : Les paramètres du Porteur (`data[5..9]`) sont désormais correctement assignés à `operators[0]`, et ceux du Modulateur (`data[0..4]`) à `operators[1]`, en totale conformité avec les spécifications de libADLMIDI.
- Cette correction s'applique de manière uniforme à l'ensemble du dépôt :
  - Au plugin AGS (`agsimwrap-x32.dll`)
  - Au shim natif (`imwrap_shim.dll` / `imwrap_shim_x64.dll`)
  - Aux outils de prévisualisation de l'interface graphique (Explorer, Player, Éditeur SysEx)

## Améliorations

- Le rendu audio des variantes AdLib/OPL est maintenant pleinement fidèle aux compositions iMUSE et aux timbres originaux des classiques de LucasArts sous DOS (comme *Monkey Island 2* ou *Day of the Tentacle*).

## Bundle Windows

Le bundle Windows de cette version embarque les binaires corrigés et recompilés :

- `agsimwrap-x32.dll` (Plugin AGS)
- `imwrap_explorer_gui-x64.exe`
- `imwrap_player_gui-x64.exe`
- `imwrap_sysex_gui-x64.exe`

Nom d'archive recommandé :

`imwrap-v1.4.0-windows.zip`
