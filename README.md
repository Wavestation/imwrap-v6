# iMWRAP V6

**iMWRAP V6** est une implémentation moderne, modulaire et multiplateforme en C++ du célèbre moteur interactif de musique **LucasArts iMUSE (version 6)**. Ce projet inclut un plugin d'exécution pour Adventure Game Studio (AGS), des applications graphiques macOS (SwiftUI) de lecture et de création, ainsi que des wrappers .NET (C# et VB.NET) pour s'intégrer directement dans l'éditeur de jeu d'AGS ou d'autres outils Windows.

---

## 🚀 Fonctionnalités Clés

- **Moteur C++ iMUSE V6 complet** : Gestion des évènements musicaux dynamiques, transpositions, coupures de pistes, boucles intelligentes, fades et exécution inconditionnelle ou conditionnelle des déclencheurs (**Hooks**).
- **Backends Audio & Émulation supportés** :
  - **FluidSynth** (Rendu via SoundFonts `.sf2`).
  - **Roland MT-32 Emulation** (Via la bibliothèque Munt / `mt32emu` intégrée).
  - **AdLib / OPL3** (Via l'émulateur `libADLMIDI` intégré).
  - **MIDI Matériel / Virtuel** (Via CoreMIDI sur macOS et les APIs multimédias Windows natifs `winmm.dll`).
- **Compatibilité AGS intégrée** :
  - Plugin runtime AGS (`agsimuse`) compilable sous macOS (`.dylib`) et Windows (`.dll`).
  - **NOUVEAU - API Script 100% Interactive** : Contrôle total en temps réel (Volume/Pan/Transpose par canal), gestion des sauts et des boucles (`Jump`, `SetLoop`, `Scan`), fades temporels, et exécution de scripts asynchrones sur marqueurs (`iMuse_OnTrigger`).
  - API script d'AGS pour interroger le tempo, les statuts de lecture et les profils de compatibilité (Standard vs Sam & Max).
  - Registre dynamique de correspondances de timbres Roland MT-32 personnalisés vers General MIDI.
- **Intégration .NET (Éditeur AGS)** :
  - Wrappers C# et VB.NET prêts à l'emploi avec appels Cdecl (P/Invoke) compatibles avec **Visual Studio 2012** (et versions supérieures) sous Windows 11 pour développer des extensions d'éditeur de banque de sons `.ims`.

---

## 📂 Structure du Dépôt

- **`src/` & `include/`** : Code source C++ du moteur iMUSE core.
- **`ags-wrapper/`** : Wrappers d'intégration pour les technologies .NET :
  - [ImuseShim.cs](file:///Users/komasami/Dev/scumm-tools/imwrap-v6/ags-wrapper/ImuseShim.cs) (C#)
  - [ImuseShim.vb](file:///Users/komasami/Dev/scumm-tools/imwrap-v6/ags-wrapper/ImuseShim.vb) (VB.NET)
- **`swift-shim/`** : Couche de transition de type C (`CImuseShim`) facilitant l'intégration avec Swift ou P/Invoke.
- **`swift-app/`** : Suite d'applications graphiques macOS en SwiftUI (Player, Packer, SysEx utility). Inclut l'intégration de l'émulateur AdLib (OPL2) sans aucune configuration (Zero-config).
- **`tools/`** : Utilitaires en ligne de commande C++ (`imusepack` et `imsprobe`).
- **`third_party/`** : Dépendances intégrées (`libadlmidi`, `mt32emu`, `miniaudio`).

---

## 🛠️ Instructions de Compilation

### 1. Sous Windows (Windows 11 avec Visual Studio 2012 ou supérieur)

CMake gère la génération des fichiers de solution Visual Studio.

1. Installez [CMake](https://cmake.org/download/).
2. Ouvrez **CMake (gui)**.
3. Configurez les chemins :
   - *Source* : Répertoire racine du projet (`imwrap-v6`).
   - *Build* : Un répertoire de votre choix (ex: `imwrap-v6/build_vs2012`).
4. Cliquez sur **Configure** :
   - Choisissez le générateur **Visual Studio 11 2012** (ou votre version actuelle).
   - Choisissez l'architecture cible (ex: `Win32` pour l'éditeur AGS standard).
5. Cliquez sur **Generate** puis **Open Project** pour ouvrir la solution `.sln` directement dans Visual Studio.
6. Compilez en mode *Release* pour générer la DLL du plugin AGS (`libagsimuse.dll`) et du shim (`imuse_shim.dll`).

Pour reconstruire le bundle Windows de diffusion depuis zero :

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-windows-release.ps1 -QtPrefixPath "C:\Qt\6.8.0\msvc2022_64"
```

Le dossier final prêt à distribuer est ensuite généré dans `final-release/windows`.

### 2. Sous macOS

Le script shell automatisé compile le moteur C++, le shim et produit les bundles d'applications macOS autonomes :

```bash
./scripts/make-app.sh
```
Les fichiers packagés se retrouveront dans le dossier `dist/`.

---

## 📄 Licence et Copyright

Ce programme est la propriété légale de **Masami Komuro** et de ses contributeurs.
Il est distribué sous les termes de la licence **GNU General Public License v3** (ou ultérieure). Veuillez consulter le fichier `COPYRIGHT` pour plus de détails.
