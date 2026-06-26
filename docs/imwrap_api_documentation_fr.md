# Documentation détaillée de l'API C++ iMWrap v6

Ce document référence exhaustivement les méthodes publiques des classes principales du moteur iMWrap v6 (`ResourceBank`, `MidiSink` et `IMWrapEngine`), avec des exemples d'utilisation pour chacune.

---

## 1. Classe `imwrap::ResourceBank`

La classe `ResourceBank` gère la mémoire et le chargement des fichiers `.ims` (Interactive Music System).

### `bool openFromFile(const std::string &path, std::string *error = nullptr)`
Charge une banque musicale depuis un chemin sur le disque.
- **Paramètres** : 
  - `path` : Le chemin vers le fichier `.ims`.
  - `error` (optionnel) : Pointeur vers une chaîne de caractères pour récupérer un message d'erreur.
- **Retourne** : `true` si le chargement a réussi, sinon `false`.
- **Exemple** :
```cpp
imwrap::ResourceBank bank;
std::string err;
if (!bank.openFromFile("data/music.ims", &err)) {
    std::cerr << "Erreur de chargement: " << err << std::endl;
}
```

### `bool openFromMemory(std::vector<uint8_t> data, std::string *error = nullptr)`
Charge une banque musicale depuis un buffer mémoire (utile si vos assets sont empaquetés/compressés).
- **Paramètres** : 
  - `data` : Vecteur d'octets contenant la structure du fichier `.ims`.
  - `error` (optionnel) : Pointeur vers une chaîne pour l'erreur.
- **Retourne** : `true` en cas de succès.
- **Exemple** :
```cpp
std::vector<uint8_t> myData = LoadMyVFSFile("music.ims");
bank.openFromMemory(myData);
```

### `bool isOpen() const`
Vérifie si une banque valide est actuellement chargée.
- **Retourne** : `true` si ouverte.
- **Exemple** :
```cpp
if (bank.isOpen()) { std::cout << "Banque prête!" << std::endl; }
```

### `bool hasSound(uint16_t soundId) const`
Vérifie si un numéro de son spécifique existe dans la banque.
- **Paramètres** : `soundId` : Identifiant iMUSE du son.
- **Exemple** :
```cpp
if (bank.hasSound(10)) { engine.startSound(10); }
```

### `std::vector<uint16_t> soundIds() const`
Récupère la liste de tous les identifiants de sons disponibles dans cette banque.
- **Retourne** : Un vecteur d'ID (`uint16_t`).
- **Exemple** :
```cpp
for (uint16_t id : bank.soundIds()) {
    std::cout << "Son disponible: " << id << std::endl;
}
```

---

## 2. Interface `imwrap::MidiSink`

Cette interface virtuelle est utilisée par le moteur pour "cracher" les événements MIDI à jouer. Vous **devez** en dériver une classe pour le lier à votre backend audio.

### `virtual void onMidiMessage(uint16_t soundId, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) = 0`
**Obligatoire.** Appelée pour chaque instruction MIDI conventionnelle (Note On, Note Off, Control Change, Pitch Bend...).
- **Paramètres** :
  - `soundId` : Le son qui a émis l'instruction.
  - `status` : L'octet de statut MIDI (ex: `0x90` pour Note On sur canal 0).
  - `data1` : Premier octet de donnée (ex: la note, de 0 à 127).
  - `hasData2` : Indique si la commande a un deuxième octet de donnée.
  - `data2` : Deuxième octet de donnée (ex: la vélocité).
- **Exemple** :
```cpp
void onMidiMessage(uint16_t sid, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) override {
    uint32_t msg = status | (data1 << 8);
    if (hasData2) msg |= (data2 << 16);
    midiOutShortMsg(hMidiOut, msg); // Exemple WinMM API
}
```

### `virtual void onSysEx(uint16_t soundId, ByteView message)`
Appelée lorsqu'un événement System Exclusive (SysEx) long est déclenché (très utilisé sur MT-32 ou pour initialiser un synthé).
- **Exemple** :
```cpp
void onSysEx(uint16_t soundId, imwrap::ByteView message) override {
    // message.data() et message.size() pour envoyer le buffer MIDI SysEx
    SendSysExBufferToHardware(message.data(), message.size());
}
```

### `virtual void onTempoChange(uint16_t soundId, uint32_t microsPerQuarter)`
Appelée lorsque le moteur signale un changement de tempo (généralement issu du fichier SMF interne).
- **Exemple** :
```cpp
void onTempoChange(uint16_t soundId, uint32_t microsPerQuarter) override {
    double bpm = 60000000.0 / microsPerQuarter;
    std::cout << "Le BPM passe à : " << bpm << std::endl;
}
```

### `virtual void onAllNotesOff()`
Appelée lorsque le moteur souhaite couper de force le son (ex: lors d'un `stopAllSounds()`). Pratique pour envoyer une commande "Panic" à un synthétiseur.

---

## 3. Classe `imwrap::IMWrapEngine`

Le cœur du système. C'est l'instance qui gère le temps, la priorité des sons, et exécute la logique interactive.

### Initialisation et Configurations

#### `explicit IMWrapEngine(const ResourceBank *bank = nullptr)`
#### `void setResourceBank(const ResourceBank *bank)`
#### `void setMidiSink(MidiSink *sink)`
Associe le moteur à une banque de ressources et une interface de sortie.
- **Exemple** :
```cpp
imwrap::IMWrapEngine engine;
engine.setResourceBank(&myBank);
engine.setMidiSink(&mySynth);
```

#### `void setTargetProfile(TargetProfile profile)`
Configure le format de sortie cible. Utile pour adapter les événements aux standards du matériel.
- **Paramètres** : `profile` : `TargetProfile::GeneralMidi`, `TargetProfile::RolandMT32`, etc.
- **Exemple** :
```cpp
engine.setTargetProfile(imwrap::TargetProfile::GeneralMidi);
```

#### `void setLogCallback(LogCallback cb)`
#### `void setMarkerCallback(MarkerCallback cb)`
Enregistre des fonctions anonymes (lambdas) pour capturer des événements textuels (logs) ou musicaux (marqueurs MIDI iMUSE).
- **Exemple** :
```cpp
engine.setMarkerCallback([](uint16_t soundId, uint8_t marker) {
    std::cout << "Le son " << soundId << " a atteint le marqueur " << (int)marker << " !\n";
    // Parfait pour synchroniser une animation graphique avec la musique.
});
```

### Contrôle de la lecture

#### `bool startSound(uint16_t soundId)`
Lance ou planifie la lecture d'un son.
- **Retourne** : `true` si le son a été trouvé et initié.
- **Exemple** : `engine.startSound(15);`

#### `void stopSound(uint16_t soundId)`
Arrête un son en douceur (en générant les Note Off appropriés).
- **Exemple** : `engine.stopSound(15);`

#### `void stopAllSounds()`
Arrête l'intégralité de la musique en cours.

#### `bool isSoundActive(uint16_t soundId) const`
Vérifie si un son tourne toujours.
- **Exemple** : 
```cpp
if (engine.isSoundActive(10)) { /* La musique d'intro tourne encore */ }
```

#### `int getSoundStatus(uint16_t soundId) const`
Renvoie un entier décrivant le statut spécifique du son (0 si inactif, 1 si actif... utile pour un wrapper haut niveau).

#### `bool getPlaybackLocation(uint16_t soundId, uint16_t *track, uint16_t *beat, uint16_t *tick) const`
Récupère la position précise de la tête de lecture. Très utile pour le debug ou des affichages "Now Playing".
- **Exemple** :
```cpp
uint16_t trk, beat, tick;
if (engine.getPlaybackLocation(10, &trk, &beat, &tick)) {
    printf("Position : Piste %d, Temps %d, Tick %d\n", trk, beat, tick);
}
```

### Gestion du Temps

#### `double transportTicksPerSecond() const`
Récupère la résolution du temps du moteur (nombre de "ticks" de séquenceur théoriques qui doivent passer en une seconde). En général, ce sera basé sur le format Standard MIDI (ex: 60 ou 120 Hz).
- **Exemple** : `double ticksPerSec = engine.transportTicksPerSecond();`

#### `void advanceAll(uint32_t deltaTicks)`
Fait avancer l'horloge principale du séquenceur pour tous les sons. **C'est la fonction la plus importante à appeler dans votre boucle principale.**
- **Exemple** :
```cpp
// Si la frame dure 0.016s et que le transportTick est à 120Hz :
// deltaTicks = 0.016 * 120 = 1.92 -> cast à 2 ticks.
engine.advanceAll(2);
```

### Commandes Interactives (iMUSE Protocol)

#### `int32_t doCommand(uint16_t argc, const int16_t *args)`
Envoie une instruction arbitraire d'interaction iMUSE au séquenceur. Les codes sont dans `imwrap::Command`.

**Exemple 1 : Changer le volume d'une piste spécifique d'un son**
(Syntaxe iMUSE : Set Volume)
```cpp
int16_t args[] = { 
    (int16_t)imwrap::Command::PlayerSetPartVolume, 
    10,    // soundId
    0,     // channel (0 = piste 1)
    100    // Nouveau volume (0-127)
};
engine.doCommand(4, args);
```

**Exemple 2 : Sauter vers un marqueur rythmique (Jump)**
Sauter au "beat 10" de la "track 0".
```cpp
int16_t args[] = {
    (int16_t)imwrap::Command::PlayerJump,
    10,    // soundId
    0,     // track
    10,    // beat
    0      // tick
};
engine.doCommand(5, args);
```

**Exemple 3 : Placer un "Hook" (fondu ou action conditionnelle)**
Déclenche une commande iMUSE automatiquement lorsque la séquence musicale atteint tel endroit.
```cpp
// Les arguments dépendent du payload du Hook iMUSE.
int16_t args[] = { (int16_t)imwrap::Command::PlayerSetHook, /* ... */ };
engine.doCommand(argc, args);
```

### Sauvegarde et Restauration (Save states)

#### `bool Serialize(std::ostream &os) const`
#### `bool Deserialize(std::istream &is)`
Sauvegarde l'état exact du moteur (notes jouées, états des commandes, séquenceurs) dans un flux binaire, ou le restaure. Indispensable pour implémenter un système de sauvegarde "Save/Load" dans un jeu.
- **Exemple** :
```cpp
std::ofstream saveFile("savegame.bin", std::ios::binary);
engine.Serialize(saveFile);

// Plus tard :
std::ifstream loadFile("savegame.bin", std::ios::binary);
engine.Deserialize(loadFile);
```
