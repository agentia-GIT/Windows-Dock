// Tests unitaires de dock_math.h (compilables sous Linux/Windows).
#include <cstdio>
#include <cstdlib>
#include "../src/dock_math.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #c); ++fails; } } while (0)

int main() {
    const float s0 = 44, sMax = 88, gap = 7, sepW = 12;
    std::vector<bool> sep = {false, false, true, false, false, false, true, false};
    dm::Layout base = dm::makeBase(sep, s0, sepW, gap);
    CHECK(base.left.size() == sep.size());
    CHECK(std::fabs(base.left[0]) < 1e-4f);

    // Inactif : tout reste à s0, layout réel == base
    std::vector<float> t;
    dm::zoomTargets(base, sep, s0, sMax, 3, gap, 100, false, t);
    for (size_t i = 0; i < t.size(); ++i) CHECK(t[i] == s0);

    // Actif au centre de l'icône 4 : cette icône est au maximum, les lointaines au minimum
    float c4 = base.left[4] + base.width[4] * 0.5f;
    dm::zoomTargets(base, sep, s0, sMax, 3, gap, c4, true, t);
    CHECK(std::fabs(t[4] - sMax) < 1e-3f);
    CHECK(t[4] >= t[3] && t[4] >= t[5]);
    CHECK(t[0] < t[4]);
    for (size_t i = 0; i < t.size(); ++i) { CHECK(t[i] >= s0 - 1e-3f && t[i] <= sMax + 1e-3f); }

    // mapCursor : identité quand le layout réel == base
    dm::Layout real0 = dm::makeReal(sep, std::vector<float>(sep.size(), s0), sepW, gap);
    for (float x = -20; x < base.total + 40; x += 7.3f)
        CHECK(std::fabs(dm::mapCursor(base, real0, gap, x) - x) < 1e-3f);

    // mapCursor monotone et continu avec un layout zoomé
    dm::Layout real = dm::makeReal(sep, t, sepW, gap);
    float prev = -1e9f;
    for (float x = -30; x < base.total + 60; x += 0.5f) {
        float p = dm::mapCursor(base, real, gap, x);
        CHECK(p >= prev - 1e-3f);
        if (prev > -1e8f) CHECK(p - prev < 4.0f);  // pas de saut
        prev = p;
    }
    // Le zoom agrandit le dock
    CHECK(real.total > base.total);

    // hitIndex / nearestSlot
    CHECK(dm::hitIndex(real, gap, real.left[3] + 1) == 3);
    CHECK(dm::hitIndex(real, gap, -100) == -1);
    CHECK(dm::nearestSlot(real, real.left[5] + real.width[5] * 0.5f) == 5);

    // Dock vide
    std::vector<bool> none;
    dm::Layout e = dm::makeBase(none, s0, sepW, gap);
    CHECK(e.total == 0.0f);
    CHECK(dm::mapCursor(e, e, gap, 12.0f) == 12.0f);

    if (fails == 0) std::printf("OK : tous les tests passent\n");
    return fails ? 1 : 0;
}
