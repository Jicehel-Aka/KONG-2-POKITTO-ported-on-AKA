#include "Game.h"
#include <cstdio>   // printf (trace de diagnostic temporaire)
// #include "utils/Enums.h"
// #include "utils/Structs.h"
// #include "utils/Utils.h"

using PC = Pokitto::Core;
using PD = Pokitto::Display;
using PS = Pokitto::Sound;


void Game::setup(GameCookie *cookie) { 
    
    this->cookie = cookie;
    	
}

void Game::loop(void) {

    static GameStateType lastState = (GameStateType)255;
    if (gameState != lastState) {
        printf("[ETAT] gameState = %d\n", (int)gameState);   // trace temporaire de diagnostic
        lastState = gameState;
    }

    PC::buttons.pollButtons();
    PD::clear();

    switch (gameState) {

        case GameStateType::SplashScreen_Activate:
            splashScreen_Activate();
            gameState = GameStateType::SplashScreen;
            [[fallthrough]];

        case GameStateType::SplashScreen:
            splashScreen_Update();
            splashScreen_Render();
            break;

        case GameStateType::TitleScreen_Activate:
            titleScreen_Activate();
            gameState = GameStateType::TitleScreen;
            [[fallthrough]];

        case GameStateType::TitleScreen:
            titleScreen_Update();
            titleScreen_Render();
            break;

        case GameStateType::PlayGame_Activate:
            playGame_Activate();
            gameState = GameStateType::PlayGame;
            [[fallthrough]];

        case GameStateType::PlayGame:
            playGame_Update();
            playGame_Render();
            break;

        default: break;

    }    

	
}
