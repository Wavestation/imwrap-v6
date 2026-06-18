# Bundle Windows `final-release/windows`

Ce bundle rassemble les sorties Windows du projet dans un seul dossier de diffusion.

## Organisation

- Les binaires du projet sont copies a la racine de `final-release/windows`.
- Les executables outils sont ranges dans `final-release/windows/tools/`.
- Les binaires propres au projet portent un suffixe d'architecture :
  - `-x32` pour les sorties Win32
  - `-x64` pour les sorties x64
- Le moteur est livre a la fois en DLL et en bibliotheque statique :
  - `imuse_v6-x32.dll` et `imuse_v6-x64.dll`
  - `imuse_v6-x32.lib` et `imuse_v6-x64.lib` pour les import libs de ces DLL
  - `imuse_v6-static-x32.lib` et `imuse_v6-static-x64.lib` pour la liaison statique
- Les DLL Qt restent non renommees, car leurs noms sont imposes par les executables Qt.
- Les DLL Qt et leurs sous-dossiers de plugins sont copies dans `tools/` pour que les utilitaires GUI restent portables :
  - `tools/platforms/`
  - `tools/styles/`
  - `tools/generic/`
- `tools/imageformats/` est copie seulement si `windeployqt` l'a effectivement genere.
- La documentation, les wrappers, les licences et les exemples sont ranges dans :
  - `docs/`
  - `wrappers/`
  - `licenses/`
  - `examples/`

## Generation

Depuis la racine du depot :

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-windows-release.ps1
```

Pour reconstruire toute la release Windows depuis zero :

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-windows-release.ps1 -QtPrefixPath "C:\Qt\6.8.0\msvc2022_64"
```

Par defaut, le script assemble :

- `build_msvc_win32\Release` pour le plugin AGS et les outils Win32
- `build_gui\Release` pour les outils Qt x64 et les bibliotheques x64

Le script de build complet utilise plutot :

- `build_msvc_win32` pour Win32
- `build_msvc_x64` pour x64
- `third_party\fluidsynth-build` pour la dependance FluidSynth Win32 vendoree du plugin AGS

## Release GitHub v1

Le workflow [release-v1.yml](/D:/Prog/imwrap-v6/.github/workflows/release-v1.yml) est prevu pour :

1. installer Qt 6.8.0 sous Windows ;
2. compiler le bundle Windows complet ;
3. regenerer `final-release/windows` ;
4. creer un zip `imwrap-vX.Y.Z-windows.zip` ;
5. publier ce zip comme asset de release sur un tag `v*`.

## Notes utiles

- `SetMIDI-x64.exe` ignore maintenant les autres utilitaires livres dans le bundle, y compris les variantes suffixees `-x32` et `-x64`.
- `SetMIDI-x64.exe` est livre dans `tools/`. Pour configurer un vrai jeu AGS, il faut le copier a cote de l'executable du jeu.
- `agsimuse-x32.dll` correspond au plugin AGS pret a etre copie dans un projet AGS Win32.
- Le bundle inclut aussi l'exemple `monkey-island.ims` pour avoir un fichier de reference directement sous la main.
