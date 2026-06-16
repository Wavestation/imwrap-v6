# imuse-v6

Prototype de bibliotheque iMUSE classique orientee compatibilite `SCUMM v6`, avec :

- un conteneur `.ims` chunked pour AGS ;
- un loader de banque et de ressources `SOUN` ;
- un `imusepack` minimal pour empaqueter des MIDIs type 2 ;
- un runtime `ImuseEngine` qui sait deja :
  - charger une sequence iMUSE depuis une banque `.ims` ;
  - dispatcher des `doCommand()` compatibles SCUMM v6 ;
  - avancer dans le temps en ticks ;
  - sortir les vrais evenements MIDI musicaux au fil du transport ;
  - executer les principaux evenements SysEx iMUSE ;
  - gerer la queue de markers classique et la surcouche de triggers explicites facon Sam & Max ;
- un bridge `CImuseShim` pour l'app Swift d'authoring ;
- une app SwiftUI `ImuseAuthoringApp` avec :
  - chargement d'une banque `.ims` ;
  - inspection des evenements iMUSE par son ;
  - preview temps reel avec FluidSynth + soundfont `.sf2` ;
  - transport simple pour tester les hooks, jumps et markers.

Le format et l'outil sont documentes ici :

- [docs/ims-v1.md](/Users/komasami/Dev/scumm-tools/imuse-v6/docs/ims-v1.md)
- [docs/imusepack.md](/Users/komasami/Dev/scumm-tools/imuse-v6/docs/imusepack.md)

## Build rapide

Bibliotheque C++ et outils :

```sh
cmake -S . -B build
cmake --build build
```

App Swift d'authoring :

```sh
cmake -S third_party/fluidsynth-master -B third_party/fluidsynth-build -DCMAKE_PREFIX_PATH=/opt/homebrew -DCMAKE_CXX_STANDARD=17 -DBUILD_SHARED_LIBS=OFF -Denable-libinstpatch=OFF -Denable-libsndfile=OFF -Denable-readline=OFF -Denable-dbus=OFF -Denable-jack=OFF -Denable-pipewire=OFF -Denable-pulseaudio=OFF -Denable-portaudio=OFF -Denable-sdl3=OFF -Denable-oss=OFF -Denable-coremidi=OFF -Denable-ladspa=OFF -Denable-systemd=OFF -Denable-aufile=OFF -Denable-ipv6=OFF -Denable-network=OFF -Denable-floats=ON -Denable-limiter=OFF -Denable-openmp=OFF -Denable-framework=OFF -Dosal=embedded -Denable-coreaudio=ON
cmake --build third_party/fluidsynth-build -j4
swift build
```

Bundle macOS autonome :

```sh
zsh scripts/make-app.sh
```

Le bundle est genere dans `dist/ImuseAuthoringApp.app` avec :

- `openquest-lite.ims`
- `openquest-demo.ims`
- `arachno.sf2`

La source FluidSynth vendoree est dans `third_party/fluidsynth-master`, avec un shim local `gcem.hpp` pour eviter la dependance reseau pendant la configuration.
