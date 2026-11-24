#!/usr/bin/env python3
"""Prépare dist/ pour la mise à jour directe : version.txt, Themes.zip et manifest.txt.
Usage : python3 tools/make_dist.py <version>   (les exécutables WinDock_x64/x86.exe doivent déjà être dans dist/)"""
import os, shutil, sys, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, "dist")
ver = sys.argv[1] if len(sys.argv) > 1 else open(os.path.join(DIST, "version.txt")).read().strip()
open(os.path.join(DIST, "version.txt"), "w").write(ver + "\n")

# copies sous l'ancien nom : migration des installations RocketDockModern_x*.exe
for a in ("x64", "x86"):
    shutil.copyfile(os.path.join(DIST, f"WinDock_{a}.exe"), os.path.join(DIST, f"RocketDockModern_{a}.exe"))

# une seule archive pour tous les thèmes (1 requête au lieu de plusieurs centaines)
zp = os.path.join(DIST, "Themes.zip")
with zipfile.ZipFile(zp, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for root, _, files in os.walk(os.path.join(ROOT, "Themes")):
        for f in sorted(files):
            full = os.path.join(root, f)
            z.write(full, os.path.relpath(full, ROOT).replace(os.sep, "/"))

lines = ["# Fichiers remplacés par la mise à jour (chemins relatifs au dossier de l'exe).",
         "# Settings.ini n'est jamais remplacé. Themes.zip est décompressé dans le dossier de l'application.",
         "WinDock_x64.exe", "WinDock_x86.exe", "RocketDockModern_x64.exe", "RocketDockModern_x86.exe", "Themes.zip"]
open(os.path.join(DIST, "manifest.txt"), "w", encoding="utf-8").write("\n".join(lines) + "\n")
print("dist prêt :", ver, os.path.getsize(zp), "octets de thèmes")
