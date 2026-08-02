#include "pokitto_compat/PokittoCookie.h"
#include "aka_runtime/aka_runtime.h"   // akaRuntime.gamePath()
#include <cstdio>
#include <cstring>

namespace Pokitto {

bool Cookie::begin(const char* name, int size, char* data) {
    m_name = name; m_size = size; m_data = data;
    return true;   // le dossier de sauvegarde est cree par aka_runtime au demarrage
}

bool Cookie::loadCookie() {
    if (!m_data) return false;
    char path[128];
    snprintf(path, sizeof path, "%s/save.dat", akaRuntime.gamePath());
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    size_t n = fread(m_data, 1, (size_t)m_size, f);
    fclose(f);
    return n == (size_t)m_size;
}

bool Cookie::saveCookie() {
    if (!m_data) return false;
    char path[128];
    snprintf(path, sizeof path, "%s/save.dat", akaRuntime.gamePath());
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    size_t n = fwrite(m_data, 1, (size_t)m_size, f);
    fclose(f);
    return n == (size_t)m_size;
}

} // namespace Pokitto
