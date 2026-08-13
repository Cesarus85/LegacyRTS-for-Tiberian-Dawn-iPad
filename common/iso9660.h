#pragma once

#include <cstdint>
#include <fstream>
#include <map>
#include <string>

class ISO9660
{
public:
    struct Entry
    {
        uint32_t extent;
        uint32_t size;
        bool directory;
    };

    explicit ISO9660(const std::string& path);

    const std::string& Volume() const;
    bool Has(const std::string& path) const;
    void Extract(const std::string& source, const std::string& destination) const;

private:
    void ReadDirectory(uint32_t extent, uint32_t size, const std::string& prefix, int depth);
    void ValidateRange(uint32_t extent, uint32_t size, const char* description) const;

    std::ifstream stream;
    std::string file_path;
    std::string volume;
    std::map<std::string, Entry> entries;
    uint64_t image_size;
    size_t directory_count;
};
