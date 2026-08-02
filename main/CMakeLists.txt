idf_component_register(
    SRCS "main.cpp" "game/Game.cpp" "game/Game_PlayGameState_activate.cpp" "game/Game_PlayGameState_render.cpp" "game/Game_PlayGameState_update.cpp" "game/Game_PlayGameState_utils.cpp" "game/Game_SplashScreenState.cpp" "game/Game_TitleScreenState.cpp" "game/entities/Bird.cpp" "game/entities/Birds.cpp" "game/entities/Clapper.cpp" "game/entities/Clappers.cpp" "game/entities/Key.cpp" "game/entities/Kong.cpp" "game/entities/Player.cpp" "game/entities/Spark.cpp" "game/entities/Sparks.cpp"
    INCLUDE_DIRS "." 
    REQUIRES gamebuino aka_runtime pokitto_compat
    PRIV_REQUIRES app_update)

# Le code Kong-II d'origine n'a pas ete ecrit pour -Werror : on relegue en
# simples avertissements les points non bloquants (meme approche que pour
# les autres jeux portes de ce projet).
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=reorder -Wno-error=switch -Wno-error=unused-variable
    -Wno-error=type-limits -Wno-error=narrowing -Wno-error=parentheses
    -Wno-error=maybe-uninitialized -Wno-error=unused-function -Wno-error=missing-field-initializers
    -Wno-error=unused-value)

# Utils::playMusicStream() (game/utils/Utils.h) a tout son corps derriere
# #ifdef INCLUDE_SOUND_FROM_SD -- sans cette macro, la fonction ne fait
# RIEN (aucun appel a PS::playMusicStream), silence total sans aucune
# erreur. A definir des que le fichiers de musique sont sur la carte SD.
#
# INCLUDE_SOUND (differente !) encadre TOUS les points d'appel reels dans
# la logique de jeu (Jump/Collide/ThrowKey/Introduction... dans
# Game_PlayGameState_update.cpp, _activate.cpp, _utils.cpp). Sans elle,
# Utils::playSoundEffect()/playMusicStream() ne sont JAMAIS appeles du
# tout -- silence total meme en jouant une partie complete jusqu'au Game
# Over, sans la moindre erreur de compilation (juste du code elimine par
# le preprocesseur). Trouve seulement via des traces printf placees a la
# fois dans Utils.h (jamais atteintes) et en relisant Game_PlayGameState_
# activate.cpp ligne par ligne.
target_compile_definitions(${COMPONENT_LIB} PRIVATE INCLUDE_SOUND_FROM_SD INCLUDE_SOUND)
