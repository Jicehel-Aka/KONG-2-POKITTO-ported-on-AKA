#include "aka_runtime/aka_runtime.h"
#include "core/input.h"
#include "gb_core.h"
#include "gb_common.h"      // GB_KEY_RUN / GB_KEY_MENU / EXPANDER_KEY_*
#include "gb_graphics.h"
#include "gb_ll_lcd.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <sys/stat.h>
#include <errno.h>
#include <cstdio>
#include <cstring>

extern gb_core g_core;   // defini dans main.cpp
static const char* TAG = "aka_runtime";

extern gb_graphics gfx;

AkaRuntime akaRuntime;

static uint8_t s_musicVolume = 80, s_sfxVolume = 70;   // defaut ; persistance settings.json a completer
static bool s_helpRequested = false;

// ---------------------------------------------------------------------
// Langues : table en memoire, chargee depuis DEUX fichiers par langue --
// /sdcard/AKA/lang/<code>.json (COMMUN, partage par tous les jeux : libelles
// du menu systeme) PUIS /sdcard/AKA/lang/<gameId>/<code>.json (SPECIFIQUE au
// jeu : commandes, credits...). Recherche en ordre INVERSE (le plus
// recemment charge -- donc le specifique au jeu -- gagne en cas de cle en
// double). Format volontairement simple ({"CLE": "texte"}, pas de JSON
// imbrique) pour eviter une dependance a une bibliotheque JSON complete.
// ---------------------------------------------------------------------
#define LANG_MAX_ENTRIES 96
#define LANG_KEY_LEN     24
#define LANG_VAL_LEN     96
static char s_langKeys[LANG_MAX_ENTRIES][LANG_KEY_LEN];
static char s_langVals[LANG_MAX_ENTRIES][LANG_VAL_LEN];
static int  s_langCount = 0;

static void load_language_file_append(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { ESP_LOGE(TAG, "load_language_file(%s) -> ECHEC (fichier absent)", path); return; }

    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    int added = 0;
    const char* p = buf;
    while (*p && s_langCount < LANG_MAX_ENTRIES) {
        while (*p && *p != '"') ++p;
        if (!*p) break;
        ++p;
        const char* keyStart = p;
        while (*p && *p != '"') ++p;
        if (!*p) break;
        size_t keyLen = (size_t)(p - keyStart);
        ++p;
        while (*p && *p != ':') ++p;
        if (!*p) break;
        ++p;
        while (*p && *p != '"') ++p;
        if (!*p) break;
        ++p;
        const char* valStart = p;
        while (*p && *p != '"') {
            if (*p == '\\' && *(p+1)) ++p;   // ignore les echappements simples
            ++p;
        }
        if (!*p) break;
        size_t valLen = (size_t)(p - valStart);
        ++p;

        if (keyLen > 0 && keyLen < LANG_KEY_LEN && valLen < LANG_VAL_LEN) {
            memcpy(s_langKeys[s_langCount], keyStart, keyLen);
            s_langKeys[s_langCount][keyLen] = '\0';
            memcpy(s_langVals[s_langCount], valStart, valLen);
            s_langVals[s_langCount][valLen] = '\0';
            ++s_langCount; ++added;
        }
    }
    ESP_LOGI(TAG, "load_language_file(%s) -> %d entrees ajoutees", path, added);
}

static void load_language(const char* gameId, const char* code) {
    s_langCount = 0;
    char pathCommon[64], pathGame[96];
    snprintf(pathCommon, sizeof pathCommon, "/sdcard/AKA/lang/%s.json", code);
    snprintf(pathGame, sizeof pathGame, "/sdcard/AKA/lang/%s/%s.json", gameId, code);
    load_language_file_append(pathCommon);
    load_language_file_append(pathGame);
}

// ---------------------------------------------------------------------

static bool sd_mkdir(const char* path) {
    if (mkdir(path, 0777) == 0) { ESP_LOGI(TAG, "mkdir(%s) -> cree", path); return true; }
    if (errno == EEXIST) { ESP_LOGI(TAG, "mkdir(%s) -> deja present", path); return true; }
    ESP_LOGE(TAG, "mkdir(%s) -> ECHEC (errno=%d, %s)", path, errno, strerror(errno));
    return false;
}

void AkaRuntime::begin(const char* gameId) {
    strncpy(m_gameId, gameId, sizeof(m_gameId) - 1);
    snprintf(m_gamePath, sizeof(m_gamePath), "/sdcard/%s", gameId);

    struct stat st;
    bool sd_ok = (stat("/sdcard", &st) == 0);
    ESP_LOGI(TAG, "verif /sdcard : %s (errno=%d si echec)", sd_ok ? "monte" : "ABSENT/PAS MONTE", sd_ok ? 0 : errno);

    sd_mkdir("/sdcard/AKA");
    sd_mkdir("/sdcard/AKA/screenshots");
    sd_mkdir("/sdcard/AKA/lang");
    char langGameDir[80]; snprintf(langGameDir, sizeof langGameDir, "/sdcard/AKA/lang/%s", gameId);
    sd_mkdir(langGameDir);
    sd_mkdir(m_gamePath);
    char musicDir[80]; snprintf(musicDir, sizeof musicDir, "%s/music", m_gamePath);
    sd_mkdir(musicDir);
    load_language(m_gameId, m_language);
}

void AkaRuntime::setCredits(const char* title, const char* author,
                             const char* license, const char* sourceUrl) {
    m_title = title; m_author = author; m_license = license; m_sourceUrl = sourceUrl;
}

void AkaRuntime::setControlsKeys(const char* const* keys) {
    m_controlsKeys = keys;
}

static void check_return_to_loader(bool run_held, bool menu_held) {
    static uint32_t combo_start = 0;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (run_held && menu_held) {
        if (!combo_start) { combo_start = now; ESP_LOGI(TAG, "RUN+MENU maintenus..."); }
        else if (now - combo_start >= 500) {
            ESP_LOGI(TAG, "-> retour au loader");
            const esp_partition_t* loader = esp_partition_find_first(
                ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
            if (!loader) { ESP_LOGE(TAG, "partition loader (OTA_1) introuvable !"); return; }
            esp_ota_set_boot_partition(loader);
            esp_restart();
        }
    } else combo_start = 0;
}

// =======================================================================
// Menu systeme (RUNTIME_SPEC.md) -- rendu direct via gb_graphics,
// independant de l'etat d'affichage du jeu.
// =======================================================================
enum class MenuState : uint8_t { Closed, Main, Controls, Language, Credits };
static MenuState s_menuState = MenuState::Closed;
static int8_t s_menuSel = 0;
static int8_t s_langSel = 0;

static const char* const kMainKeys[] = {
    "MENU_RESUME", "MENU_CONTROLS", "MENU_LANGUAGE", "MENU_CREDITS", "MENU_RETURN_LOADER"
};
static const int kMainCount = 5;

static const char* const kLangCodes[] = { "fr", "en", "de", "es", "it" };
static const char* const kLangNames[] = { "Francais", "English", "Deutsch", "Espanol", "Italiano" };
static const int kLangCount = 5;

static void menu_frame(const char* titleKey) {
    gfx.setColor(gfx.makeColor(10, 10, 30));
    gfx.fillRect(40, 20, 240, 200);
    gfx.setColor(gfx.makeColor(255, 255, 255));
    gfx.drawRect(40, 20, 240, 200);
    gfx.setColor(gfx.makeColor(255, 220, 0));
    gfx.move_cursor(52, 28);
    gfx.print_str(akaRuntime.translate(titleKey));
    gfx.setColor(gfx.makeColor(255, 255, 255));
}

void AkaRuntime::menuDrawMain() {
    menu_frame("MENU_TITLE");
    for (int i = 0; i < kMainCount; ++i) {
        int y = 50 + i * 16;
        bool sel = (i == s_menuSel);
        gfx.setColor(sel ? gfx.makeColor(255, 220, 0) : gfx.makeColor(255, 255, 255));
        gfx.move_cursor(52, y);
        gfx.print_str(sel ? ">" : " ");
        gfx.move_cursor(64, y);
        gfx.print_str(translate(kMainKeys[i]));
    }
}

void AkaRuntime::menuDrawControls() {
    menu_frame("MENU_CONTROLS");
    int y = 50;
    if (m_controlsKeys) {
        for (int i = 0; m_controlsKeys[i] != nullptr && y < 205; ++i, y += 14) {
            gfx.move_cursor(52, y);
            gfx.print_str(translate(m_controlsKeys[i]));
        }
    }
}

void AkaRuntime::menuDrawLanguage() {
    menu_frame("MENU_LANGUAGE");
    for (int i = 0; i < kLangCount; ++i) {
        int y = 50 + i * 16;
        bool sel = (i == s_langSel);
        gfx.setColor(sel ? gfx.makeColor(255, 220, 0) : gfx.makeColor(255, 255, 255));
        gfx.move_cursor(52, y);
        gfx.print_str(sel ? ">" : " ");
        gfx.move_cursor(64, y);
        gfx.print_str(kLangNames[i]);
    }
}

// Tronque un texte pour qu'il tienne dans la largeur de la boite du menu --
// BUG TROUVE ET CORRIGE (Galaxy Fighter) : un texte plus long que la boite
// (ex: URL de depot) laissait des residus visibles hors zone apres un
// changement d'ecran, car seule la boite elle-meme est effacee a chaque
// rafraichissement, pas le debordement au-dela.
static char s_truncBuf[3][40];
static const char* truncate_to_fit(const char* s, int maxChars, int bufIdx) {
    char* buf = s_truncBuf[bufIdx];
    size_t len = strlen(s);
    if ((int)len <= maxChars) { strncpy(buf, s, 39); buf[39] = '\0'; return buf; }
    int keep = maxChars - 3; if (keep < 0) keep = 0;
    memcpy(buf, s, keep);
    buf[keep] = buf[keep+1] = buf[keep+2] = '.';
    buf[keep+3] = '\0';
    return buf;
}

void AkaRuntime::menuDrawCredits() {
    menu_frame("MENU_CREDITS");
    int y = 50;
    gfx.move_cursor(52, y); gfx.print_str(truncate_to_fit(m_title, 28, 0)); y += 16;
    gfx.move_cursor(52, y); gfx.print_str(translate("CREDITS_AUTHOR")); y += 12;
    gfx.move_cursor(60, y); gfx.print_str(truncate_to_fit(m_author, 26, 0)); y += 20;
    gfx.move_cursor(52, y); gfx.print_str(translate("CREDITS_LICENSE")); y += 12;
    gfx.move_cursor(60, y); gfx.print_str(truncate_to_fit(m_license, 26, 0)); y += 20;
    gfx.move_cursor(52, y); gfx.print_str(translate("CREDITS_SOURCE")); y += 12;
    // Source (souvent la plus longue, ex: URL github) : repartie sur 2
    // lignes plutot que tronquee brutalement, plus lisible.
    size_t srcLen = strlen(m_sourceUrl);
    if (srcLen > 26) {
        char line1[27]; strncpy(line1, m_sourceUrl, 26); line1[26] = '\0';
        gfx.move_cursor(60, y); gfx.print_str(line1); y += 12;
        gfx.move_cursor(60, y); gfx.print_str(truncate_to_fit(m_sourceUrl + 26, 26, 1));
    } else {
        gfx.move_cursor(60, y); gfx.print_str(m_sourceUrl);
    }
}

// Renvoie true si le menu doit rester ouvert (le jeu ne doit rien faire
// d'autre ce tour-ci), false quand il vient de se fermer (le jeu reprend
// la main a partir du tour SUIVANT).
bool AkaRuntime::menuHandleInput(const Keys& k) {
    bool up = k.pressed & EXPANDER_KEY_UP, down = k.pressed & EXPANDER_KEY_DOWN;
    bool a  = k.pressed & GB_KEY_A, c = k.pressed & GB_KEY_C;
    bool menuShort = false;
    static bool s_wasMenuHeld = false;
    if (k.MENU) { s_wasMenuHeld = true; }
    else if (s_wasMenuHeld) { menuShort = true; s_wasMenuHeld = false; }

    switch (s_menuState) {
        case MenuState::Closed:
            return false;   // ne devrait pas arriver (appele seulement si menu ouvert)

        case MenuState::Main:
            if (up)   s_menuSel = (int8_t)((s_menuSel + kMainCount - 1) % kMainCount);
            if (down) s_menuSel = (int8_t)((s_menuSel + 1) % kMainCount);
            if (c || menuShort) { s_menuState = MenuState::Closed; return false; }
            if (a) {
                switch (s_menuSel) {
                    case 0: s_menuState = MenuState::Closed; return false;           // Reprendre
                    case 1: s_menuState = MenuState::Controls; break;                // Commandes
                    case 2: s_menuState = MenuState::Language; s_langSel = 0; break; // Langue
                    case 3: s_menuState = MenuState::Credits; break;                 // Credits
                    case 4: returnToLoader(); break;                                 // Retour au loader
                }
            }
            menuDrawMain();
            gfx.update();
            return true;

        case MenuState::Controls:
            if (a || c || menuShort) { s_menuState = MenuState::Main; }
            menuDrawControls();
            gfx.update();
            return true;

        case MenuState::Language:
            if (up)   s_langSel = (int8_t)((s_langSel + kLangCount - 1) % kLangCount);
            if (down) s_langSel = (int8_t)((s_langSel + 1) % kLangCount);
            if (c || menuShort) { s_menuState = MenuState::Main; }
            if (a) { setLanguage(kLangCodes[s_langSel]); s_menuState = MenuState::Main; }
            menuDrawLanguage();
            gfx.update();
            return true;

        case MenuState::Credits:
            if (a || c || menuShort) { s_menuState = MenuState::Main; }
            menuDrawCredits();
            gfx.update();
            return true;
    }
    return true;
}

bool AkaRuntime::update(const Keys& k) {
    // RUN+MENU maintenus -> retour au loader (lu directement sur g_core.buttons).
    uint32_t s = g_core.buttons.state();
    check_return_to_loader(s & GB_KEY_RUN, s & GB_KEY_MENU);

    // Menu deja ouvert : il prend la main entierement ce tour-ci.
    if (s_menuState != MenuState::Closed) {
        return !menuHandleInput(k);   // update() renvoie FAUX tant que le menu est ouvert
    }

    static uint32_t s_menu_start = 0;
    static bool     s_shot_done  = false;
    static bool     s_was_held   = false;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (k.MENU && !k.RUN) {
        if (!s_menu_start) { s_menu_start = now; s_shot_done = false; }
        else if (!s_shot_done && now - s_menu_start >= 500) {
            s_shot_done = true;
            bool ok = takeScreenshot();
            ESP_LOGI(TAG, "capture ecran : %s", ok ? "OK" : "ECHEC");
        }
        s_was_held = true;
    } else {
        if (s_was_held && !s_shot_done && s_menu_start != 0) {
            s_menuState = MenuState::Main;
            s_menuSel = 0;
            ESP_LOGI(TAG, "MENU (appui court) -> menu systeme ouvert");
        }
        s_menu_start = 0; s_shot_done = false; s_was_held = false;
    }
    return true;
}

bool AkaRuntime::helpRequested() {
    bool r = s_helpRequested;
    s_helpRequested = false;
    return r;
}

bool AkaRuntime::isMenuOpen() const { return s_menuState != MenuState::Closed; }

const char* AkaRuntime::gamePath()       const { return m_gamePath; }
const char* AkaRuntime::settingsPath()   const { return "/sdcard/AKA/settings.json"; }
const char* AkaRuntime::screenshotPath() const { return "/sdcard/AKA/screenshots"; }

bool AkaRuntime::takeScreenshot() {
    static const char* kDir = "/sdcard/AKA/screenshots";
    bool mkOk = sd_mkdir(kDir);
    ESP_LOGI(TAG, "takeScreenshot: mkdir(%s) -> %s", kDir, mkOk ? "ok" : "ECHEC");

    char path[80];
    int shot_num = -1;
    for (int i = 0; i < 10000; ++i) {
        snprintf(path, sizeof(path), "%s/%s_%04d.BMP", kDir, m_gameId, i);
        FILE* test = fopen(path, "rb");
        if (!test) { shot_num = i; break; }
        fclose(test);
    }
    if (shot_num < 0) { ESP_LOGE(TAG, "takeScreenshot: pas de nom de fichier libre"); return false; }
    ESP_LOGI(TAG, "takeScreenshot: chemin = %s", path);

    FILE* f = fopen(path, "wb");
    if (!f) { ESP_LOGE(TAG, "takeScreenshot: fopen ECHEC (%s)", path); return false; }

    const int W = 320, H = 240;
    const int row_bytes   = W * 3;
    const int row_padding = (4 - (row_bytes % 4)) % 4;
    const int row_stride  = row_bytes + row_padding;
    const uint32_t data_size = (uint32_t)row_stride * H;
    const uint32_t file_size = 14 + 40 + data_size;

    uint8_t header[54] = {0};
    header[0]='B'; header[1]='M';
    header[2]=(uint8_t)file_size;       header[3]=(uint8_t)(file_size>>8);
    header[4]=(uint8_t)(file_size>>16); header[5]=(uint8_t)(file_size>>24);
    header[10]=54;
    header[14]=40;
    header[18]=(uint8_t)W;  header[19]=(uint8_t)(W>>8);
    header[22]=(uint8_t)H;  header[23]=(uint8_t)(H>>8);
    header[26]=1; header[28]=24;
    header[34]=(uint8_t)data_size;       header[35]=(uint8_t)(data_size>>8);
    header[36]=(uint8_t)(data_size>>16); header[37]=(uint8_t)(data_size>>24);
    fwrite(header, 1, 54, f);

    uint8_t row[320 * 3 + 3] = {0};
    for (int y = H - 1; y >= 0; --y) {
        for (int x = 0; x < W; ++x) {
            gb_pixel v = lcd_getpixel((uint16_t)x, (uint16_t)y);
            uint8_t r5 =  v        & 0x1F;
            uint8_t g6 = (v >> 5)  & 0x3F;
            uint8_t b5 = (v >> 11) & 0x1F;
            row[x*3+0] = (uint8_t)((b5 * 255) / 31);
            row[x*3+1] = (uint8_t)((g6 * 255) / 63);
            row[x*3+2] = (uint8_t)((r5 * 255) / 31);
        }
        for (int p = 0; p < row_padding; ++p) row[row_bytes + p] = 0;
        fwrite(row, 1, row_stride, f);
    }
    fclose(f);
    ESP_LOGI(TAG, "takeScreenshot: ecrit avec succes (%s)", path);
    return true;
}

void AkaRuntime::returnToLoader() {
    const esp_partition_t* loader = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    if (loader) { esp_ota_set_boot_partition(loader); esp_restart(); }
}

uint8_t AkaRuntime::getMusicVolume() const { return s_musicVolume; }
uint8_t AkaRuntime::getSfxVolume()   const { return s_sfxVolume; }
void    AkaRuntime::setMusicVolume(uint8_t v) { s_musicVolume = v; }
void    AkaRuntime::setSfxVolume(uint8_t v)   { s_sfxVolume = v; }

const char* AkaRuntime::getLanguage() const { return m_language; }

void AkaRuntime::setLanguage(const char* code) {
    strncpy(m_language, code, sizeof(m_language) - 1);
    load_language(m_gameId, m_language);
}

const char* AkaRuntime::translate(const char* key) const {
    for (int i = s_langCount - 1; i >= 0; --i)   // ordre inverse : specifique au jeu prioritaire
        if (strcmp(s_langKeys[i], key) == 0) return s_langVals[i];
    return key;
}
