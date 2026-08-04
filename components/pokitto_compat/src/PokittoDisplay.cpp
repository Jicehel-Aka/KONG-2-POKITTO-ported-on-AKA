#include "pokitto_compat/PokittoDisplay.h"
#include "pokitto_compat/Font5x7.h"
#include "gb_graphics.h"
#include "esp_heap_caps.h"
#include <cstring>
#include <cstdlib>

extern gb_graphics gfx;

namespace Pokitto {

uint8_t  Display::invisiblecolor = 255;   // aucune transparence tant que non reglee
uint8_t* Display::screenbuffer = nullptr;
bool     Display::persistence = false;
uint8_t  Display::s_penColor = 0;
uint16_t Display::s_palette[16] = {0};
const uint8_t* Display::s_font = nullptr;
int16_t  Display::s_cursorX = 0;
int16_t  Display::s_cursorY = 0;

static uint16_t* s_decodeBuf = nullptr;   // RGB565 scratch (PSRAM), decode complet a chaque present()

static inline uint16_t pack_bgr565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)((r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11));
}

void Display::ensureBuffers() {
    // IMPORTANT : heap_caps_malloc() NE met PAS a zero la memoire allouee.
    // Le vrai Pokitto demarre avec un framebuffer a zero (noir), et certains
    // jeux (Galaxy Fighter) ne font JAMAIS clear()/fillScreen() explicitement
    // -- ils comptent sur ce zero initial. Sans le memset ici, le premier
    // affichage montre les residus aleatoires de la PSRAM (points de
    // couleurs au lieu de noir).
    if (!screenbuffer) {
        screenbuffer = (uint8_t*)heap_caps_malloc((width * height) / 2, MALLOC_CAP_SPIRAM);
        if (screenbuffer) memset(screenbuffer, 0, (width * height) / 2);
    }
    if (!s_decodeBuf)
        s_decodeBuf = (uint16_t*)heap_caps_malloc((size_t)width * height * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
}

void Display::setNibble(int16_t x, int16_t y, uint8_t idx) {
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    if (!screenbuffer) ensureBuffers();
    if (!screenbuffer) return;
    int off = y * (width >> 1) + (x >> 1);
    uint8_t& b = screenbuffer[off];
    if (x & 1) b = (uint8_t)((b & 0xF0) | (idx & 0x0F));
    else       b = (uint8_t)((b & 0x0F) | ((idx & 0x0F) << 4));
}
uint8_t Display::getNibble(int16_t x, int16_t y) {
    if (x < 0 || y < 0 || x >= width || y >= height) return 0;
    if (!screenbuffer) ensureBuffers();
    if (!screenbuffer) return 0;
    int off = y * (width >> 1) + (x >> 1);
    uint8_t b = screenbuffer[off];
    return (x & 1) ? (uint8_t)(b & 0x0F) : (uint8_t)(b >> 4);
}

void Display::loadRGBPalette(const uint8_t* palette24) {
    for (int i = 0; i < 16; ++i)
        s_palette[i] = pack_bgr565(palette24[i*3+0], palette24[i*3+1], palette24[i*3+2]);
}
void Display::setColor(uint8_t idx)          { s_penColor = idx; }
void Display::setInvisibleColor(uint8_t idx) { invisiblecolor = idx; }

void Display::clear() {
    ensureBuffers();
    if (screenbuffer) memset(screenbuffer, 0, (width * height) / 2);
}
void Display::fillScreen(uint16_t colorIdx) {
    ensureBuffers();
    if (!screenbuffer) return;
    uint8_t v = (uint8_t)(((colorIdx & 0x0F) << 4) | (colorIdx & 0x0F));
    memset(screenbuffer, v, (width * height) / 2);
}
void Display::drawPixel(int16_t x, int16_t y) { setNibble(x, y, s_penColor); }

void Display::drawColumn(int16_t x, int16_t y0, int16_t y1) {
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }
    for (int16_t y = y0; y <= y1; ++y) setNibble(x, y, s_penColor);
}
void Display::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        setNibble((int16_t)x0, (int16_t)y0, s_penColor);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
void Display::drawRect(int16_t x, int16_t y, int16_t w, int16_t h) {
    for (int16_t i = 0; i < w; ++i) { setNibble(x+i, y, s_penColor); setNibble(x+i, y+h-1, s_penColor); }
    for (int16_t j = 0; j < h; ++j) { setNibble(x, y+j, s_penColor); setNibble(x+w-1, y+j, s_penColor); }
}
void Display::fillRect(int16_t x, int16_t y, int16_t w, int16_t h) {
    for (int16_t j = 0; j < h; ++j)
        for (int16_t i = 0; i < w; ++i)
            setNibble(x+i, y+j, s_penColor);
}

// Bitmap : [0]=W [1]=H puis donnees 4bpp, CHAQUE LIGNE ALIGNEE SUR UN OCTET
// (rowBytes=(W+1)/2) -- confirme sur 2 jeux (Kong-II, Galaxy Fighter).
void Display::drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap) {
    uint8_t w = bitmap[0], h = bitmap[1];
    const uint8_t* px = bitmap + 2;
    int rowBytes = (w + 1) / 2;
    for (int row = 0; row < h; ++row) {
        const uint8_t* rowPx = px + row * rowBytes;
        for (int col = 0; col < w; ++col) {
            uint8_t b = rowPx[col >> 1];
            uint8_t idx = (col & 1) ? (b & 0x0F) : (b >> 4);
            if (idx == invisiblecolor) continue;
            setNibble((int16_t)(x+col), (int16_t)(y+row), idx);
        }
    }
}
void Display::drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, bool /*flipX*/, bool /*flipY*/) {
    drawBitmap(x, y, bitmap);   // retournement pas encore implemente (idem V0.1)
}

void Display::setCursor(int16_t x, int16_t y) { s_cursorX = x; s_cursorY = y; }
void Display::setFont(const uint8_t* font)    { s_font = font; }

void Display::print(char c) {
    const uint8_t* f = s_font ? s_font : font5x7;
    int idx = (c >= FONT5X7_FIRST && c <= FONT5X7_LAST) ? (c - FONT5X7_FIRST) : 0;
    const uint8_t* glyph = f + idx * FONT5X7_WIDTH;
    for (int col = 0; col < FONT5X7_WIDTH; ++col) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < FONT5X7_HEIGHT; ++row)
            if (bits & (1 << row)) setNibble((int16_t)(s_cursorX+col), (int16_t)(s_cursorY+row), s_penColor);
    }
    s_cursorX += FONT5X7_WIDTH + 1;
}
void Display::print(const char* s) { while (*s) print(*s++); }
void Display::println(const char* s) { print(s); s_cursorX = 0; s_cursorY += FONT5X7_HEIGHT + 1; }

void Display::present() {
    ensureBuffers();
    if (!screenbuffer || !s_decodeBuf) return;
    // OPTIMISATION VITESSE : l'ancienne version appelait getNibble() par
    // pixel (verification de bornes + ensureBuffers() + recalcul complet du
    // decalage a CHAQUE pixel, et le MEME octet indexe etait lu DEUX FOIS
    // pour chaque paire de pixels). Ici : decalage de ligne calcule une
    // seule fois par ligne, lecture d'octet UNIQUE partagee entre les deux
    // pixels qu'il contient, aucun appel de fonction dans la boucle chaude.
    const int rowBytes = width >> 1;
    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = screenbuffer + y * rowBytes;
        uint16_t* dstRow = s_decodeBuf + y * width;
        int x = 0;
        for (int bx = 0; bx < rowBytes; ++bx) {
            uint8_t b = srcRow[bx];
            dstRow[x++] = s_palette[b >> 4];
            if (x < width) dstRow[x++] = s_palette[b & 0x0F];
        }
    }
    gfx.drawImage(POK_VIEWPORT_X, POK_VIEWPORT_Y, s_decodeBuf, width, height);
    gfx.update();
}

} // namespace Pokitto
