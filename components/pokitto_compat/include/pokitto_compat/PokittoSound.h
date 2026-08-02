// pokitto_compat/PokittoSound.h — Pokitto::Sound sur l'AKA.
// Usage reel (Kong-II, plus riche que le brouillon initial de la spec) :
//   PS::playSFX(data, length)        -> echantillons PCM 8-bit non signes embarques
//   PS::playMusicStream(path, loop)  -> flux PCM brut depuis la carte SD
//   PS::sfxDataPtr / PS::sfxEndPtr   -> comparaison de pointeurs pour detecter "fini"
// Implementation : gb_audio_track_wav du composant gamebuino (play_raw / play_wav).
// NOTE : pas de reechantillonnage de frequence pour l'instant (8-bit->16-bit
// seulement) ; a valider/ajuster sur le materiel reel (cf. rapport de portage).
#pragma once
#include <cstdint>
#include <cstddef>

namespace Pokitto {

class Sound {
public:
    static void playSFX(const uint8_t* data, uint32_t length);
    static void playMusicStream(const char* path, uint8_t loop);
    static void stopMusic();

    static void playTone(uint16_t frequency, uint16_t duration);   // V0.1 spec (fallback simple)
    static void stopTone();

    // A appeler une fois au demarrage (fait par Core::begin()) : enregistre les
    // pistes SFX/musique aupres du lecteur audio et demarre la tache de mixage.
    // Sans ceci, aucun son n'est audible (player.pool() n'est jamais appele).
    static void begin();

    // Compat pointeurs (usage reel : `return PS::sfxDataPtr >= PS::sfxEndPtr;`
    // pour savoir si le dernier SFX est termine). Mis a jour par playSFX/poll.
    static const uint8_t* sfxDataPtr;
    static const uint8_t* sfxEndPtr;

    static void poll();   // a appeler une fois par frame (Core::update) : met a jour sfxDataPtr
};

} // namespace Pokitto
