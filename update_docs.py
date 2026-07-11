import re

def update_file(path, is_fr):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    if is_fr:
        content = re.sub(r'(\* `import void iMWrap_LoadSoundFont\(const string filename\);`\s+Raccourci pour charger une SoundFont \(`\.sf2`\))( et configurer)', r'\1 ou `.sf3` compressée\2', content)
        
        sf_dyn = '''
* `import void iMWrap_SetSFDynLoad(int enabled);`
  Active (`1`) ou désactive (`0`) le chargement dynamique pour les SoundFonts `.sf3`. Si activé, les échantillons sont streamés en mémoire à la demande plutôt qu'entièrement décompressés au chargement. Doit être appelé AVANT `iMWrap_LoadSoundFont`.
  ```c
  iMWrap_SetSFDynLoad(1); // Active le streaming dynamique pour le SF3
  iMWrap_LoadSoundFont("$DATA$/music_data/SGM-V2.01.sf3");
  ```
'''
        content = re.sub(r'(\* `import void iMWrap_SetDriver\(int driverType)', sf_dyn + r'\n\1', content)
        
        marker_docs = '''
* `import int iMWrap_PopMarker();`
  Récupère et retire le plus ancien marqueur déclenché (ou valeur de Hook) de la file d'attente. Renvoie `0` si la file est vide.
  ```c
  int marker = iMWrap_PopMarker();
  while (marker > 0) {
      Display("Marqueur %d déclenché !", marker);
      marker = iMWrap_PopMarker();
  }
  ```

* `import int iMWrap_GetLastMarker();`
  Renvoie le marqueur déclenché le plus récent sans vider la file d'attente.

'''
        content = re.sub(r'(## 8\.6\. Informations de Position)', marker_docs + r'\1', content)
    else:
        # English
        content = re.sub(r'(\* `import void iMWrap_LoadSoundFont\(const string filename\);`\s+Shortcut to load a SoundFont \(`\.sf2`\))( and configure)', r'\1 or compressed `.sf3`\2', content)
        
        sf_dyn = '''
* `import void iMWrap_SetSFDynLoad(int enabled);`
  Enables (`1`) or disables (`0`) dynamic loading for `.sf3` SoundFonts. When enabled, samples are streamed into memory on demand rather than entirely decompressed at load time. Must be called BEFORE `iMWrap_LoadSoundFont`.
  ```c
  iMWrap_SetSFDynLoad(1); // Enable dynamic streaming for SF3
  iMWrap_LoadSoundFont("$DATA$/music_data/SGM-V2.01.sf3");
  ```
'''
        content = re.sub(r'(\* `import void iMWrap_SetDriver\(int driverType)', sf_dyn + r'\n\1', content)
        
        marker_docs = '''
* `import int iMWrap_PopMarker();`
  Pops and returns the oldest triggered marker value (or Hook value) from the queue. Returns `0` if the queue is empty.
  ```c
  int marker = iMWrap_PopMarker();
  while (marker > 0) {
      Display("Marker %d triggered!", marker);
      marker = iMWrap_PopMarker();
  }
  ```

* `import int iMWrap_GetLastMarker();`
  Returns the most recent triggered marker without emptying the queue.

'''
        content = re.sub(r'(## 8\.6\. Playback Position Information)', marker_docs + r'\1', content)
        
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)

update_file('D:/Prog/imwrap-v6/docs/imwrap_manual_fr/FR_08_reference_api_ags.md', True)
update_file('D:/Prog/imwrap-v6/docs/imwrap_manual_en/EN_08_ags_api_reference.md', False)
update_file('D:/Prog/imwrap-v6-wiki/FR_08_reference_api_ags.md', True)
update_file('D:/Prog/imwrap-v6-wiki/EN_08_ags_api_reference.md', False)
