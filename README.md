# Win-Dock (ex Win-Dock)

Dock de lanceur d'applications pour Windows 10 / 11, réécrit en C++ natif (Win32 + GDI+) à partir de
l'idée de RocketDock 1.3.5 (abandonné, sans code source). Un seul `.exe`, aucune DLL à fournir.

- Barre en verre translucide, coins arrondis, ombre douce, zoom fluide ancré sur le curseur
- **21 thèmes** avec leurs propres icônes (dossier `Themes/`) : Classique (Dark, Light), Informatique (Terminal, Circuit, Silicon, Matrix, Blueprint, Cyberpunk), IA (Neural, Quantum, Aurora, Cortex, Nebula, Oracle), Games (NeonArcade, RetroPixel, Lava, GameBoy, Frostbite, CandyPop, Emerald)
- **Dock déplaçable** : glisser le fond de la barre (ou Alt + glisser) ; près d'un bord il s'ancre, ailleurs il flotte librement sur le bureau (menu Position > Libre, verrou de position)
- **Glisser-déposer** de raccourcis, programmes et dossiers depuis le bureau ou l'Explorateur, avec marqueur d'insertion
- Pastille sous les applications ouvertes, clic = mise au premier plan
- Glisser-déposer de fichiers / dossiers / raccourcis, réordonnancement des icônes
- Position haut / bas / gauche / droite, masquage automatique, raccourci `Ctrl+Alt+R`
- Haute définition (DPI par écran), français / anglais selon Windows

## Fenêtre Paramètres (clic droit > Paramètres…)
Quatre onglets, tous les changements s'appliquent immédiatement et sont enregistrés dans `Settings.ini` :
- **Thèmes** : galerie avec aperçu de chaque thème (barre + icônes), un clic pour l'appliquer ; la fenêtre Paramètres reprend les couleurs du thème
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
Lancer `dist/WinDock_x64.exe` (ou `_x86` en 32 bits). Au premier lancement, un `Settings.ini` est créé
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


## Thèmes (v2.2.0)

Chaque thème est un dossier `Themes/<Nom>/` contenant `theme.ini` et `Icons/*.png` (19 icônes : computer, folder, documents, downloads, pictures, music, video, recycle, recycle_full, settings, terminal, browser, mail, network, code, games, ai, chat, app).

Pour créer le vôtre : copiez un dossier existant, modifiez `theme.ini` (couleurs `AARRGGBB`, `CornerRadius`, `BorderWidth`, `Glow`, `TileShape` = squircle / circle / square / hex, palette des tuiles) et remplacez les PNG. Dans Settings.ini ou le menu, une icône `theme:folder` prend l'image du thème actif. `tools/make_themes.py` régénère les thèmes fournis (`python3 tools/make_themes.py Themes`, nécessite Pillow et numpy).


## Renommage (v2.3.0)

L'application s'appelle désormais **Win-Dock** (`WinDock_x64.exe` / `WinDock_x86.exe`). `Settings.ini` reste inchangé et compatible.
Les copies `RocketDockModern_x*.exe` de `dist/` ne servent qu'à migrer les anciennes installations lors de leur mise à jour directe.

## Mise à jour directe

`dist/` contient `version.txt`, `manifest.txt`, les exécutables et `Themes.zip` (tous les thèmes en une archive, décompressée avec `tar.exe` ou PowerShell). Après avoir recompilé : `python3 tools/make_dist.py <version>`.
Le menu clic droit s'ouvre à côté de la zone des icônes pour ne jamais les recouvrir.
