# iMWrap v6

iMWrap v6 est une reimplementation C++ moderne et modulaire du moteur de
musique interactive utilise par les jeux LucasArts de l'ere iMUSE v6.

Le depot contient :

- le moteur C++ principal dans `src/` et `include/`
- des outils en ligne de commande maintenus dans `tools/`
- des outils graphiques Qt maintenus dans `tools_gui/`
- les aides d'integration AGS dans `ags-wrapper/` et `ags-editor-plugin/`
- des surfaces legacy dans `legacy-tools/`
- des dependances vendorisees dans `third_party/`

## Notes de Build

Le depot peut etre configure dans un mode minimal sans Qt ni dependances AGS :

```powershell
cmake -S . -B build/minimal `
  -DIMWRAP_BUILD_GUI_TOOLS=OFF `
  -DIMWRAP_BUILD_AGS_PLUGIN=OFF `
  -DIMWRAP_BUILD_SETMIDI=OFF `
  -DIMWRAP_BUILD_SHIM=OFF
```

Les builds GUI Windows demandent que Qt 6 soit decouvrable via
`CMAKE_PREFIX_PATH` ou `IMWRAP_QT6_PREFIX_PATH`.

Le plugin AGS et certains scripts de packaging attendent aussi des artefacts
tiers vendorises sous `third_party/*-build/`.

`IMWRAP_BUILD_SHIM` controle le DLL de compatibilite legacy `imwrap_shim`. Il
reste buildable, mais ses sources vivent maintenant dans `legacy-tools/`.

## Organisation du Depot

- `src/`, `include/` : sources et headers du moteur
- `tools/` : outils en ligne de commande maintenus, dont le `imwrappack` actuel
- `tools_gui/` : utilitaires Windows natifs avec interface
- `ags-wrapper/`, `ags-editor-plugin/` : integration .NET et plugin editeur AGS
- `legacy-tools/` : anciens outils Swift, sources du shim legacy et code de l'ancien packer CLI
- `Package.swift` : manifeste Swift conserve pour compatibilite avec les sources legacy deplacees
- `samples/` : exemples de banques `.ims`
- `third_party/` : sources des dependances vendorisees

## Auteur

Masami Komuro

## Licence

iMWrap v6 est distribue sous GNU Lesser General Public License v3.0 ou
ulterieure. Voir `LICENSE.LESSER` et `LICENSE.GPL`.

Les licences des dependances vendorisees sont referencees dans
`THIRD_PARTY_LICENSES.md` et dans les fichiers de licence amont conserves sous
`third_party/`.
