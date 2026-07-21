# iMWrap 1.0.4

Date: 2026-06-29

Cette version 1.0.4 se concentre sur les outils Windows, le workflow AdLib/OPL et la lisibilite des evenements MIDI dans les interfaces.

## Nouveautes

- Explorer et Player reconnaissent maintenant les evenements MIDI standards au lieu de les afficher surtout en brut.
- Les principaux types identifies sont `Note On`, `Note Off`, `Control Change`, `Program Change`, `Pitch Bend`, ainsi que les evenements `Meta` les plus utiles.
- L'editeur SysEx de l'Explorer et le generateur SysEx peuvent maintenant importer et exporter des timbres OPL au format `SBI`.
- Le mapping du format `SBI` vers le format AdLib iMUSE suit au plus pres l'interpretation utilisee par ScummVM pour les instruments AdLib.
- L'Explorer et le Player peuvent maintenant ecouter directement les variantes AdLib avec un rendu audio OPL embarque dans l'application.

## Ameliorations

- Les outils GUI Windows basculent automatiquement vers un backend audio AdLib embarque pour les variantes OPL, sans dependre d'un peripherique MIDI externe.
- Les commandes SysEx AdLib editees dans l'Explorer peuvent etre converties plus facilement vers un timbre `SBI` exploitable dans d'autres outils compatibles Sound Blaster.
- L'affichage des evenements dans le Player est plus utile pour relire rapidement une sequence, suivre les changements de controleurs et verifier les marqueurs et metas.

## Notes de compatibilite

- Le format `SBI` ne stocke que le coeur du timbre OPL. Les octets etendus propres au format AdLib iMUSE ne peuvent pas tous etre preserves dans un export `SBI`.
- Lorsqu'un export `SBI` perd des donnees etendues non representables, les outils le signalent.

## Bundle Windows

Le bundle Windows de cette version embarque des binaires GUI recompiles pour :

- `imwrap_explorer_gui-x64.exe`
- `imwrap_player_gui-x64.exe`
- `imwrap_sysex_gui-x64.exe`

Nom d'archive recommande :

`imwrap-v1.0.4-windows.zip`
