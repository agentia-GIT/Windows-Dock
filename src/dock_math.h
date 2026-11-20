// dock_math.h - Calculs de mise en page et de zoom du dock (sans dépendance Windows).
// Toutes les valeurs sont en pixels, le long de l'axe du dock (axe "u").
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace dm {

inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// Cloche en cosinus : 1 au centre, 0 à |t| >= 1.
inline float bell(float t) {
    t = std::fabs(t);
    if (t >= 1.0f) return 0.0f;
    return 0.5f * (1.0f + std::cos(3.14159265f * t));
}

struct Layout {
    std::vector<float> left;   // bord gauche de chaque élément (relatif au début du dock)
    std::vector<float> width;  // largeur de chaque élément
    float total = 0.0f;        // longueur totale (sans marge)
};

// Layout "repos" : icônes de taille s0, séparateurs de largeur sepW.
inline Layout makeBase(const std::vector<bool>& sep, float s0, float sepW, float gap) {
    Layout b;
    float x = 0.0f;
    for (size_t i = 0; i < sep.size(); ++i) {
        float w = sep[i] ? sepW : s0;
        b.left.push_back(x);
        b.width.push_back(w);
        x += w + gap;
    }
    b.total = sep.empty() ? 0.0f : x - gap;
    return b;
}

// Layout réel à partir des tailles animées courantes.
inline Layout makeReal(const std::vector<bool>& sep, const std::vector<float>& sizes, float sepW, float gap) {
    Layout r;
    float x = 0.0f;
    for (size_t i = 0; i < sep.size(); ++i) {
        float w = sep[i] ? sepW : sizes[i];
        r.left.push_back(x);
        r.width.push_back(w);
        x += w + gap;
    }
    r.total = sep.empty() ? 0.0f : x - gap;
    return r;
}

// Tailles cibles : l'icône la plus proche du curseur (coordonnées du layout repos) est la plus grande.
// radiusSlots = rayon d'influence exprimé en nombre d'icônes.
inline void zoomTargets(const Layout& base, const std::vector<bool>& sep, float s0, float sMax,
                        float radiusSlots, float gap, float cursorBase, bool active,
                        std::vector<float>& out) {
    out.assign(sep.size(), s0);
    if (!active) return;
    float pitch = s0 + gap;
    float radius = std::max(0.5f, radiusSlots) * pitch;
    for (size_t i = 0; i < sep.size(); ++i) {
        if (sep[i]) continue;
        float center = base.left[i] + base.width[i] * 0.5f;
        float k = bell((cursorBase - center) / radius);
        out[i] = lerpf(s0, sMax, k);
    }
}

// Convertit une position du curseur (layout repos) vers le layout réel (relatif au début réel).
// Chaque élément + son espacement forme une "unité" qui est interpolée linéairement.
inline float mapCursor(const Layout& base, const Layout& real, float gap, float cursorBase) {
    size_t n = base.left.size();
    if (n == 0) return cursorBase;
    if (cursorBase <= base.left[0]) return real.left[0] + (cursorBase - base.left[0]);
    for (size_t k = 0; k < n; ++k) {
        float unitStart = base.left[k];
        float unitLen = base.width[k] + gap;
        if (cursorBase < unitStart + unitLen) {
            float f = (cursorBase - unitStart) / unitLen;
            return real.left[k] + f * (real.width[k] + gap);
        }
    }
    size_t k = n - 1;
    float baseEnd = base.left[k] + base.width[k] + gap;
    return real.left[k] + real.width[k] + gap + (cursorBase - baseEnd);
}

// Index de l'élément sous la position u (relative au début du layout), -1 si aucun.
inline int hitIndex(const Layout& l, float gap, float u) {
    for (size_t i = 0; i < l.left.size(); ++i) {
        if (u >= l.left[i] - gap * 0.5f && u < l.left[i] + l.width[i] + gap * 0.5f) return (int)i;
    }
    return -1;
}

// Index d'insertion le plus proche (pour le glisser-déposer / réordonnancement), 0..n.
inline int nearestSlot(const Layout& l, float u) {
    int best = 0;
    float bestD = 1e30f;
    for (size_t i = 0; i < l.left.size(); ++i) {
        float c = l.left[i] + l.width[i] * 0.5f;
        float d = std::fabs(u - c);
        if (d < bestD) { bestD = d; best = (int)i; }
    }
    return best;
}

}  // namespace dm
