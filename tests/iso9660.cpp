#include "common/iso9660.h"

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
const size_t SectorSize = 2048;

void LE32(unsigned char* destination, uint32_t value)
{
    destination[0] = static_cast<unsigned char>(value);
    destination[1] = static_cast<unsigned char>(value >> 8);
    destination[2] = static_cast<unsigned char>(value >> 16);
    destination[3] = static_cast<unsigned char>(value >> 24);
}

size_t Record(unsigned char* destination,
              uint32_t extent,
              uint32_t size,
              bool directory,
              const unsigned char* name,
              size_t name_size)
{
    size_t length = 33 + name_size;
    if ((length & 1) != 0) ++length;
    std::memset(destination, 0, length);
    destination[0] = static_cast<unsigned char>(length);
    LE32(destination + 2, extent);
    LE32(destination + 10, size);
    destination[25] = directory ? 2 : 0;
    destination[28] = 1;
    destination[31] = 1;
    destination[32] = static_cast<unsigned char>(name_size);
    std::memcpy(destination + 33, name, name_size);
    return length;
}

std::vector<unsigned char> ValidImage()
{
    std::vector<unsigned char> image(22 * SectorSize, 0);
    unsigned char* pvd = image.data() + 16 * SectorSize;
    pvd[0] = 1;
    std::memcpy(pvd + 1, "CD001", 5);
    pvd[6] = 1;
    std::memset(pvd + 40, ' ', 32);
    std::memcpy(pvd + 40, "GDI95", 5);
    const unsigned char root_name = 0;
    Record(pvd + 156, 20, SectorSize, true, &root_name, 1);

    unsigned char* directory = image.data() + 20 * SectorSize;
    size_t offset = Record(directory, 20, SectorSize, true, &root_name, 1);
    const unsigned char parent_name = 1;
    offset += Record(directory + offset, 20, SectorSize, true, &parent_name, 1);
    const unsigned char file_name[] = "CONQUER.MIX;1";
    Record(directory + offset, 21, 4, false, file_name, sizeof(file_name) - 1);
    std::memcpy(image.data() + 21 * SectorSize, "MIX!", 4);
    return image;
}

void WriteImage(const std::string& path, const std::vector<unsigned char>& image)
{
    std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
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
}

int main()
{
    char directory_pattern[] = "/tmp/TiberianDawn-iso-XXXXXX";
    const char* created = mkdtemp(directory_pattern);
    assert(created != nullptr);
    const std::string directory(created);
    const std::string image_path = directory + "/disc.iso";
    const std::string extracted_path = directory + "/conquer.mix";

    std::vector<unsigned char> image = ValidImage();
    WriteImage(image_path, image);
    ISO9660 valid(image_path);
    assert(valid.Volume() == "GDI95");
    assert(valid.Has("conquer.mix"));
    valid.Extract("CONQUER.MIX", extracted_path);
    std::ifstream extracted(extracted_path.c_str(), std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(extracted)), std::istreambuf_iterator<char>());
    assert(bytes == "MIX!");
    unlink(extracted_path.c_str());

    std::vector<unsigned char> truncated(16 * SectorSize, 0);
    WriteImage(image_path, truncated);
    assert(Throws([&]() { ISO9660 invalid(image_path); }));

    image = ValidImage();
    image[16 * SectorSize + 1] = 'X';
    WriteImage(image_path, image);
    assert(Throws([&]() { ISO9660 invalid(image_path); }));

    image = ValidImage();
    LE32(image.data() + 16 * SectorSize + 158, 200);
    WriteImage(image_path, image);
    assert(Throws([&]() { ISO9660 invalid(image_path); }));

    image = ValidImage();
    unsigned char* root = image.data() + 20 * SectorSize;
    const size_t first_record = root[0];
    const size_t second_record = root[first_record];
    root[first_record + second_record] = 10;
    WriteImage(image_path, image);
    assert(Throws([&]() { ISO9660 invalid(image_path); }));

    image = ValidImage();
    root = image.data() + 20 * SectorSize;
    const size_t file_offset = root[0] + root[root[0]];
    LE32(root + file_offset + 2, 999);
    WriteImage(image_path, image);
    assert(Throws([&]() { ISO9660 invalid(image_path); }));

    unlink(image_path.c_str());
    assert(rmdir(directory.c_str()) == 0);
    std::cout << "ISO-9660 corruption matrix passed\n";
    return 0;
}
