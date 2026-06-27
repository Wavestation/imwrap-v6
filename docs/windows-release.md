# Bundle Windows `final-release/imwrap-windows`

Ce bundle rassemble les sorties Windows du projet dans un seul dossier de diffusion.

## Organisation

- Les binaires du projet sont copies a la racine de `final-release/imwrap-windows`.
- Les executables outils sont ranges dans `final-release/imwrap-windows/tools/`.
- Les binaires propres au projet portent un suffixe d'architecture :
  - `-x32` pour les sorties Win32
  - `-x64` pour les sorties x64
- Le moteur est livre a la fois en DLL et en bibliotheque statique :
  - `imwrap_v6-x32.dll` et `imwrap_v6-x64.dll`
  - `imwrap_v6-x32.lib` et `imwrap_v6-x64.lib` pour les import libs de ces DLL
  - `imwrap_v6-static-x32.lib` et `imwrap_v6-static-x64.lib` pour la liaison statique
- Les DLL Qt restent non renommees, car leurs noms sont imposes par les executables Qt.
- Les DLL Qt et leurs sous-dossiers de plugins sont copies dans `tools/` pour que les utilitaires GUI restent portables :
  - `tools/platforms/`
  - `tools/styles/`
  - `tools/generic/`
- `tools/imageformats/` est copie seulement si `windeployqt` l'a effectivement genere.
- Si le PoC VST3 SysEx est compile, son bundle `imwrap_sysex_tool.vst3/` est aussi copie dans `tools/`.
- `SetMIDI` est maintenant un utilitaire Win32 natif. Il est livre en `x32` et `x64` et ne depend plus de Qt.
- La documentation, les wrappers, les licences et les exemples sont ranges dans :
  - `docs/`
  - `wrappers/`
  - `licenses/`
  - `examples/`
- Le plugin editeur AGS et son shim natif sont ranges dans `ags-editor-plugin/`.

## Generation

Depuis la racine du depot :

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-windows-release.ps1
```

Pour reconstruire toute la release Windows depuis zero :

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-windows-release.ps1 -QtPrefixPath "C:\Qt\6.8.0\msvc2022_64"
```

Avec un SDK VST3 local explicite :

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-windows-release.ps1 -QtPrefixPath "C:\Qt\6.8.0\msvc2022_64" -Vst3SdkDir "D:\Prog\third_party\vst3sdk"
```

Par defaut, le script assemble :

- `.build\windows-release\win32\Release` pour le plugin AGS et les outils Win32
- `.build\windows-release\x64\Release` pour les outils Qt x64 et les bibliotheques x64
- `ags-editor-plugin\bin\Release` pour l'assembly du plugin editeur AGS

Le script de build complet utilise plutot :

- `.build\windows-release\win32` pour Win32
- `.build\windows-release\x64` pour x64
- `third_party\fluidsynth-build` pour la dependance FluidSynth Win32 vendoree du plugin AGS
- `third_party\vst3sdk` si vous voulez que le build Windows prenne automatiquement en compte le PoC VST3

## Release GitHub v1

Le workflow [release-v1.yml](/D:/Prog/imwrap-v6/.github/workflows/release-v1.yml) est prevu pour :

1. installer Qt 6.8.0 sous Windows ;
2. compiler le bundle Windows complet ;
3. regenerer `final-release/imwrap-windows` ;
4. creer un zip `imwrap-vX.Y.Z-windows.zip` ;
5. publier ce zip comme asset de release sur un tag `v*`.

## Notes utiles

- `SetMIDI-x32.exe` et `SetMIDI-x64.exe` ignorent les autres utilitaires livres dans le bundle, y compris les variantes suffixees `-x32` et `-x64`.
- `SetMIDI-x32.exe` est le meilleur choix pour accompagner un jeu AGS Win32. Il suffit de le copier a cote de l'executable du jeu.
- `SetMIDI-x64.exe` est egalement fourni pour les environnements Windows x64.
- `agsimwrap-x32.dll` correspond au plugin AGS pret a etre copie dans un projet AGS Win32.
- `ags-editor-plugin/AGS.Plugin.IMWrap.Editor.dll` correspond au plugin de l'editeur AGS, a copier dans le dossier de l'editeur avec `ags-editor-plugin/imwrap_shim.dll`.
- Le bundle inclut aussi l'exemple `monkey-island.ims` pour avoir un fichier de reference directement sous la main.
