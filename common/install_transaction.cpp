#include "install_transaction.h"

InstallCommitResult Commit_Staged_Install(const std::string& staging,
                                          const std::string& destination,
                                          const std::string& backup,
                                          const InstallOperations& operations)
{
    if (!operations.exists || !operations.remove || !operations.move) {
        InstallCommitResult result = {false, false, INSTALL_COMMIT_INVALID_OPERATIONS};
        return result;
    }

    if (operations.exists(backup) && !operations.remove(backup)) {
        InstallCommitResult result = {false, operations.exists(destination), INSTALL_COMMIT_BACKUP_CLEANUP_FAILED};
        return result;
    }

    const bool had_previous_install = operations.exists(destination);
    if (had_previous_install && !operations.move(destination, backup)) {
        InstallCommitResult result = {false, true, INSTALL_COMMIT_BACKUP_MOVE_FAILED};
        return result;
    }

    if (!operations.move(staging, destination)) {
        if (had_previous_install && !operations.move(backup, destination)) {
            InstallCommitResult result = {false, false, INSTALL_COMMIT_ROLLBACK_FAILED};
            return result;
        }
        InstallCommitResult result = {false, had_previous_install, INSTALL_COMMIT_STAGING_MOVE_FAILED};
        return result;
    }

    if (had_previous_install && !operations.remove(backup)) {
        InstallCommitResult result = {true, true, INSTALL_COMMIT_OLD_DATA_RETAINED};
        return result;
    }
    InstallCommitResult result = {true, had_previous_install, INSTALL_COMMIT_OK};
    return result;
}

ImportCapacityDecision Evaluate_Import_Capacity(bool measurement_available,
                                                unsigned long long available_bytes,
                                                unsigned long long required_bytes)
{
    if (!measurement_available) {
        return IMPORT_CAPACITY_UNKNOWN;
    }
    return available_bytes < required_bytes ? IMPORT_CAPACITY_INSUFFICIENT : IMPORT_CAPACITY_SUFFICIENT;
}
