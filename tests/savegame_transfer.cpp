#include "common/savegame_transfer.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    assert(Is_Manual_Savegame_Name("SAVEGAME.000"));
    assert(Is_Manual_Savegame_Name("savegame.999"));
    assert(!Is_Manual_Savegame_Name("AUTOSAVE.IPAD0"));
    assert(!Is_Manual_Savegame_Name("SAVEGAME.01"));
    assert(!Is_Manual_Savegame_Name("SAVEGAME.A01"));
    assert(!Is_Manual_Savegame_Name("SAVEGAME.001.tmp"));
    assert(Manual_Savegame_Slot("savegame.042") == 42);
    assert(Manual_Savegame_Slot("autosave.ipad0") == -1);
    assert(Manual_Savegame_Name(7) == "SAVEGAME.007");
    assert(Manual_Savegame_Name(-1).empty());
    assert(Manual_Savegame_Name(1000).empty());

    std::vector<std::string> occupied;
    occupied.push_back("SAVEGAME.000");
    occupied.push_back("savegame.002");
    occupied.push_back("AUTOSAVE.IPAD0");
    assert(First_Free_Manual_Savegame_Slot(occupied) == 1);
    occupied.push_back("SAVEGAME.001");
    assert(First_Free_Manual_Savegame_Slot(occupied, 2) == -1);

    const std::string exported = Savegame_Export_Name("Vor dem Tempel / Test", "GDI", 7, 2);
    assert(exported == "CNC-GDI-Vor-dem-Tempel-Test-Mission-7-Slot-002.cncsave");
    assert(Savegame_Export_Name("../", "", 1, 0) == "CNC-CNC-Spielstand-Mission-1-Slot-000.cncsave");

    std::cout << "Save-game transfer policy matrix passed\n";
    return 0;
}
