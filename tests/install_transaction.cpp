#include "common/install_transaction.h"

#include <cassert>
#include <iostream>
#include <map>
#include <string>

namespace
{
struct FakeFiles
{
    std::map<std::string, std::string> values;
    int fail_move_number = 0;
    int move_count = 0;
    bool fail_remove = false;

    InstallOperations Operations()
    {
        InstallOperations operations;
        operations.exists = [this](const std::string& path) { return values.count(path) != 0; };
        operations.remove = [this](const std::string& path) {
            if (fail_remove) return false;
            values.erase(path);
            return true;
        };
        operations.move = [this](const std::string& source, const std::string& destination) {
            ++move_count;
            if (move_count == fail_move_number || values.count(source) == 0 || values.count(destination) != 0) {
                return false;
            }
            values[destination] = values[source];
            values.erase(source);
            return true;
        };
        return operations;
    }
};
}

int main()
{
    const std::string staging = "staging";
    const std::string destination = "destination";
    const std::string backup = "backup";

    FakeFiles fresh;
    fresh.values[staging] = "new";
    InstallCommitResult result = Commit_Staged_Install(staging, destination, backup, fresh.Operations());
    assert(result.committed && fresh.values[destination] == "new");

    FakeFiles replacement;
    replacement.values[staging] = "new";
    replacement.values[destination] = "old";
    result = Commit_Staged_Install(staging, destination, backup, replacement.Operations());
    assert(result.committed && replacement.values[destination] == "new" && replacement.values.count(backup) == 0);

    FakeFiles no_backup;
    no_backup.values[staging] = "new";
    no_backup.values[destination] = "old";
    no_backup.fail_move_number = 1;
    result = Commit_Staged_Install(staging, destination, backup, no_backup.Operations());
    assert(!result.committed && result.error == INSTALL_COMMIT_BACKUP_MOVE_FAILED);
    assert(no_backup.values[destination] == "old" && no_backup.values[staging] == "new");

    FakeFiles rollback;
    rollback.values[staging] = "new";
    rollback.values[destination] = "old";
    rollback.fail_move_number = 2;
    result = Commit_Staged_Install(staging, destination, backup, rollback.Operations());
    assert(!result.committed && result.previous_install_preserved);
    assert(rollback.values[destination] == "old" && rollback.values[staging] == "new");

    FakeFiles recoverable;
    recoverable.values[staging] = "new";
    recoverable.values[destination] = "old";
    recoverable.fail_move_number = 2;
    InstallOperations recoverable_operations = recoverable.Operations();
    const std::function<bool(const std::string&, const std::string&)> normal_move = recoverable_operations.move;
    recoverable_operations.move = [&recoverable, normal_move](const std::string& source, const std::string& target) {
        if (recoverable.move_count >= 2) return false;
        return normal_move(source, target);
    };
    result = Commit_Staged_Install(staging, destination, backup, recoverable_operations);
    assert(!result.committed && result.error == INSTALL_COMMIT_ROLLBACK_FAILED);
    assert(recoverable.values[backup] == "old");

    FakeFiles stale_backup;
    stale_backup.values[staging] = "new";
    stale_backup.values[destination] = "old";
    stale_backup.values[backup] = "older";
    stale_backup.fail_remove = true;
    result = Commit_Staged_Install(staging, destination, backup, stale_backup.Operations());
    assert(!result.committed && result.error == INSTALL_COMMIT_BACKUP_CLEANUP_FAILED);
    assert(stale_backup.values[destination] == "old");

    const unsigned long long required = 1200ULL * 1024ULL * 1024ULL;
    assert(Evaluate_Import_Capacity(false, 0, required) == IMPORT_CAPACITY_UNKNOWN);
    assert(Evaluate_Import_Capacity(true, required - 1, required) == IMPORT_CAPACITY_INSUFFICIENT);
    assert(Evaluate_Import_Capacity(true, required, required) == IMPORT_CAPACITY_SUFFICIENT);

    std::cout << "Import transaction and capacity failure matrix passed\n";
    return 0;
}
