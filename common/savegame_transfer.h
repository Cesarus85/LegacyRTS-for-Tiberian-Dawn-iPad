#pragma once

#include <string>
#include <vector>

bool Is_Manual_Savegame_Name(const std::string& file_name);
int Manual_Savegame_Slot(const std::string& file_name);
std::string Manual_Savegame_Name(int slot);
int First_Free_Manual_Savegame_Slot(const std::vector<std::string>& file_names, int maximum_slot = 999);
std::string Savegame_Export_Name(const std::string& description,
                                 const std::string& faction,
                                 unsigned scenario,
                                 int slot);
