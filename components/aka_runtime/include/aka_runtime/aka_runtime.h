// aka_runtime/aka_runtime.h — Socle commun a tous les portages AKA Port Studio.
// V0.1 (fondation) : cycle de vie, chemins SD, capture ecran, retour loader.
// Le menu systeme complet (Volume/Langue/Infos/Credits/Licence) et les
// notifications visuelles sont prevus pour une prochaine iteration (cf.
// RUNTIME_SPEC.md) — non encore implementes ici.
#pragma once
#include <cstdint>

struct Keys;   // core/input.h

class AkaRuntime {
public:
    void begin(const char* gameId);   // monte la SD, cree /sdcard/<gameId>/, charge settings.json
    // A appeler une fois par frame ; renvoie true si le jeu doit continuer,
    // false si le menu systeme a pris la main ce tour-ci (le jeu ne doit pas
    // lire les boutons ni dessiner dans ce cas).
    bool update(const Keys& k);

    const char* gamePath() const;         // "/sdcard/<gameId>"
    const char* settingsPath() const;     // "/sdcard/AKA/settings.json"
    const char* screenshotPath() const;   // "/sdcard/AKA/screenshots"

    bool takeScreenshot();
    void returnToLoader();

    uint8_t getMusicVolume() const;
    uint8_t getSfxVolume()   const;
    void    setMusicVolume(uint8_t v);
    void    setSfxVolume(uint8_t v);

private:
    char m_gameId[32]   = {0};
    char m_gamePath[64] = {0};
};

extern AkaRuntime akaRuntime;
