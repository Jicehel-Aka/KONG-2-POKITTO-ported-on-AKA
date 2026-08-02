// pokitto_compat/AvrCompat.h — compat minimale des macros/fonctions AVR-Arduino
// utilisees telles quelles par le code source Kong-II (assets en PROGMEM,
// random()/abs() Arduino). Sur ESP32, tout le const est deja en flash : ces
// macros deviennent des no-op / de simples fonctions.
#pragma once
#include <cstdint>
#include <cstdlib>

#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))
#endif
#ifndef pgm_read_word
#define pgm_read_word(addr) (*(const uint16_t*)(addr))
#endif

// random() façon Arduino (random(max) et random(min,max)) ; abs() vient deja
// de <cstdlib> (inclus ci-dessus).
inline long random(long max)            { return max > 0 ? (rand() % max) : 0; }
inline long random(long mini, long maxi){ return maxi > mini ? mini + (rand() % (maxi - mini)) : mini; }
