#include "pokitto_compat/PokittoCore.h"
#include "pokitto_compat/PokittoDisplay.h"
#include "pokitto_compat/PokittoSound.h"
#include "core/input.h"
#include "gb_ll_common.h"   // EXPANDER_KEY_*
#include "esp_timer.h"

namespace Pokitto {

uint32_t Core::frameCount = 0;
bool Core::suppressPresent = false;
Buttons  Core::buttons;

static uint8_t  s_fps = 60;
static uint32_t s_lastFrameMs = 0;
static bool     s_firstFrame = true;

static inline uint32_t now_ms() { return (uint32_t)(esp_timer_get_time() / 1000); }
static inline uint32_t btn_mask(uint8_t b) {
    switch (b) {
        case BTN_UP:    return EXPANDER_KEY_UP;
        case BTN_DOWN:  return EXPANDER_KEY_DOWN;
        case BTN_LEFT:  return EXPANDER_KEY_LEFT;
        case BTN_RIGHT: return EXPANDER_KEY_RIGHT;
        case BTN_A:     return EXPANDER_KEY_A;
        case BTN_B:     return EXPANDER_KEY_B;
        case BTN_C:     return EXPANDER_KEY_C;
        default:        return 0;
    }
}

bool Buttons::pressed(uint8_t btn)  const { return g_keys.pressed  & btn_mask(btn); }
bool Buttons::released(uint8_t btn) const { return g_keys.released & btn_mask(btn); }
bool Buttons::down(uint8_t btn)     const { return g_keys.raw      & btn_mask(btn); }

bool Buttons::repeat(uint8_t btn, uint8_t framesInterval) const {
    // BUG TROUVE ET CORRIGE : l'ancienne version approximait la repetition
    // avec un modulo sur le temps maintenu, ce qui ne renvoyait "vrai" qu'une
    // fraction du temps (~25%) meme avec framesInterval=1 -- alors que les
    // jeux (ex: Galaxy Fighter : pressed(BTN_LEFT) || repeat(BTN_LEFT, 1))
    // attendent un "vrai" pratiquement CHAQUE frame quand l'intervalle vaut 1
    // (mouvement continu). Resultat observe : deplacement saccade, comme
    // s'il ne reagissait que par a-coups sur des fronts.
    // Nouvelle version : suivi par COMPTEUR DE FRAMES (Core::frameCount),
    // pas par le temps reel -- correspond a la cadence logique du jeu.
    static uint32_t s_nextRepeatFrame[BTN_COUNT] = {0};

    uint32_t mask = btn_mask(btn);
    if (!(g_keys.raw & mask)) { s_nextRepeatFrame[btn] = 0; return false; }

    uint32_t interval = framesInterval ? framesInterval : 1;
    if (g_keys.pressed & mask) {
        s_nextRepeatFrame[btn] = Core::frameCount + interval;   // premier front deja gere par pressed()
        return false;
    }
    if (Core::frameCount >= s_nextRepeatFrame[btn]) {
        s_nextRepeatFrame[btn] = Core::frameCount + interval;
        return true;
    }
    return false;
}

bool Buttons::aBtn()     const { return down(BTN_A); }
bool Buttons::bBtn()     const { return down(BTN_B); }
bool Buttons::cBtn()     const { return down(BTN_C); }
bool Buttons::upBtn()    const { return down(BTN_UP); }
bool Buttons::downBtn()  const { return down(BTN_DOWN); }
bool Buttons::leftBtn()  const { return down(BTN_LEFT); }
bool Buttons::rightBtn() const { return down(BTN_RIGHT); }

void Core::begin() {
    input_init();
    Sound::begin();
    s_lastFrameMs = now_ms();
    s_firstFrame = true;
    frameCount = 0;
}

void Core::setFrameRate(uint8_t fps) { s_fps = fps ? fps : 60; }
bool Core::isRunning() { return true; }
uint32_t Core::getTime() { return now_ms(); }

bool Core::update() {
    uint32_t frame_ms = 1000u / s_fps;
    uint32_t t = now_ms();
    if (!s_firstFrame && (t - s_lastFrameMs) < frame_ms) return false;   // pas encore l'heure
    s_lastFrameMs = t;
    s_firstFrame = false;

    if (frameCount > 0 && !suppressPresent) Display::present();   // affiche la frame precedente avant d'en dessiner une nouvelle
    input_poll(g_keys);
    // BUG TROUVE ET CORRIGE : Sound::poll() n'etait jamais appele -- sfxDataPtr
    // ne se mettait jamais a jour, Utils::sfxOver() renvoyait toujours faux,
    // et les jeux utilisant un systeme de priorite de son (ex: Galaxy Fighter,
    // BaseState::playSFX_ByPriority) restaient bloques apres le premier son
    // (soundFX.active ne repassait jamais a false).
    Sound::poll();
    ++frameCount;
    return true;
}

} // namespace Pokitto
