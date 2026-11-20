# Windows-Dock (RocketDock Modern 2.0)

Dock de lanceur d'applications pour Windows 10 / 11, réécrit en C++ natif (Win32 + GDI+) à partir de
l'idée de RocketDock 1.3.5 (abandonné, sans code source). Un seul `.exe`, aucune DLL à fournir.

- Barre en verre translucide, coins arrondis, ombre douce, zoom fluide ancré sur le curseur
- Thèmes Sombre / Clair / Aurore, tuiles d'icônes modernes (police Segoe Fluent Icons)
- Pastille sous les applications ouvertes, clic = mise au premier plan
- Glisser-déposer de fichiers / dossiers / raccourcis, réordonnancement des icônes
- Position haut / bas / gauche / droite, masquage automatique, raccourci `Ctrl+Alt+R`
- Haute définition (DPI par écran), français / anglais selon Windows

## Fenêtre Paramètres (clic droit > Paramètres…)
Quatre onglets, tous les changements s'appliquent immédiatement et sont enregistrés dans `Settings.ini` :
- **Apparence** : position, thème, écran, police, tailles, zoom, décalage, opacités
- **Comportement** : masquage auto, noms, verrouillage, applications ouvertes, démarrage avec Windows, raccourci clavier
- **Éléments** : ajouter / supprimer / réordonner, modifier titre, commande, arguments, dossier, icône
- **Mise à jour** : bouton « Mettre à jour maintenant »

### Mise à jour directe
Télécharge `dist/version.txt` et `dist/manifest.txt` depuis ce dépôt, puis remplace l'exécutable et les fichiers listés
dans `manifest.txt` (chemins relatifs, sous-dossiers créés si besoin), puis redémarre le dock. **`Settings.ini` n'est jamais
modifié.** L'ancien exécutable est conservé le temps du remplacement (`.old`), avec retour arrière en cas d'échec.
Pour publier une version : compiler, copier les exe dans `dist/`, incrémenter `dist/version.txt`, pousser sur `main`.
Clés optionnelles de `Settings.ini` : `UpdateUrl` (autre source) et `UpdateToken` (dépôt privé).

## Utilisation
Lancer `dist/RocketDockModern_x64.exe` (ou `_x86` en 32 bits). Au premier lancement, un `Settings.ini` est créé
à côté de l'exe. Clic droit sur le dock pour tout régler. Le format du `Settings.ini` reste compatible avec RocketDock.

## Compilation
```
pip install ziglang
./build.sh        # Linux / macOS
build.bat         # Windows
```
Aucun Visual Studio requis. Tests du calcul de zoom : `g++ -std=c++17 tests/test_math.cpp && ./a.out`

## Structure
- `src/main.cpp` : fenêtre, rendu GDI+, icônes, menus, animation
- `src/dock_math.h` : mise en page et zoom (sans dépendance Windows, testé)
- `resources/` : manifeste (DPI), icône, informations de version

> État : compilé et testé pour la partie calcul ; le rendu n'a pas encore été validé sur une machine Windows.
