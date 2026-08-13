#include "third_party/unshieldv3/ISArchiveV3.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>

namespace
{
template<class T> void Append(std::vector<unsigned char>& bytes, T value)
{
    const unsigned char* begin = reinterpret_cast<const unsigned char*>(&value);
    bytes.insert(bytes.end(), begin, begin + sizeof(value));
}

void Write(const std::string& path, const std::vector<unsigned char>& bytes)
{
    std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    assert(output.good());
}

bool Throws(const std::function<void()>& operation)
{
    try {
        operation();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

std::vector<unsigned char> Archive(const std::string& name, uint32_t declared_size, bool out_of_range)
{
    ISArchiveV3::Header header = {};
    header.signature1 = 0x8C655D13;
    header.signature2 = 0x02013a;
    header.file_count = 1;
    header.dir_count = 1;
    header.toc_address = sizeof(header);

    std::vector<unsigned char> bytes(reinterpret_cast<unsigned char*>(&header),
                                     reinterpret_cast<unsigned char*>(&header) + sizeof(header));
    Append<uint16_t>(bytes, 1); // directory file count
    Append<uint16_t>(bytes, 6); // empty directory record size
    Append<uint16_t>(bytes, 0); // empty directory name

    const uint32_t data_offset = static_cast<uint32_t>(sizeof(header) + 6 + 30 + name.size());
    Append<uint8_t>(bytes, 1);
    Append<uint16_t>(bytes, 0);
    Append<uint32_t>(bytes, declared_size);
    Append<uint32_t>(bytes, declared_size);
    Append<uint32_t>(bytes, out_of_range ? data_offset + 100 : data_offset);
    Append<uint32_t>(bytes, 0);
    Append<uint32_t>(bytes, 0);
    Append<uint16_t>(bytes, static_cast<uint16_t>(30 + name.size()));
    Append<uint8_t>(bytes, ISArchiveV3::File::UNCOMPRESSED);
    Append<uint8_t>(bytes, 0);
    Append<uint8_t>(bytes, 0);
    Append<uint8_t>(bytes, 1);
    Append<uint8_t>(bytes, static_cast<uint8_t>(name.size()));
    bytes.insert(bytes.end(), name.begin(), name.end());
    bytes.insert(bytes.end(), declared_size, static_cast<unsigned char>('A'));
    return bytes;
}
}

int main()
{
    char directory_pattern[] = "/tmp/TiberianDawn-isarchive-XXXXXX";
    const char* created = mkdtemp(directory_pattern);
    assert(created != nullptr);
    const std::string directory(created);
    const std::string path = directory + "/setup.z";

    Write(path, std::vector<unsigned char>());
    assert(Throws([&]() { ISArchiveV3 archive(path); }));

    std::vector<unsigned char> valid = Archive("TEST", 4, false);
    Write(path, valid);
    ISArchiveV3 archive(path);
    assert(archive.exists("TEST"));
    const std::vector<uint8_t> extracted = archive.decompress("TEST");
    assert(extracted.size() == 4 && extracted[0] == 'A');

    valid[0] ^= 0xff;
    Write(path, valid);
    assert(Throws([&]() { ISArchiveV3 invalid(path); }));

    Write(path, Archive("TEST", 4, true));
    assert(Throws([&]() { ISArchiveV3 invalid(path); }));

    Write(path, Archive("..\\X", 4, false));
    assert(Throws([&]() { ISArchiveV3 invalid(path); }));

    ISArchiveV3::Header header = {};
    header.signature1 = 0x8C655D13;
    header.signature2 = 0x02013a;
    header.dir_count = 1;
    header.toc_address = sizeof(header);
    std::vector<unsigned char> bad_record(reinterpret_cast<unsigned char*>(&header),
                                          reinterpret_cast<unsigned char*>(&header) + sizeof(header));
    Append<uint16_t>(bad_record, 0);
    Append<uint16_t>(bad_record, 0);
    Append<uint16_t>(bad_record, 0);
    Write(path, bad_record);
    assert(Throws([&]() { ISArchiveV3 invalid(path); }));

    unlink(path.c_str());
    assert(rmdir(directory.c_str()) == 0);
    std::cout << "InstallShield V3 corruption matrix passed\n";
    return 0;
}
