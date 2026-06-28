# Plugin Éditeur AGS

Ce dossier contient un projet .NET séparé pour un plugin d'éditeur AGS 3.6.2.

## Fonctionnalités Actuelles

- ajoute un menu `iMWrap` dans l'éditeur AGS
- ajoute un nœud racine `Banks` dans l'arbre du projet
- détecte les fichiers `.ims` du projet et les liste dans cet arbre
- ouvre l'éditeur IMS dans un onglet de document
- affiche les sons, les pistes, et les événements de contrôle de la banque sélectionnée
- permet de créer, renommer, et supprimer des banques `.ims` depuis l'arbre
- réutilise le wrapper C# existant dans `ags-wrapper/IMWrapShim.cs`

## Build

Le projet attend `AGS.Types.dll` depuis un dossier local d'installation de l'éditeur AGS.

Propriétés supportées :

- `AGSReferenceDir`
- `AGS_TYPES_DIR`
- `AGS_EDITOR_DIR`

Exemple :

```powershell
dotnet msbuild .\AGS.Plugin.IMWrap.Editor.csproj /p:Configuration=Release /p:AGSReferenceDir=C:\Program Files (x86)\Adventure Game Studio
```

Le projet cible maintenant `.NET Framework 4.8` afin de pouvoir compiler sur des machines Windows modernes qui n'embarquent pas forcément le targeting pack 4.6 par défaut.

## Installation Dans AGS

1. Copiez `AGS.Plugin.IMWrap.Editor.dll` dans le dossier de l'éditeur AGS.
2. Copiez `imwrap_shim.dll` et ses dépendances natives à côté si elles ne sont pas déjà présentes.
3. Assurez-vous que l'architecture de la DLL native correspond à celle du processus de l'éditeur AGS.

AGS charge automatiquement les assemblies dont le nom de fichier commence par `AGS.Plugin.`

## Notes

- Ce plugin éditeur est distinct du plugin runtime AGS déjà présent dans `src/`.
- Le runtime natif sait déjà charger des banques `.ims` depuis des données de jeu packagées.
- L'éditeur IMS actuel reste volontairement minimal et centré sur l'inspection et la gestion de l'arbre avant d'ajouter lecture, import/export, ou actions de build.
