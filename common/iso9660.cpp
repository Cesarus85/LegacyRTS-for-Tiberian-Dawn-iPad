#include "iso9660.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
const uint64_t SectorSize = 2048;
const uint32_t MaximumDirectorySize = 32U * 1024U * 1024U;
const size_t MaximumDirectoryCount = 256;
const size_t MaximumEntryCount = 16384;

uint32_t ReadLE32(const unsigned char* value)
{
    return static_cast<uint32_t>(value[0]) | (static_cast<uint32_t>(value[1]) << 8)
           | (static_cast<uint32_t>(value[2]) << 16) | (static_cast<uint32_t>(value[3]) << 24);
}

std::string Upper(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return value;
}

bool ValidComponent(const std::string& name)
{
    if (name.empty() || name == "." || name == ".." || name.find('/') != std::string::npos
        || name.find('\\') != std::string::npos) {
        return false;
    }
    for (size_t index = 0; index < name.size(); ++index) {
        if (static_cast<unsigned char>(name[index]) < 32) {
            return false;
        }
    }
    return true;
}
} // namespace

ISO9660::ISO9660(const std::string& path)
    : stream(path.c_str(), std::ios::binary), file_path(path), image_size(0), directory_count(0)
{
    if (!stream) {
        throw std::runtime_error("ISO konnte nicht geoeffnet werden");
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff end = stream.tellg();
    if (end < static_cast<std::streamoff>(17 * SectorSize)) {
        throw std::runtime_error("ISO-Datei ist verkuerzt");
    }
    image_size = static_cast<uint64_t>(end);

    std::vector<unsigned char> pvd(SectorSize);
    stream.seekg(static_cast<std::streamoff>(16 * SectorSize), std::ios::beg);
    stream.read(reinterpret_cast<char*>(pvd.data()), static_cast<std::streamsize>(pvd.size()));
    if (stream.gcount() != static_cast<std::streamsize>(pvd.size()) || pvd[0] != 1
        || std::memcmp(pvd.data() + 1, "CD001", 5) != 0 || pvd[6] != 1) {
        throw std::runtime_error("Keine gueltige ISO-9660-CD");
    }

    volume.assign(reinterpret_cast<char*>(pvd.data() + 40), 32);
    while (!volume.empty() && (volume.back() == ' ' || volume.back() == '\0')) {
        volume.pop_back();
    }

    const unsigned char* root = pvd.data() + 156;
    if (root[0] < 34 || root[32] != 1 || root[33] != 0 || (root[25] & 2) == 0) {
        throw std::runtime_error("ISO-Hauptverzeichnis ist ungueltig");
    }
    ReadDirectory(ReadLE32(root + 2), ReadLE32(root + 10), "", 0);
}

const std::string& ISO9660::Volume() const
{
    return volume;
}

bool ISO9660::Has(const std::string& path) const
{
    return entries.find(Upper(path)) != entries.end();
}

void ISO9660::ValidateRange(uint32_t extent, uint32_t size, const char* description) const
{
    const uint64_t offset = static_cast<uint64_t>(extent) * SectorSize;
    if (offset > image_size || static_cast<uint64_t>(size) > image_size - offset) {
        throw std::runtime_error(std::string(description) + " liegt ausserhalb der ISO-Datei");
    }
}

void ISO9660::ReadDirectory(uint32_t extent, uint32_t size, const std::string& prefix, int depth)
{
    if (depth > 4) {
        throw std::runtime_error("ISO-Verzeichnisstruktur ist zu tief");
    }
    if (size == 0 || size > MaximumDirectorySize) {
        throw std::runtime_error("ISO-Verzeichnisgroesse ist ungueltig");
    }
    if (++directory_count > MaximumDirectoryCount) {
        throw std::runtime_error("ISO enthaelt zu viele Verzeichnisse");
    }
    ValidateRange(extent, size, "ISO-Verzeichnis");

    std::vector<unsigned char> data(size);
    stream.clear();
    stream.seekg(static_cast<std::streamoff>(static_cast<uint64_t>(extent) * SectorSize), std::ios::beg);
    stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    if (stream.gcount() != static_cast<std::streamsize>(size)) {
        throw std::runtime_error("ISO-Verzeichnis konnte nicht vollstaendig gelesen werden");
    }

    size_t offset = 0;
    while (offset < data.size()) {
        const unsigned char record_length = data[offset];
        if (record_length == 0) {
            const size_t next_sector = ((offset / SectorSize) + 1) * SectorSize;
            if (next_sector <= offset) {
                throw std::runtime_error("ISO-Verzeichnisueberlauf");
            }
            offset = next_sector;
            continue;
        }
        if (record_length < 34 || offset + record_length > data.size()) {
            throw std::runtime_error("ISO-Verzeichniseintrag ist verkuerzt");
        }

        const unsigned char* record = data.data() + offset;
        const unsigned char name_length = record[32];
        if (name_length == 0 || static_cast<size_t>(33 + name_length) > record_length) {
            throw std::runtime_error("ISO-Dateiname ist verkuerzt");
        }
        const bool special = name_length == 1 && (record[33] == 0 || record[33] == 1);
        if (!special) {
            std::string name(reinterpret_cast<const char*>(record + 33), name_length);
            const size_t version = name.find(';');
            if (version != std::string::npos) {
                name.erase(version);
            }
            if (!ValidComponent(name)) {
                throw std::runtime_error("ISO enthaelt einen ungueltigen Dateinamen");
            }

            Entry entry = {ReadLE32(record + 2), ReadLE32(record + 10), (record[25] & 2) != 0};
            ValidateRange(entry.extent, entry.size, entry.directory ? "ISO-Unterverzeichnis" : "ISO-Datei");
            const std::string path = prefix.empty() ? name : prefix + "/" + name;
            if (!entries.insert(std::make_pair(Upper(path), entry)).second) {
                throw std::runtime_error("ISO enthaelt doppelte Dateieintraege");
            }
            if (entries.size() > MaximumEntryCount) {
                throw std::runtime_error("ISO enthaelt zu viele Dateieintraege");
            }
            if (entry.directory && (depth == 0 || Upper(path) == "INSTALL")) {
                ReadDirectory(entry.extent, entry.size, path, depth + 1);
            }
        }
        offset += record_length;
    }
}

void ISO9660::Extract(const std::string& source, const std::string& destination) const
{
    std::map<std::string, Entry>::const_iterator found = entries.find(Upper(source));
    if (found == entries.end() || found->second.directory) {
        throw std::runtime_error("CD-Datei fehlt: " + source);
    }
    ValidateRange(found->second.extent, found->second.size, "CD-Datei");

    std::ifstream input(file_path.c_str(), std::ios::binary);
    std::ofstream output(destination.c_str(), std::ios::binary | std::ios::trunc);
    if (!input || !output) {
        throw std::runtime_error("Quell- oder Zieldatei konnte nicht geoeffnet werden");
    }

    try {
        input.seekg(static_cast<std::streamoff>(static_cast<uint64_t>(found->second.extent) * SectorSize),
                    std::ios::beg);
        std::vector<char> buffer(1024 * 1024);
        uint32_t remaining = found->second.size;
        while (remaining > 0) {
            const std::streamsize count = static_cast<std::streamsize>(
                std::min<uint32_t>(remaining, static_cast<uint32_t>(buffer.size())));
            input.read(buffer.data(), count);
            if (input.gcount() != count) {
                throw std::runtime_error("CD-Lesefehler");
            }
            output.write(buffer.data(), count);
            if (!output) {
                throw std::runtime_error("Zieldatei konnte nicht geschrieben werden");
            }
            remaining -= static_cast<uint32_t>(count);
        }
        output.flush();
        if (!output) {
            throw std::runtime_error("Zieldatei konnte nicht abgeschlossen werden");
        }
    } catch (...) {
        output.close();
        std::remove(destination.c_str());
        throw;
    }
}
