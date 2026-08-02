// pokitto_compat/PokittoDisplay.h — Pokitto::Display sur l'AKA.
// Regle officielle (POKITTO_COMPAT_SPEC.md) : viewport 220x176, centre sur
// l'ecran AKA (320x240) a l'offset (50,32). Aucune mise a l'echelle, aucune
// deformation : le pixel-art original est rendu 1:1.
//
// Format bitmap reel observe dans Kong-II (Kong_FacingRight_F1.h etc.) :
//   uint8_t data[] = { W, H, <nibbles 4bpp, 2 pixels/octet, ordre haut->bas> }
// Couleur = palette chargee par loadRGBPalette() ; index transparent regle
// par setInvisibleColor() (Kong-II utilise l'index 14).
#pragma once
#include <cstdint>
#include <cstddef>

namespace Pokitto {

constexpr int16_t POK_SCREEN_W = 220;
constexpr int16_t POK_SCREEN_H = 176;
constexpr int16_t POK_VIEWPORT_X = 50;   // offset fixe sur l'ecran AKA (320x240)
constexpr int16_t POK_VIEWPORT_Y = 32;

class Display {
public:
    // --- API Pokitto d'origine (POKITTO_COMPAT_SPEC.md) ---
    static void clear();
    static void fillScreen(uint16_t color);
    static void drawPixel(int16_t x, int16_t y);
    static void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
    static void drawRect(int16_t x, int16_t y, int16_t w, int16_t h);
    static void fillRect(int16_t x, int16_t y, int16_t w, int16_t h);
    // Format reel Kong-II : bitmap 4bpp nibble-packe, entete [W,H] inclus.
    static void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap);
    // Variante avec drapeaux (retournement horizontal/vertical) : les 2 seuls
    // appels du jeu passent (false,false) — pas encore implemente au-dela.
    static void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, bool /*flipX*/, bool /*flipY*/) {
        drawBitmap(x, y, bitmap);
    }

    static void setCursor(int16_t x, int16_t y);
    static void print(const char* s);
    static void println(const char* s);

    // --- Extensions reellement utilisees par Kong-II (main.cpp) ---
    static void loadRGBPalette(const uint8_t* palette24);   // 16 x (r,g,b) 8-bit
    static void setColor(uint8_t paletteIndex);              // couleur du pen (drawPixel/Rect/...)
    static void setInvisibleColor(uint8_t paletteIndex);     // index transparent pour drawBitmap
    static bool persistence;                                  // Pokitto: pas d'auto-clear si true (ici : ignore, le jeu redessine tout)

    // Presente la frame (appelee par PokittoCore::update() une fois par frame)
    static void present();

private:
    static uint8_t  s_penColor;
    static uint8_t  s_invisibleColor;
    static uint16_t s_palette[16];   // palette courante, deja convertie en BGR565 AKA
};

} // namespace Pokitto
