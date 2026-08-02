idf_component_register(
    SRCS
        "src/PokittoCore.cpp"
        "src/PokittoDisplay.cpp"
        "src/PokittoSound.cpp"
        "src/PokittoCookie.cpp"
    INCLUDE_DIRS "include"
    REQUIRES gamebuino aka_runtime)
