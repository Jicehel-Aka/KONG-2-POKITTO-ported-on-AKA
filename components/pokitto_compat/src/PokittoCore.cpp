#include "pokitto_compat/PokittoCore.h"
#include "pokitto_compat/PokittoDisplay.h"
#include "pokitto_compat/PokittoSound.h"
#include "core/input.h"
#include "gb_ll_common.h"   // EXPANDER_KEY_*
#include "esp_timer.h"

namespace Pokitto {

uint32_t Core::frameCount = 0;
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
    // Auto-repeat façon UI : premier appui immediat (pressed), puis un nouveau
    // "tick" toutes les framesInterval frames tant que la touche est maintenue.
    uint32_t mask = btn_mask(btn);
    if (!(g_keys.raw & mask)) return false;
    if (g_keys.pressed & mask) return true;              // premier front, deja gere par pressed()
    // approx a partir du temps de maintien (holdStart) et de la cadence de frame
    int bit = __builtin_ctz(mask ? mask : 1);
    uint32_t held_ms = g_keys.holdStart[bit] ? (now_ms() - g_keys.holdStart[bit]) : 0;
    uint32_t frame_ms = s_fps ? (1000u / s_fps) : 16;
    uint32_t interval_ms = frame_ms * (framesInterval ? framesInterval : 1);
    return interval_ms ? (held_ms % (interval_ms * 4) < frame_ms) : false;  // repetition periodique approx
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

    if (frameCount > 0) Display::present();   // affiche la frame precedente avant d'en dessiner une nouvelle
    input_poll(g_keys);
    ++frameCount;
    return true;
}

} // namespace Pokitto
