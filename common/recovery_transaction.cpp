#include "recovery_transaction.h"

#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
bool Older_Or_Equal(const struct stat& left, const struct stat& right)
{
#if defined(__APPLE__)
    if (left.st_mtimespec.tv_sec != right.st_mtimespec.tv_sec) {
        return left.st_mtimespec.tv_sec < right.st_mtimespec.tv_sec;
    }
    return left.st_mtimespec.tv_nsec <= right.st_mtimespec.tv_nsec;
#else
    if (left.st_mtim.tv_sec != right.st_mtim.tv_sec) {
        return left.st_mtim.tv_sec < right.st_mtim.tv_sec;
    }
    return left.st_mtim.tv_nsec <= right.st_mtim.tv_nsec;
#endif
}

bool Sync_Path(const std::string& path, bool directory)
{
    int flags = O_RDONLY;
#ifdef O_DIRECTORY
    if (directory) {
        flags |= O_DIRECTORY;
    }
#else
    (void)directory;
#endif
    const int descriptor = open(path.c_str(), flags);
    if (descriptor < 0) {
        return false;
    }
    const bool result = fsync(descriptor) == 0;
    close(descriptor);
    return result;
}

std::string Select_Target(const std::string& first, const std::string& second)
{
    struct stat first_status;
    struct stat second_status;
    const bool first_exists = stat(first.c_str(), &first_status) == 0;
    const bool second_exists = stat(second.c_str(), &second_status) == 0;
    if (!first_exists) {
        return first;
    }
    if (!second_exists) {
        return second;
    }
    return Older_Or_Equal(first_status, second_status) ? first : second;
}

RecoveryCommitResult Failure(const std::string& target, RecoveryCommitError error)
{
    RecoveryCommitResult result = {false, false, target, error};
    return result;
}
} // namespace

RecoveryCommitResult Commit_Recovery_File(const std::string& temporary_path,
                                          const std::string& first_slot,
                                          const std::string& second_slot,
                                          const std::string& directory_path,
                                          const RecoveryWriter& writer)
{
    const std::string target = Select_Target(first_slot, second_slot);
    unlink(temporary_path.c_str());

    bool written = false;
    try {
        written = writer && writer(temporary_path);
    } catch (...) {
        unlink(temporary_path.c_str());
        return Failure(target, RECOVERY_COMMIT_WRITER_FAILED);
    }
    if (!written) {
        unlink(temporary_path.c_str());
        return Failure(target, RECOVERY_COMMIT_WRITER_FAILED);
    }

    struct stat temporary_status;
    if (stat(temporary_path.c_str(), &temporary_status) != 0 || !S_ISREG(temporary_status.st_mode)
        || temporary_status.st_size <= 0) {
        unlink(temporary_path.c_str());
        return Failure(target, RECOVERY_COMMIT_INVALID_TEMPORARY_FILE);
    }
    if (!Sync_Path(temporary_path, false)) {
        unlink(temporary_path.c_str());
        return Failure(target, RECOVERY_COMMIT_FILE_SYNC_FAILED);
    }
    if (rename(temporary_path.c_str(), target.c_str()) != 0) {
        unlink(temporary_path.c_str());
        return Failure(target, RECOVERY_COMMIT_RENAME_FAILED);
    }

    if (!Sync_Path(directory_path, true)) {
        RecoveryCommitResult result = {true, false, target, RECOVERY_COMMIT_DIRECTORY_SYNC_FAILED};
        return result;
    }
    RecoveryCommitResult result = {true, true, target, RECOVERY_COMMIT_OK};
    return result;
}
