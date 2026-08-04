// main.cpp — Point d'entree du portage AKA de Kong II.
// Structure calquee sur le main() original de Kong-II (cookie -> setup ->
// boucle), adaptee pour app_main() ESP-IDF et l'init materiel AKA.
#include "gb_core.h"
#include "gb_graphics.h"
#include "core/input.h"
#include "aka_runtime/aka_runtime.h"
#include "pokitto_compat/Pokitto.h"

#include "game/Game.h"
#include "game/utils/GameCookie.h"
#include "game/utils/Enums.h"

#include <cstdlib>
#include <ctime>

gb_core     g_core;
gb_graphics gfx;

using PC = Pokitto::Core;
using PD = Pokitto::Display;

// Palette PICO-8 (16 x RGB 8-bit) — fournie par le SDK Pokitto reel ; Kong-II
// ne la definit pas lui-meme (PD::loadRGBPalette(palettePico) dans son main()
// d'origine). Reprise ici a l'identique.
const uint8_t palettePico[16 * 3] = {
      0,  0,  0,   29, 43, 83,  126, 37, 83,   0,135, 81,
    171, 82, 54,   95, 87, 79,  194,195,199,  255,241,232,
    255,  0, 77,  255,163,  0,  255,236, 39,    0,228, 54,
     41,173,255,  131,118,156,  255,119,168,  255,204,170,
};

Game       game;
GameCookie cookie;

extern "C" void app_main(void) {
    g_core.init();
    gfx.set_backlight_percent(80);
    gfx.set_refresh_rate(60);

    akaRuntime.begin("kong2");
    static const char* const kControls[] = { "CTRL_MOVE", "CTRL_CLIMB", "CTRL_JUMP", nullptr };
    akaRuntime.setControlsKeys(kControls);
    akaRuntime.setCredits("Kong II", "Press Play On Tape", "BSD 3-Clause",
                           "github.com/Press-Play-On-Tape/Kong-II-Pokitto");

    cookie.begin("KONGII", sizeof(cookie), (char*)&cookie);

    PC::begin();
    PD::loadRGBPalette(palettePico);
    PD::persistence = true;
    PD::setColor(5);
    PD::setInvisibleColor(14);
    PC::setFrameRate(70);

    srand((unsigned)time(0));

    // Meme logique que le main() d'origine : initialise la sauvegarde au
    // premier lancement, migre le score "dead" sans perdre les autres scores.
    switch (cookie.initialised) {
        case COOKIE_INITIALISED:
            cookie.initialised = COOKIE_INITIALISED + 1;
            cookie.deadScore = 0;
            cookie.saveCookie();
            break;
        case COOKIE_INITIALISED + 1:
            break;
        default:
            cookie.initialise();
            break;
    }

    game.setup(&cookie);

    while (PC::isRunning()) {
        PC::suppressPresent = akaRuntime.isMenuOpen();   // evite le clignotement menu/jeu
        if (!PC::update()) continue;
        if (!akaRuntime.update(g_keys)) continue;   // menu systeme actif ce tour-ci
        PC::sound.updateStream();
        game.loop();
    }
}
