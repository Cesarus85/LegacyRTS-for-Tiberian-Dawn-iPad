#include "common/recovery_transaction.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

namespace
{
std::string Join(const std::string& directory, const char* name)
{
    return directory + "/" + name;
}

void Write(const std::string& path, const std::string& bytes)
{
    std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    assert(output.good());
}

std::string Read(const std::string& path)
{
    std::ifstream input(path.c_str(), std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

void SetTime(const std::string& path, long seconds)
{
    struct timeval times[2] = {{seconds, 0}, {seconds, 0}};
    assert(utimes(path.c_str(), times) == 0);
}
}

int main()
{
    char pattern[] = "/tmp/TiberianDawn-recovery-XXXXXX";
    const char* created = mkdtemp(pattern);
    assert(created != nullptr);
    const std::string directory(created);
    const std::string temporary = Join(directory, "save.tmp");
    const std::string first = Join(directory, "save.0");
    const std::string second = Join(directory, "save.1");

    RecoveryCommitResult result = Commit_Recovery_File(
        temporary, first, second, directory, [](const std::string& path) {
            Write(path, "first save");
            return true;
        });
    assert(result.committed && result.durable && result.target_path == first);
    assert(Read(first) == "first save");
    assert(access(temporary.c_str(), F_OK) != 0);

    result = Commit_Recovery_File(temporary, first, second, directory, [](const std::string& path) {
        Write(path, "second save");
        return true;
    });
    assert(result.committed && result.target_path == second);
    assert(Read(second) == "second save");

    SetTime(first, 100);
    SetTime(second, 200);
    result = Commit_Recovery_File(temporary, first, second, directory, [](const std::string& path) {
        Write(path, "rotated save");
        return true;
    });
    assert(result.committed && result.target_path == first);
    assert(Read(first) == "rotated save");
    assert(Read(second) == "second save");

    const std::string first_before = Read(first);
    const std::string second_before = Read(second);
    result = Commit_Recovery_File(temporary, first, second, directory, [](const std::string& path) {
        Write(path, "partial due to ENOSPC");
        return false;
    });
    assert(!result.committed && result.error == RECOVERY_COMMIT_WRITER_FAILED);
    assert(Read(first) == first_before && Read(second) == second_before);
    assert(access(temporary.c_str(), F_OK) != 0);

    result = Commit_Recovery_File(temporary, first, second, directory, [](const std::string&) -> bool {
        throw std::runtime_error("injected writer failure");
    });
    assert(!result.committed && result.error == RECOVERY_COMMIT_WRITER_FAILED);
    assert(Read(first) == first_before && Read(second) == second_before);

    result = Commit_Recovery_File(temporary, first, second, directory, [](const std::string& path) {
        Write(path, "");
        return true;
    });
    assert(!result.committed && result.error == RECOVERY_COMMIT_INVALID_TEMPORARY_FILE);
    assert(Read(first) == first_before && Read(second) == second_before);

    unlink(first.c_str());
    unlink(second.c_str());
    assert(rmdir(directory.c_str()) == 0);
    std::cout << "Recovery transaction failure matrix passed\n";
    return 0;
}
