// pokitto_compat/PokittoCookie.h — Sauvegarde Pokitto::Cookie sur carte SD.
// Usage reel (main.cpp Kong-II) : cookie.begin("KONGII", sizeof(cookie), (char*)&cookie);
// Emplacement (DECISIONS.md) : /sdcard/<game_id>/save.dat
#pragma once
#include <cstdint>
#include <cstddef>

namespace Pokitto {

class Cookie {
public:
    bool begin(const char* name, int size, char* data);
    bool loadCookie();
    bool saveCookie();

private:
    const char* m_name = nullptr;
    char*       m_data = nullptr;
    int         m_size = 0;
};

} // namespace Pokitto
