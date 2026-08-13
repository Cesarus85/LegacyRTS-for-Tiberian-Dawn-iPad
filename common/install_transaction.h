#pragma once

#include <functional>
#include <string>

struct InstallOperations
{
    std::function<bool(const std::string&)> exists;
    std::function<bool(const std::string&)> remove;
    std::function<bool(const std::string&, const std::string&)> move;
};

enum InstallCommitError
{
    INSTALL_COMMIT_OK,
    INSTALL_COMMIT_INVALID_OPERATIONS,
    INSTALL_COMMIT_BACKUP_CLEANUP_FAILED,
    INSTALL_COMMIT_BACKUP_MOVE_FAILED,
    INSTALL_COMMIT_STAGING_MOVE_FAILED,
    INSTALL_COMMIT_ROLLBACK_FAILED,
    INSTALL_COMMIT_OLD_DATA_RETAINED
};

struct InstallCommitResult
{
    bool committed;
    bool previous_install_preserved;
    InstallCommitError error;
};

InstallCommitResult Commit_Staged_Install(const std::string& staging,
                                          const std::string& destination,
                                          const std::string& backup,
                                          const InstallOperations& operations);

enum ImportCapacityDecision
{
    IMPORT_CAPACITY_UNKNOWN,
    IMPORT_CAPACITY_INSUFFICIENT,
    IMPORT_CAPACITY_SUFFICIENT
};

ImportCapacityDecision Evaluate_Import_Capacity(bool measurement_available,
                                                unsigned long long available_bytes,
                                                unsigned long long required_bytes);
