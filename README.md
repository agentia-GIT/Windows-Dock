# Windows-Dock (RocketDock Modern 2.0)

Dock de lanceur d'applications pour Windows 10 / 11, réécrit en C++ natif (Win32 + GDI+) à partir de
l'idée de RocketDock 1.3.5 (abandonné, sans code source). Un seul `.exe`, aucune DLL à fournir.

- Barre en verre translucide, coins arrondis, ombre douce, zoom fluide ancré sur le curseur
- Thèmes Sombre / Clair / Aurore, tuiles d'icônes modernes (police Segoe Fluent Icons)
- Pastille sous les applications ouvertes, clic = mise au premier plan
- Glisser-déposer de fichiers / dossiers / raccourcis, réordonnancement des icônes
- Position haut / bas / gauche / droite, masquage automatique, raccourci `Ctrl+Alt+R`
- Haute définition (DPI par écran), français / anglais selon Windows

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
