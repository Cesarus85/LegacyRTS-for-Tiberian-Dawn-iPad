#include "hd_asset_pack.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>

namespace
{
const std::size_t MaximumManifestBytes = 1024 * 1024;
const std::size_t MaximumAssets = 4096;

std::string Trim(const std::string& value)
{
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) ++first;
    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) --last;
    return value.substr(first, last - first);
}

std::string Lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool Is_Stable_Identifier(const std::string& value, bool asset)
{
    if (value.empty() || value.size() > (asset ? 128 : 96) || value.front() == '.' || value.back() == '.') {
        return false;
    }
    for (unsigned char character : value) {
        if (std::islower(character) || std::isdigit(character) || character == '.' || character == '_'
            || character == '-') continue;
        return false;
    }
    return true;
}

bool Parse_Integer(const std::string& text, int& value)
{
    if (text.empty()) return false;
    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || *end != '\0'
        || parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) return false;
    value = static_cast<int>(parsed);
    return true;
}

bool Has_PNG_Signature(const std::string& path)
{
    static const unsigned char Signature[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file) return false;
    unsigned char bytes[sizeof(Signature)] = {};
    file.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
    return file.gcount() == static_cast<std::streamsize>(sizeof(bytes))
           && std::equal(bytes, bytes + sizeof(bytes), Signature);
}
}

bool Is_Safe_HD_Asset_Path(const std::string& path)
{
    if (path.empty() || path.size() > 240 || path.front() == '/' || path.back() == '/' || path.find('\\') != std::string::npos
        || path.find(':') != std::string::npos) return false;

    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t slash = path.find('/', start);
        const std::string component = path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (component.empty() || component == "." || component == "..") return false;
        start = slash == std::string::npos ? path.size() : slash + 1;
    }

    const std::string lower = Lowercase(path);
    return lower.size() > 4 && lower.compare(lower.size() - 4, 4, ".png") == 0;
}

bool Parse_HD_Asset_Manifest(const std::string& text, HDAssetManifest& manifest, std::string& error)
{
    manifest = HDAssetManifest();
    error.clear();
    if (text.empty() || text.size() > MaximumManifestBytes) {
        error = "manifest is empty or larger than 1 MB";
        return false;
    }

    enum Section
    {
        SECTION_NONE,
        SECTION_PACK,
        SECTION_ASSETS,
    } section = SECTION_NONE;
    bool have_format = false;
    bool have_identifier = false;
    bool have_name = false;
    bool have_scale = false;
    std::set<std::string> asset_identifiers;
    std::istringstream lines(text);
    std::string line;
    int line_number = 0;
    while (std::getline(lines, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.size() > 4096) {
            error = "line " + std::to_string(line_number) + " is too long";
            return false;
        }
        if (line.front() == '[' && line.back() == ']') {
            const std::string name = Lowercase(Trim(line.substr(1, line.size() - 2)));
            if (name == "pack") section = SECTION_PACK;
            else if (name == "assets") section = SECTION_ASSETS;
            else {
                error = "unknown section on line " + std::to_string(line_number);
                return false;
            }
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos || section == SECTION_NONE) {
            error = "expected key=value on line " + std::to_string(line_number);
            return false;
        }
        const std::string key = Lowercase(Trim(line.substr(0, equals)));
        const std::string value = Trim(line.substr(equals + 1));
        if (key.empty() || value.empty()) {
            error = "empty key or value on line " + std::to_string(line_number);
            return false;
        }

        if (section == SECTION_PACK) {
            if (key == "format") {
                if (have_format || !Parse_Integer(value, manifest.FormatVersion)) {
                    error = "invalid or duplicate pack format";
                    return false;
                }
                have_format = true;
            } else if (key == "id") {
                if (have_identifier || !Is_Stable_Identifier(value, false)) {
                    error = "invalid or duplicate pack id";
                    return false;
                }
                manifest.Identifier = value;
                have_identifier = true;
            } else if (key == "name") {
                if (have_name || value.size() > 96) {
                    error = "invalid or duplicate pack name";
                    return false;
                }
                manifest.Name = value;
                have_name = true;
            } else if (key == "scale") {
                if (have_scale || !Parse_Integer(value, manifest.Scale)) {
                    error = "invalid or duplicate pack scale";
                    return false;
                }
                have_scale = true;
            } else {
                error = "unknown pack key on line " + std::to_string(line_number);
                return false;
            }
        } else {
            if (!Is_Stable_Identifier(key, true) || !Is_Safe_HD_Asset_Path(value)) {
                error = "invalid asset id or path on line " + std::to_string(line_number);
                return false;
            }
            if (!asset_identifiers.insert(key).second) {
                error = "duplicate asset id on line " + std::to_string(line_number);
                return false;
            }
            if (manifest.Assets.size() >= MaximumAssets) {
                error = "manifest contains more than 4096 assets";
                return false;
            }
            manifest.Assets.push_back({key, value});
        }
    }

    if (!have_format || manifest.FormatVersion != 1) error = "unsupported or missing pack format";
    else if (!have_identifier || !have_name) error = "pack id or name is missing";
    else if (!have_scale || (manifest.Scale != 2 && manifest.Scale != 4)) error = "pack scale must be 2 or 4";
    else if (manifest.Assets.empty()) error = "pack contains no assets";
    if (!error.empty()) return false;

    std::sort(manifest.Assets.begin(), manifest.Assets.end(), [](const HDAssetEntry& first, const HDAssetEntry& second) {
        return first.Identifier < second.Identifier;
    });
    return true;
}

bool HDAssetPack::Load(const std::string& directory, std::string& error)
{
    Clear();
    error.clear();
    const std::string manifest_path = directory + (directory.empty() || directory.back() == '/' ? "" : "/") + "manifest.ini";
    std::ifstream file(manifest_path.c_str(), std::ios::binary);
    if (!file) {
        error = "manifest.ini not found";
        return false;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    if (!file.good() && !file.eof()) {
        error = "manifest.ini could not be read";
        return false;
    }
    if (!Parse_HD_Asset_Manifest(contents.str(), Data, error)) return false;

    Directory = directory;
    for (const HDAssetEntry& entry : Data.Assets) {
        if (!Has_PNG_Signature(Absolute_Path(entry))) {
            error = "missing or invalid PNG for asset " + entry.Identifier;
            Clear();
            return false;
        }
    }
    Loaded = true;
    return true;
}

void HDAssetPack::Clear()
{
    Loaded = false;
    Directory.clear();
    Data = HDAssetManifest();
}

bool HDAssetPack::Is_Loaded() const
{
    return Loaded;
}

const HDAssetManifest& HDAssetPack::Manifest() const
{
    return Data;
}

const HDAssetEntry* HDAssetPack::Find(const std::string& identifier) const
{
    const std::string key = Lowercase(identifier);
    const auto match = std::lower_bound(Data.Assets.begin(), Data.Assets.end(), key,
                                        [](const HDAssetEntry& entry, const std::string& value) {
        return entry.Identifier < value;
    });
    return match != Data.Assets.end() && match->Identifier == key ? &*match : nullptr;
}

std::string HDAssetPack::Absolute_Path(const HDAssetEntry& entry) const
{
    return Directory + (Directory.empty() || Directory.back() == '/' ? "" : "/") + entry.RelativePath;
}
