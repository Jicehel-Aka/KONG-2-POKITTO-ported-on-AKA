// pokitto_compat/PokittoCore.h — Pokitto::Core + Pokitto::Buttons sur l'AKA.
// Usage reel observe (Kong-II) : PC::buttons.pressed(BTN_X) / .repeat(BTN_X, N)
// (pas les aBtn()/bBtn() du brouillon initial de la spec).
#pragma once
#include <cstdint>

// Constantes de boutons (noms utilises tels quels par le code Kong-II).
// Valeurs arbitraires : seul le nom symbolique compte pour le jeu porte.
enum {
    BTN_UP = 0, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
    BTN_A, BTN_B, BTN_C,
    BTN_COUNT
};

namespace Pokitto {

class Buttons {
public:
    void pollButtons() {}   // le sondage reel se fait dans Core::update() (input_poll)

    bool pressed(uint8_t btn) const;                    // front d'appui (une fois)
    bool repeat(uint8_t btn, uint8_t framesInterval) const;  // ré-appui automatique en appui maintenu
    bool released(uint8_t btn) const;
    bool down(uint8_t btn) const;                        // etat brut (maintenu)

    // Boutons Pokitto d'origine (aBtn/bBtn/... — fournis pour completude de la spec)
    bool aBtn() const;     bool bBtn() const;     bool cBtn() const;
    bool upBtn() const;    bool downBtn() const;
    bool leftBtn() const;  bool rightBtn() const;
};

class Core {
public:
    static void begin();
    static bool update();          // rythme la cadence ; true quand une nouvelle frame est prete
    static bool isRunning();
    static void setFrameRate(uint8_t fps);
    static uint32_t getTime();

    static uint32_t frameCount;
    static Buttons  buttons;        // PC::buttons.pressed(...) — usage reel Kong-II

    // Pokitto::Core::sound est utilise par main.cpp (PC::sound.updateStream()) :
    // no-op ici, le mixage audio AKA tourne en tache de fond (cf audio_game_init()).
    struct SoundStreamStub { void updateStream() {} };
    inline static SoundStreamStub sound;
};

} // namespace Pokitto
