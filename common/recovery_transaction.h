#pragma once

#include <functional>
#include <string>

enum RecoveryCommitError
{
    RECOVERY_COMMIT_OK,
    RECOVERY_COMMIT_WRITER_FAILED,
    RECOVERY_COMMIT_INVALID_TEMPORARY_FILE,
    RECOVERY_COMMIT_FILE_SYNC_FAILED,
    RECOVERY_COMMIT_RENAME_FAILED,
    RECOVERY_COMMIT_DIRECTORY_SYNC_FAILED
};

struct RecoveryCommitResult
{
    bool committed;
    bool durable;
    std::string target_path;
    RecoveryCommitError error;
};

typedef std::function<bool(const std::string&)> RecoveryWriter;

RecoveryCommitResult Commit_Recovery_File(const std::string& temporary_path,
                                          const std::string& first_slot,
                                          const std::string& second_slot,
                                          const std::string& directory_path,
                                          const RecoveryWriter& writer);
