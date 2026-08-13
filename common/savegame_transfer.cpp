#include "savegame_transfer.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <set>

bool Is_Manual_Savegame_Name(const std::string& file_name)
{
    if (file_name.size() != 12) {
        return false;
    }
    const char prefix[] = "SAVEGAME.";
    for (size_t index = 0; index < sizeof(prefix) - 1; ++index) {
        if (std::toupper(static_cast<unsigned char>(file_name[index])) != prefix[index]) {
            return false;
        }
    }
    return std::isdigit(static_cast<unsigned char>(file_name[9]))
           && std::isdigit(static_cast<unsigned char>(file_name[10]))
           && std::isdigit(static_cast<unsigned char>(file_name[11]));
}

int Manual_Savegame_Slot(const std::string& file_name)
{
    if (!Is_Manual_Savegame_Name(file_name)) {
        return -1;
    }
    return (file_name[9] - '0') * 100 + (file_name[10] - '0') * 10 + (file_name[11] - '0');
}

std::string Manual_Savegame_Name(int slot)
{
    if (slot < 0 || slot > 999) {
        return std::string();
    }
    char name[16];
    std::snprintf(name, sizeof(name), "SAVEGAME.%03d", slot);
    return name;
}

int First_Free_Manual_Savegame_Slot(const std::vector<std::string>& file_names, int maximum_slot)
{
    if (maximum_slot < 0) {
        return -1;
    }
    maximum_slot = std::min(maximum_slot, 999);
    std::set<int> occupied;
    for (size_t index = 0; index < file_names.size(); ++index) {
        const int slot = Manual_Savegame_Slot(file_names[index]);
        if (slot >= 0) {
            occupied.insert(slot);
        }
    }
    for (int slot = 0; slot <= maximum_slot; ++slot) {
        if (occupied.count(slot) == 0) {
            return slot;
        }
    }
    return -1;
}

std::string Savegame_Export_Name(const std::string& description,
                                 const std::string& faction,
                                 unsigned scenario,
                                 int slot)
{
    std::string safe;
    bool separator = false;
    for (size_t index = 0; index < description.size() && safe.size() < 48; ++index) {
        const unsigned char character = static_cast<unsigned char>(description[index]);
        if (std::isalnum(character)) {
            safe += static_cast<char>(character);
            separator = false;
        } else if (!safe.empty() && !separator) {
            safe += '-';
            separator = true;
        }
    }
    while (!safe.empty() && safe.back() == '-') {
        safe.pop_back();
    }
    if (safe.empty()) {
        safe = "Spielstand";
    }

    std::string safe_faction;
    for (size_t index = 0; index < faction.size(); ++index) {
        const unsigned char character = static_cast<unsigned char>(faction[index]);
        if (std::isalnum(character)) {
            safe_faction += static_cast<char>(std::toupper(character));
        }
    }
    if (safe_faction.empty()) {
        safe_faction = "CNC";
    }

    char suffix[80];
    std::snprintf(suffix, sizeof(suffix), "-Mission-%u-Slot-%03d.cncsave", scenario, slot);
    return "CNC-" + safe_faction + "-" + safe + suffix;
}
