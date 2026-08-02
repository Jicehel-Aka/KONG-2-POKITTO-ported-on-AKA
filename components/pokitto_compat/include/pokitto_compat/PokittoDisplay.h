// pokitto_compat/PokittoDisplay.h — Pokitto::Display sur l'AKA.
//
// V0.2 (upgrade suite au portage Galaxy Fighter, 2e jeu de reference) :
// remplace le "blit immediat par primitive" (V0.1, valide sur Kong-II) par
// un vrai FRAMEBUFFER INDEXE PERSISTANT (220x176, 4 bits/pixel, 2px/octet),
// exactement comme le Pokitto reel. Necessaire car certains jeux (Galaxy
// Fighter) accedent DIRECTEMENT au framebuffer (PD::screenbuffer), lisent
// PD::width/PD::height comme des champs, et assignent PD::invisiblecolor
// directement (pas seulement via setInvisibleColor()). Cette version reste
// 100% compatible avec Kong-II (drawBitmap/fillRect/etc marchent pareil,
// juste en ecrivant dans le framebuffer au lieu de blitter immediatement).
//
// Toutes les primitives ECRIVENT dans le framebuffer ; present() (appele
// une fois par frame depuis Core::update()) decode l'integralite du
// framebuffer via la palette et fait UN SEUL blit vers l'ecran AKA.
#pragma once
#include <cstdint>
#include <cstddef>

namespace Pokitto {

constexpr int16_t POK_VIEWPORT_X = 50;   // offset fixe sur l'ecran AKA (320x240)
constexpr int16_t POK_VIEWPORT_Y = 32;

class Display {
public:
    static constexpr int16_t width  = 220;
    static constexpr int16_t height = 176;
    static uint8_t  invisiblecolor;
    static uint8_t* screenbuffer;
    static bool     persistence;

    static void clear();
    static void fillScreen(uint16_t color);
    static void drawPixel(int16_t x, int16_t y);
    static void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
    static void drawRect(int16_t x, int16_t y, int16_t w, int16_t h);
    static void fillRect(int16_t x, int16_t y, int16_t w, int16_t h);
    static void drawColumn(int16_t x, int16_t y0, int16_t y1);

    static void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap);
    static void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, bool flipX, bool flipY);

    static void setCursor(int16_t x, int16_t y);
    static void print(const char* s);
    static void print(char c);
    static void println(const char* s);
    static void setFont(const uint8_t* font);

    static void loadRGBPalette(const uint8_t* palette24);
    static void setColor(uint8_t paletteIndex);
    static void setInvisibleColor(uint8_t paletteIndex);

    static void present();

private:
    static uint8_t  s_penColor;
    static uint16_t s_palette[16];
    static const uint8_t* s_font;
    static int16_t  s_cursorX, s_cursorY;

    static void ensureBuffers();
    static void setNibble(int16_t x, int16_t y, uint8_t idx);
    static uint8_t getNibble(int16_t x, int16_t y);
};

} // namespace Pokitto
