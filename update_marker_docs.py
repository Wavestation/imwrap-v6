import re

def update_file(path, is_fr):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    # The block to find and replace in French
    old_fr = r'''\* `import int iMWrap_PopMarker\(\);`
  Récupère et retire le plus ancien marqueur déclenché \(ou valeur de Hook\) de la file d'attente\. Renvoie `0` si la file est vide\.
  ```c
  int marker = iMWrap_PopMarker\(\);
  while \(marker > 0\) {
      Display\("Marqueur %d déclenché !", marker\);
      marker = iMWrap_PopMarker\(\);
  }
  ```

\* `import int iMWrap_GetLastMarker\(\);`
  Renvoie le marqueur déclenché le plus récent sans vider la file d'attente\.'''

    new_fr = '''* `import int iMWrap_PopMarker();`
  Récupère et retire le plus ancien marqueur déclenché (ou valeur de Hook) de la file d'attente. Renvoie `-1` si la file est vide. 
  La valeur renvoyée "pacte" (pack) à la fois l'ID du son (sur les bits supérieurs) et la valeur du marqueur (sur les 8 bits inférieurs).
  Voici comment dépacquer ces valeurs :
  ```c
  int packed = iMWrap_PopMarker();
  while (packed != -1) {
      int markerValue = packed & 0xFF;         // Les 8 bits de poids faible
      int soundId = (packed >> 8) & 0xFFFFFF;  // Le reste des bits pour le soundId
      
      Display("Marqueur %d déclenché par le son %d !", markerValue, soundId);
      packed = iMWrap_PopMarker();
  }
  ```

* `import int iMWrap_GetLastMarker();`
  Renvoie le marqueur déclenché le plus récent sans vider la file d'attente. (La valeur est pacquée de la même manière que `PopMarker`, ou vaut `-1` si vide).'''

    old_en = r'''\* `import int iMWrap_PopMarker\(\);`
  Pops and returns the oldest triggered marker value \(or Hook value\) from the queue\. Returns `0` if the queue is empty\.
  ```c
  int marker = iMWrap_PopMarker\(\);
  while \(marker > 0\) {
      Display\("Marker %d triggered!", marker\);
      marker = iMWrap_PopMarker\(\);
  }
  ```

\* `import int iMWrap_GetLastMarker\(\);`
  Returns the most recent triggered marker without emptying the queue\.'''

    new_en = '''* `import int iMWrap_PopMarker();`
  Pops and returns the oldest triggered marker value (or Hook value) from the queue. Returns `-1` if the queue is empty.
  The returned value "packs" both the sound ID (on the upper bits) and the marker value (on the lower 8 bits).
  Here is how to unpack these values:
  ```c
  int packed = iMWrap_PopMarker();
  while (packed != -1) {
      int markerValue = packed & 0xFF;         // The lower 8 bits
      int soundId = (packed >> 8) & 0xFFFFFF;  // The remaining bits for the soundId
      
      Display("Marker %d triggered by sound %d!", markerValue, soundId);
      packed = iMWrap_PopMarker();
  }
  ```

* `import int iMWrap_GetLastMarker();`
  Returns the most recent triggered marker without emptying the queue. (The value is packed the same way as `PopMarker`, or returns `-1` if empty).'''

    if is_fr:
        content = re.sub(old_fr, new_fr, content)
    else:
        content = re.sub(old_en, new_en, content)

    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)

update_file('D:/Prog/imwrap-v6/docs/imwrap_manual_fr/FR_08_reference_api_ags.md', True)
update_file('D:/Prog/imwrap-v6/docs/imwrap_manual_en/EN_08_ags_api_reference.md', False)
update_file('D:/Prog/imwrap-v6-wiki/FR_08_reference_api_ags.md', True)
update_file('D:/Prog/imwrap-v6-wiki/EN_08_ags_api_reference.md', False)
