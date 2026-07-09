# iMWrap v6

iMWrap v6 est une réimplementation C++ moderne et modulaire du moteur de musique interactive utilisé par les jeux LucasArts de l'ère iMUSE v6.

Le dépôt contient :
- le moteur C++ principal dans `src/` et `include/`
- les aides d'intégration AGS dans `ags-wrapper/` et `ags-editor-plugin/`
- des utilitaires SwiftUI dans `swift-app/`
- des outils en ligne de commande dans `tools/`
- des dépendances vendorisées dans `third_party/`

## Notes de Build

Le dépôt peut être configuré dans un mode minimal sans Qt ni dépendances AGS :

```powershell
cmake -S . -B build/minimal `
  -DIMWRAP_BUILD_GUI_TOOLS=OFF `
  -DIMWRAP_BUILD_AGS_PLUGIN=OFF `
  -DIMWRAP_BUILD_SETMIDI=OFF `
  -DIMWRAP_BUILD_SHIM=OFF
```

Les builds GUI Windows demandent que Qt 6 soit découvrable via `CMAKE_PREFIX_PATH` ou `IMWRAP_QT6_PREFIX_PATH`.

Le plugin AGS et certains scripts de packaging attendent aussi des artefacts tiers vendorisés sous `third_party/*-build/`.

## Organisation du Dépôt

- `src/`, `include/` : sources et headers du moteur
- `tools/` : `imwrappack` et `imsprobe`
- `tools_gui/` : utilitaires Windows natifs avec interface
- `swift-shim/`, `swift-app/` : shim natif et apps macOS
- `ags-wrapper/`, `ags-editor-plugin/` : intégration .NET et plugin éditeur AGS
- `samples/` : exemples de banques `.ims`
- `third_party/` : sources des dépendances vendorisées

## Auteur

Masami Komuro

## Licence

iMWrap v6 est distribué sous GNU Lesser General Public License v3.0 ou ultérieure. Voir `LICENSE.LESSER` et `LICENSE.GPL`.

Les licences des dépendances vendorisées sont référencées dans `THIRD_PARTY_LICENSES.md` et dans les fichiers de licence amont conservés sous `third_party/`.
