import re

def update_file(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    # The block to find and replace
    old_code = r'''  function launchGameWithMidi\(choice\) {
    window\.imwrapMidiChoice = choice;
    midiModal\.style\.display = "none";
    Module\.callMain\(\['/' \+ \(game_file\.name \|\| game_file\), '--windowed'\]\);
  }'''

    new_code = '''  function launchGameWithMidi(choice) {
    window.imwrapMidiChoice = choice;
    midiModal.style.display = "none";
    if (choice === 0) {
        bcupElement.className = "bluecup levitate";
        bcupElement.hidden = false;
        bcupElement.style.display = "block";
        var loadingText = lang.startsWith("fr") ? "Décompression de la banque de sons, patience..." :
                          lang.startsWith("es") ? "Descomprimiendo banco de sonidos, por favor espere..." :
                          "Decompressing soundbank, please wait...";
        writeMsgOnCanvas(loadingText);
        setTimeout(function() {
            bcupElement.hidden = true;
            bcupElement.style.display = "none";
            writeMsgOnCanvas("");
            Module.callMain(['/' + (game_file.name || game_file), '--windowed']);
        }, 50);
    } else {
        Module.callMain(['/' + (game_file.name || game_file), '--windowed']);
    }
  }'''

    content = re.sub(old_code, new_code, content)

    # Some minified versions might exist in the generated ags.html
    old_code_min = r'''function launchGameWithMidi\(e\){window\.imwrapMidiChoice=e;midiModal\.style\.display="none";Module\.callMain\(\["/"\+\(game_file\.name\|\|game_file\),"--windowed"\]\)}'''
    new_code_min = '''function launchGameWithMidi(e){window.imwrapMidiChoice=e;midiModal.style.display="none";if(e===0){bcupElement.className="bluecup levitate";bcupElement.hidden=!1;bcupElement.style.display="block";var loadingText=lang.startsWith("fr")?"Décompression de la banque de sons, patience...":lang.startsWith("es")?"Descomprimiendo banco de sonidos, por favor espere...":"Decompressing soundbank, please wait...";writeMsgOnCanvas(loadingText);setTimeout(function(){bcupElement.hidden=!0;bcupElement.style.display="none";writeMsgOnCanvas("");Module.callMain(["/"+(game_file.name||game_file),"--windowed"])},50);}else{Module.callMain(["/"+(game_file.name||game_file),"--windowed"]);}}'''

    content = re.sub(old_code_min, new_code_min, content)

    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)

update_file('D:/Prog/imwrap-ags-web/ags/Emscripten/launcher_index.html')
update_file('D:/Prog/imwrap-ags-web/ags/build-web-release/ags.html')
update_file('D:/Prog/imwrap-v6/final-release/imwrap-windows/web-export/ags.html')
