# Bundle Windows `final-release/imwrap-windows`

Ce bundle rassemble les sorties Windows du projet dans un seul dossier de diffusion.

## Organisation

- Les binaires du projet sont copiés à la racine de `final-release/imwrap-windows`.
- Les exécutables outils sont rangés dans `final-release/imwrap-windows/tools/`.
- Les binaires propres au projet portent un suffixe d'architecture :
  - `-x32` pour les sorties Win32
  - `-x64` pour les sorties x64
- Le moteur est livré à la fois en DLL et en bibliothèque statique :
  - `imwrap_v6-x32.dll` et `imwrap_v6-x64.dll`
  - `imwrap_v6-x32.lib` et `imwrap_v6-x64.lib` pour les import libs de ces DLL
  - `imwrap_v6-static-x32.lib` et `imwrap_v6-static-x64.lib` pour la liaison statique
- Les DLL Qt restent non renommées, car leurs noms sont imposés par les exécutables Qt.
- Les DLL Qt et leurs sous-dossiers de plugins sont copiés dans `tools/` pour que les utilitaires GUI restent portables :
  - `tools/platforms/`
  - `tools/styles/`
  - `tools/generic/`
- `tools/imageformats/` est copié seulement si `windeployqt` l'a effectivement généré.
- Si le PoC VST3 SysEx est compilé, son bundle `imwrap_sysex_tool.vst3/` est aussi copié dans `tools/`.
- `SetMIDI` est maintenant un utilitaire Win32 natif. Il est livré en `x32` et `x64` et ne dépend plus de Qt.
- La documentation, les wrappers, les licences et les exemples sont rangés dans :
  - `docs/`
  - `wrappers/`
  - `licenses/`
  - `examples/`
- Le plugin éditeur AGS et son shim natif sont rangés dans `ags-editor-plugin/`.

## Génération

Depuis la racine du dépôt :

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-windows-release.ps1
```

Pour reconstruire toute la release Windows depuis zéro :

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-windows-release.ps1 -QtPrefixPath "C:\Qt\6.8.0\msvc2022_64"
```

Avec un SDK VST3 local explicite :

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-windows-release.ps1 -QtPrefixPath "C:\Qt\6.8.0\msvc2022_64" -Vst3SdkDir "D:\Prog\third_party\vst3sdk"
```

Par défaut, le script assemble :

- `.build\windows-release\win32\Release` pour le plugin AGS et les outils Win32
- `.build\windows-release\x64\Release` pour les outils Qt x64 et les bibliothèques x64
- `ags-editor-plugin\bin\Release` pour l'assembly du plugin éditeur AGS

Le script de build complet utilise plutôt :

- `.build\windows-release\win32` pour Win32
- `.build\windows-release\x64` pour x64
- `third_party\fluidsynth-build` pour la dépendance FluidSynth Win32 vendorée du plugin AGS
- `third_party\vst3sdk` si vous voulez que le build Windows prenne automatiquement en compte le PoC VST3

## Release GitHub v1

Le workflow [release-v1.yml](/D:/Prog/imwrap-v6/.github/workflows/release-v1.yml) est prévu pour :

1. installer Qt 6.8.0 sous Windows ;
2. compiler le bundle Windows complet ;
3. régénérer `final-release/imwrap-windows` ;
4. créer un zip `imwrap-vX.Y.Z-windows.zip` ;
5. publier ce zip comme asset de release sur un tag `v*`.

## Notes utiles

- `SetMIDI-x32.exe` et `SetMIDI-x64.exe` ignorent les autres utilitaires livrés dans le bundle, y compris les variantes suffixées `-x32` et `-x64`.
- `SetMIDI-x32.exe` est le meilleur choix pour accompagner un jeu AGS Win32. Il suffit de le copier à côté de l'exécutable du jeu.
- `SetMIDI-x64.exe` est également fourni pour les environnements Windows x64.
- `agsimwrap-x32.dll` correspond au plugin AGS prêt à être copié dans un projet AGS Win32.
- `ags-editor-plugin/AGS.Plugin.IMWrap.Editor.dll` correspond au plugin de l'éditeur AGS, à copier dans le dossier de l'éditeur avec `ags-editor-plugin/imwrap_shim.dll`.
- Le bundle inclut aussi l'exemple `monkey-island.ims` pour avoir un fichier de référence directement sous la main.
