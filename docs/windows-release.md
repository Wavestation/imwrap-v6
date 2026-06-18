# Bundle Windows `final-release/windows`

Ce bundle rassemble les sorties Windows du projet dans un seul dossier de diffusion.

## Organisation

- Les binaires du projet sont copies a la racine de `final-release/windows`.
- Les binaires propres au projet portent un suffixe d'architecture :
  - `-x32` pour les sorties Win32
  - `-x64` pour les sorties x64
- Les DLL Qt restent non renommees, car leurs noms sont imposes par les executables Qt.
- Les seuls sous-dossiers conserves sont ceux que Qt exige pour fonctionner :
  - `platforms/`
  - `imageformats/`
  - `styles/`
  - `generic/`
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

Par defaut, le script assemble :

- `build_msvc_win32\Release` pour le plugin AGS et les outils Win32
- `build_gui\Release` pour les outils Qt x64 et les bibliotheques x64

## Notes utiles

- `SetMIDI-x64.exe` ignore maintenant les autres utilitaires livres dans le bundle, y compris les variantes suffixees `-x32` et `-x64`.
- `agsimuse-x32.dll` correspond au plugin AGS pret a etre copie dans un projet AGS Win32.
- Le bundle inclut aussi l'exemple `monkey-island.ims` pour avoir un fichier de reference directement sous la main.
