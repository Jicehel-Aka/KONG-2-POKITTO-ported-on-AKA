#include "pokitto_compat/PokittoDisplay.h"
#include "gb_graphics.h"
#include "esp_heap_caps.h"
#include <cstring>
#include <cstdlib>

extern gb_graphics gfx;   // instance globale, cf main.cpp

namespace Pokitto {

uint8_t  Display::s_penColor = 0;
uint8_t  Display::s_invisibleColor = 255;   // aucune transparence tant que non reglee
uint16_t Display::s_palette[16] = {0};
bool     Display::persistence = false;

static inline uint16_t pack_bgr565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)((r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11));   // BGR565 natif AKA
}

// Buffer de decodage reutilisable (PSRAM) : assez grand pour n'importe quel
// bitmap Pokitto (max = l'ecran Pokitto complet, 220x176).
static uint16_t* s_scratch = nullptr;
static uint16_t* scratch() {
    if (!s_scratch)
        s_scratch = (uint16_t*)heap_caps_malloc(POK_SCREEN_W * POK_SCREEN_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    return s_scratch;
}
// Couleur-cle utilisee pour marquer les pixels transparents dans le buffer decode.
static const uint16_t TRANS_KEY = 0xF81F;   // magenta BGR565 (meme convention que le reste du projet)

void Display::loadRGBPalette(const uint8_t* palette24) {
    for (int i = 0; i < 16; ++i)
        s_palette[i] = pack_bgr565(palette24[i*3+0], palette24[i*3+1], palette24[i*3+2]);
}
void Display::setColor(uint8_t idx)          { s_penColor = idx; }
void Display::setInvisibleColor(uint8_t idx) { s_invisibleColor = idx; }

void Display::clear() {
    gfx.setColor(gfx.makeColor(0, 0, 0));
    gfx.fillRect(POK_VIEWPORT_X, POK_VIEWPORT_Y, POK_SCREEN_W, POK_SCREEN_H);
}
void Display::fillScreen(uint16_t colorIdx) {
    uint16_t c = (colorIdx < 16) ? s_palette[colorIdx] : 0;
    gfx.setColor(c);
    gfx.fillRect(POK_VIEWPORT_X, POK_VIEWPORT_Y, POK_SCREEN_W, POK_SCREEN_H);
}
void Display::drawPixel(int16_t x, int16_t y) {
    if (x < 0 || y < 0 || x >= POK_SCREEN_W || y >= POK_SCREEN_H) return;
    gfx.setColor(s_palette[s_penColor & 0x0F]);
    gfx.fillRect(POK_VIEWPORT_X + x, POK_VIEWPORT_Y + y, 1, 1);
}
void Display::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    gfx.setColor(s_palette[s_penColor & 0x0F]);
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (x0 >= 0 && y0 >= 0 && x0 < POK_SCREEN_W && y0 < POK_SCREEN_H)
            gfx.fillRect(POK_VIEWPORT_X + x0, POK_VIEWPORT_Y + y0, 1, 1);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
void Display::drawRect(int16_t x, int16_t y, int16_t w, int16_t h) {
    gfx.setColor(s_palette[s_penColor & 0x0F]);
    gfx.drawRect(POK_VIEWPORT_X + x, POK_VIEWPORT_Y + y, w, h);
}
void Display::fillRect(int16_t x, int16_t y, int16_t w, int16_t h) {
    gfx.setColor(s_palette[s_penColor & 0x0F]);
    gfx.fillRect(POK_VIEWPORT_X + x, POK_VIEWPORT_Y + y, w, h);
}

// Bitmap Kong-II : [0]=W [1]=H puis les donnees 4bpp (2 pixels/octet). IMPORTANT :
// chaque LIGNE est alignee sur un octet entier (ceil(W/2) octets/ligne) -- ce
// n'est PAS un flux continu de nibbles sur toute l'image. Pour une largeur
// impaire, un flux continu decale progressivement chaque ligne d'un demi-octet
// -> cisaillement diagonal (confirme sur Ppot_Full.h : 131x68, 4488 octets
// reels = 66 octets/ligne (aligne) x 68, alors qu'un flux continu donnerait 4454).
void Display::drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap) {
    uint8_t w = bitmap[0], h = bitmap[1];
    uint16_t* buf = scratch();
    if (!buf) return;
    const uint8_t* px = bitmap + 2;
    int rowBytes = (w + 1) / 2;
    for (int row = 0; row < h; ++row) {
        const uint8_t* rowPx = px + row * rowBytes;
        for (int col = 0; col < w; ++col) {
            uint8_t b = rowPx[col >> 1];
            uint8_t idx = (col & 1) ? (b & 0x0F) : (b >> 4);
            buf[row * w + col] = (idx == s_invisibleColor) ? TRANS_KEY : s_palette[idx & 0x0F];
        }
    }
    gfx.drawImage(POK_VIEWPORT_X + x, POK_VIEWPORT_Y + y, buf, w, h, TRANS_KEY);
}

void Display::setCursor(int16_t x, int16_t y) { gfx.move_cursor(POK_VIEWPORT_X + x, POK_VIEWPORT_Y + y); }
void Display::print(const char* s)   { gfx.setColor(s_palette[s_penColor & 0x0F]); gfx.print_str(s); }
void Display::println(const char* s) { gfx.setColor(s_palette[s_penColor & 0x0F]); gfx.print_str(s); gfx.print_str("\n"); }

void Display::present() { gfx.update(); }

} // namespace Pokitto
