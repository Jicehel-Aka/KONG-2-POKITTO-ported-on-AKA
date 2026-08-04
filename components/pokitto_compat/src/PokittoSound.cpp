#include "pokitto_compat/PokittoSound.h"
#include "aka_runtime/aka_runtime.h"
#include "gb_audio_track_wav.h"
#include "gb_audio_player.h"
#include "gb_ll_audio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "pokitto_sound";

namespace Pokitto {

const uint8_t* Sound::sfxDataPtr = nullptr;
const uint8_t* Sound::sfxEndPtr  = nullptr;

static gb_audio_player   s_player;         // absent avant : sans lui/sa tache, aucun son ne sort
static gb_audio_track_wav s_sfxTrack;      // une voix dediee aux SFX ponctuels (playSFX)
static gb_audio_track_wav s_musicTrack;    // une voix dediee a la musique (playMusicStream)

// La demande de musique arrive depuis la tache du JEU, mais fopen()+fread()
// doivent se faire depuis la MEME tache tout du long (certains pilotes SD/VFS
// se comportent mal si un fichier est ouvert dans une tache et lu dans une
// autre) -- on ne fait donc que POSER la demande ici ; c'est la tache audio
// elle-meme qui appelle reellement play_wav() (donc fopen()), plus bas.
static char             s_pendingMusicPath[160] = {0};
static volatile bool    s_pendingMusicRequest = false;

static void audio_mix_task(void*) {
    ESP_LOGI(TAG, "tache de mixage demarree");
    int n = 0;
    while (true) {
        if (s_pendingMusicRequest) {
            s_pendingMusicRequest = false;
            ESP_LOGI(TAG, "playMusicStream (depuis la tache audio) : %s", s_pendingMusicPath);
            int r = s_musicTrack.play_wav(s_pendingMusicPath);
            ESP_LOGI(TAG, "playMusicStream: play_wav -> %d", r);
        }
        s_player.pool();               // seul appelant : alimente le mixeur/FIFO I2S
        if ((++n % 500) == 0)           // ~1s (500 x 2ms) : etat de la piste musique
            ESP_LOGI(TAG, "musique en cours ? %s", s_musicTrack.is_playing() ? "oui" : "non");
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void Sound::begin() {
    ESP_LOGI(TAG, "Sound::begin()...");
    gb_ll_audio_set_volume(200);
    int r1 = s_player.add_track(&s_sfxTrack, 1.0f);
    int r2 = s_player.add_track(&s_musicTrack, 1.0f);   // volume piste explicite (maximum)
    ESP_LOGI(TAG, "add_track sfx=%d music=%d", r1, r2);
    s_player.set_master_volume(200);
    // Pile augmentee (etait 4096) : la lecture de fichier WAV depuis la SD
    // (streaming musique) est plus gourmande en pile que le simple memcpy
    // des SFX embarques ; un stack trop juste peut produire des lectures
    // silencieuses/corrompues sans crash visible.
    BaseType_t ok = xTaskCreatePinnedToCore(audio_mix_task, "AudioMixTask", 8192, nullptr, 5, nullptr, 1);  // AKA : Core 1 (la tache principale du jeu est sur Core 0 -- evite la contention)
    ESP_LOGI(TAG, "tache de mixage creee : %s", ok == pdPASS ? "ok" : "ECHEC");
}

// Buffer de conversion 8bit-non-signe -> 16bit-signe, reutilisable (PSRAM),
// dimensionne au plus gros SFX embarque rencontre.
static int16_t* s_convBuf = nullptr;
static size_t   s_convCap = 0;
static int16_t* conv_buffer(size_t need_samples) {
    if (s_convCap < need_samples) {
        if (s_convBuf) heap_caps_free(s_convBuf);
        s_convBuf = (int16_t*)heap_caps_malloc(need_samples * sizeof(int16_t), MALLOC_CAP_SPIRAM);
        s_convCap = s_convBuf ? need_samples : 0;
    }
    return s_convBuf;
}

void Sound::playSFX(const uint8_t* data, uint32_t length) {
    ESP_LOGI(TAG, "playSFX(len=%lu)", (unsigned long)length);
    int16_t* buf = conv_buffer(length);
    if (!buf) { ESP_LOGE(TAG, "playSFX: allocation PSRAM echouee"); return; }
    for (uint32_t i = 0; i < length; ++i)
        buf[i] = (int16_t)(((int)data[i] - 128) << 8);   // 8-bit non signe -> 16-bit signe
    s_sfxTrack.play_raw(buf, length);
    sfxDataPtr = data;
    sfxEndPtr  = data + length;   // "en cours" tant que sfxDataPtr < sfxEndPtr (cf poll())
}

void Sound::poll() {
    if (sfxDataPtr && sfxDataPtr < sfxEndPtr && !s_sfxTrack.is_playing())
        sfxDataPtr = sfxEndPtr;   // signale "termine" (comparaison de pointeurs, cf usage Kong-II)
}

void Sound::playMusicStream(const char* path, uint8_t /*loop*/) {
    // Le jeu passe un chemin RELATIF (ex: "music/kong2_1.raw") -- il faut le
    // prefixer par le dossier SD du jeu (chemin absolu obligatoire pour
    // fopen()), et remplacer l'extension .raw par .wav (fichiers convertis
    // avec un entete RIFF/WAVE valide, deposes sous /sdcard/<gameId>/music/).
    // On ne fait QUE poser la demande ici (voir audio_mix_task) : l'ouverture
    // et la lecture du fichier doivent se faire depuis la meme tache.
    char wavPath[160];
    snprintf(wavPath, sizeof wavPath, "%s/%s", akaRuntime.gamePath(), path);
    size_t n = strlen(wavPath);
    if (n > 4 && strcmp(wavPath + n - 4, ".raw") == 0) strcpy(wavPath + n - 4, ".wav");
    ESP_LOGI(TAG, "playMusicStream: demande = %s", wavPath);
    strncpy(s_pendingMusicPath, wavPath, sizeof(s_pendingMusicPath) - 1);
    s_pendingMusicRequest = true;
}
void Sound::stopMusic() { s_musicTrack.stop_playing(); }

void Sound::playTone(uint16_t, uint16_t) { /* non utilise par Kong-II ; reserve V0.1 */ }
void Sound::stopTone() {}

} // namespace Pokitto
