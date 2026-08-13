#include "function.h"

#include <cstddef>
#include <cstdio>

extern bool Get_Savefile_Info(const char* file_name, char* buf, unsigned* scenp, HousesType* housep);

extern "C" bool TiberianDawn_ValidateSaveGame(const char* path,
                                             char* description,
                                             size_t description_capacity,
                                             unsigned* scenario,
                                             int* faction)
{
    if (!path || !description || description_capacity == 0 || !scenario || !faction) {
        return false;
    }

    char engine_description[DESCRIP_MAX] = {0};
    HousesType house = HOUSE_NONE;
    if (!Get_Savefile_Info(path, engine_description, scenario, &house)) {
        description[0] = '\0';
        return false;
    }
    std::snprintf(description, description_capacity, "%s", engine_description);
    *faction = house == HOUSE_BAD ? 1 : 0;
    return true;
}
