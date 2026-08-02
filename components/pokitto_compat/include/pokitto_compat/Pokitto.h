// pokitto_compat/Pokitto.h — point d'entree unique.
// Remplace, dans le code source Kong-II : #include "Pokitto.h" -> inchangé
// (ce fichier est trouve en premier grace a l'ordre des INCLUDE_DIRS).
#pragma once
#include "pokitto_compat/AvrCompat.h"
#include "pokitto_compat/PokittoCore.h"
#include "pokitto_compat/PokittoDisplay.h"
#include "pokitto_compat/PokittoSound.h"
#include "pokitto_compat/PokittoCookie.h"
#include "pokitto_compat/Font5x7.h"
