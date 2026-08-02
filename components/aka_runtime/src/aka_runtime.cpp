#include "aka_runtime/aka_runtime.h"
#include "core/input.h"
#include "gb_core.h"
#include "gb_common.h"      // GB_KEY_RUN / GB_KEY_MENU
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

    // La carte SD est montee par g_core.init() (cf. main.cpp) ; on cree juste
    // l'arborescence attendue (DECISIONS.md : /sdcard/AKA/ + /sdcard/<id>/).
    sd_mkdir("/sdcard/AKA");
    sd_mkdir("/sdcard/AKA/screenshots");
    sd_mkdir("/sdcard/AKA/lang");
    sd_mkdir(m_gamePath);
    char musicDir[80]; snprintf(musicDir, sizeof musicDir, "%s/music", m_gamePath);
    sd_mkdir(musicDir);
    // TODO (prochaine iteration) : charger settings.json (langue/volumes) ;
    // pour l'instant, valeurs par defaut en memoire uniquement.
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

bool AkaRuntime::update(const Keys& k) {
    // Meme logique que app_main.cpp (projet GnW_AKA, deja eprouvee) :
    // RUN+MENU maintenus -> retour au loader (lu directement sur g_core.buttons,
    // comme le fait le code de reference) ; MENU seul (SANS RUN) maintenu ~500ms
    // -> capture ecran.
    uint32_t s = g_core.buttons.state();
    check_return_to_loader(s & GB_KEY_RUN, s & GB_KEY_MENU);

    static uint32_t s_menu_start = 0;
    static bool     s_shot_done  = false;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (k.MENU && !k.RUN) {
        if (!s_menu_start) { s_menu_start = now; s_shot_done = false; }
        else if (!s_shot_done && now - s_menu_start >= 500) {
            s_shot_done = true;
            bool ok = takeScreenshot();
            ESP_LOGI(TAG, "capture ecran : %s", ok ? "OK" : "ECHEC");
        }
    } else {
        s_menu_start = 0; s_shot_done = false;
    }
    return true;
}

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
    for (int y = H - 1; y >= 0; --y) {          // BMP : lignes bas -> haut
        for (int x = 0; x < W; ++x) {
            // Pixels en BGR565 (rouge sur les bits de poids faible), meme
            // convention que le reste du projet AKA.
            gb_pixel v = lcd_getpixel((uint16_t)x, (uint16_t)y);
            uint8_t r5 =  v        & 0x1F;
            uint8_t g6 = (v >> 5)  & 0x3F;
            uint8_t b5 = (v >> 11) & 0x1F;
            row[x*3+0] = (uint8_t)((b5 * 255) / 31);   // BMP = B,G,R
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
