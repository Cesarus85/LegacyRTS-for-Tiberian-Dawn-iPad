#include "ipados_lifecycle.h"

#include "function.h"
#include "externs.h"
#include "common/app_lifecycle.h"
#include "common/debugstring.h"
#include "common/paths.h"
#include "common/recovery_transaction.h"
#include "common/settings.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

extern bool InMovie;
extern bool Get_Savefile_Info(const char* file_name, char* buf, unsigned* scenp, HousesType* housep);
extern "C" void TiberianDawn_EndBackgroundTask(void);

namespace
{
const char* const AutosaveNames[2] = {"AUTOSAVE.IPAD0", "AUTOSAVE.IPAD1"};
const char* const AutosaveTemporaryName = "AUTOSAVE.IPAD.TMP";

std::string CachedRecoveryPath;
bool AutosaveInProgress = false;
bool HasAutosavedFrame = false;
unsigned AutosavedFrame = 0;

bool Modification_Time_Is_Older_Or_Equal(const struct stat& left, const struct stat& right)
{
    if (left.st_mtimespec.tv_sec != right.st_mtimespec.tv_sec) {
        return left.st_mtimespec.tv_sec < right.st_mtimespec.tv_sec;
    }
    return left.st_mtimespec.tv_nsec <= right.st_mtimespec.tv_nsec;
}

std::string User_File_Path(const char* name)
{
    std::string normalized_name(name);
    std::transform(normalized_name.begin(), normalized_name.end(), normalized_name.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return Paths.Concatenate_Paths(Paths.User_Path(), normalized_name.c_str());
}

void Sync_File(const std::string& path)
{
    const int descriptor = open(path.c_str(), O_RDONLY);
    if (descriptor >= 0) {
        fsync(descriptor);
        close(descriptor);
    }
}

void Sync_User_Directory(void)
{
    const int descriptor = open(Paths.User_Path(), O_RDONLY);
    if (descriptor >= 0) {
        fsync(descriptor);
        close(descriptor);
    }
}

void Flush_Settings(void)
{
    Options.Save_Settings();

    CCFileClass file(CONFIG_FILE_NAME);
    INIClass ini;
    ini.Load(file);
    Settings.Save(ini);
    ini.Save(file);

    Sync_File(User_File_Path(CONFIG_FILE_NAME));
}

bool Can_Autosave_Current_Game(void)
{
    return InMainLoop && GameActive && PlayerPtr != nullptr && !InMovie && !PlaybackGame
           && (GameToPlay == GAME_NORMAL || GameToPlay == GAME_SKIRMISH);
}

bool Write_Recovery_Autosave(void)
{
    if (!Can_Autosave_Current_Game() || AutosaveInProgress) {
        return false;
    }
    if (HasAutosavedFrame && AutosavedFrame == Frame) {
        return true;
    }

    AutosaveInProgress = true;
    const std::string temporary_path = User_File_Path(AutosaveTemporaryName);
    const RecoveryCommitResult result = Commit_Recovery_File(
        temporary_path,
        User_File_Path(AutosaveNames[0]),
        User_File_Path(AutosaveNames[1]),
        Paths.User_Path(),
        [](const std::string& path) { return Save_Game(path.c_str(), "iPadOS recovery autosave"); });

    if (result.committed) {
        CachedRecoveryPath = result.target_path;
        HasAutosavedFrame = true;
        AutosavedFrame = Frame;
        if (result.durable) {
            DBG_INFO("Committed iPadOS recovery autosave to %s", result.target_path.c_str());
        } else {
            DBG_ERROR("Recovery autosave was committed but directory sync failed");
        }
    } else {
        DBG_ERROR("Unable to commit iPadOS recovery autosave (error %d)", static_cast<int>(result.error));
    }
    AutosaveInProgress = false;
    return result.committed;
}

bool Find_Latest_Valid_Autosave(std::string& result)
{
    struct Candidate
    {
        std::string path;
        struct stat status;
    };

    Candidate candidates[2];
    int count = 0;
    for (int i = 0; i < 2; ++i) {
        const std::string path = User_File_Path(AutosaveNames[i]);
        struct stat status;
        if (stat(path.c_str(), &status) == 0 && S_ISREG(status.st_mode)) {
            candidates[count].path = path;
            candidates[count].status = status;
            ++count;
        }
    }

    std::sort(candidates, candidates + count, [](const Candidate& left, const Candidate& right) {
        return !Modification_Time_Is_Older_Or_Equal(left.status, right.status);
    });

    for (int i = 0; i < count; ++i) {
        char description[DESCRIP_MAX] = {0};
        unsigned scenario = 0;
        HousesType house = HOUSE_NONE;
        if (Get_Savefile_Info(candidates[i].path.c_str(), description, &scenario, &house)) {
            result = candidates[i].path;
            return true;
        }
    }
    result.clear();
    return false;
}

void Handle_Lifecycle_Event(AppLifecycleEvent event)
{
    switch (event) {
    case APP_LIFECYCLE_TERMINATING:
    case APP_LIFECYCLE_ENTERED_BACKGROUND:
        Flush_Settings();
        Write_Recovery_Autosave();
        TiberianDawn_EndBackgroundTask();
        break;
    case APP_LIFECYCLE_LOW_MEMORY:
        if (!InMovie) {
            Free_Interpolated_Palettes();
        }
        DBG_INFO("Handled iPadOS memory warning");
        break;
    case APP_LIFECYCLE_ENTERED_FOREGROUND:
    case APP_LIFECYCLE_NONE:
        break;
    }
}
}

void Install_IPadOS_Lifecycle_Handler(void)
{
    Set_App_Lifecycle_Handler(Handle_Lifecycle_Event);
}

bool IPadOS_Has_Recovery_Autosave(void)
{
    unlink(User_File_Path(AutosaveTemporaryName).c_str());
    return Find_Latest_Valid_Autosave(CachedRecoveryPath);
}

bool IPadOS_Load_Recovery_Autosave(void)
{
    if (!Find_Latest_Valid_Autosave(CachedRecoveryPath)) {
        return false;
    }

    if (!Load_Game(CachedRecoveryPath.c_str())) {
        return false;
    }

    if (PlayerPtr != nullptr) {
        CurrentObject.Set_Active_Context(PlayerPtr->Class->House);
    }
    return true;
}

void IPadOS_Discard_Recovery_Autosaves(void)
{
    for (int i = 0; i < 2; ++i) {
        unlink(User_File_Path(AutosaveNames[i]).c_str());
    }
    unlink(User_File_Path(AutosaveTemporaryName).c_str());
    Sync_User_Directory();
    CachedRecoveryPath.clear();
    HasAutosavedFrame = false;
}

namespace
{
struct LifecycleHandlerInstaller
{
    LifecycleHandlerInstaller()
    {
        Install_IPadOS_Lifecycle_Handler();
    }
};

LifecycleHandlerInstaller InstallLifecycleHandler;
}
