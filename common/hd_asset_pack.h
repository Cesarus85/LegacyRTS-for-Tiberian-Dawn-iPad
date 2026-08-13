#ifndef TIBERIAN_DAWN_HD_ASSET_PACK_H
#define TIBERIAN_DAWN_HD_ASSET_PACK_H

#include <string>
#include <vector>

struct HDAssetEntry
{
    std::string Identifier;
    std::string RelativePath;
};

struct HDAssetManifest
{
    int FormatVersion = 0;
    int Scale = 0;
    std::string Identifier;
    std::string Name;
    std::vector<HDAssetEntry> Assets;
};

bool Is_Safe_HD_Asset_Path(const std::string& path);
bool Parse_HD_Asset_Manifest(const std::string& text, HDAssetManifest& manifest, std::string& error);

class HDAssetPack
{
public:
    bool Load(const std::string& directory, std::string& error);
    void Clear();

    bool Is_Loaded() const;
    const HDAssetManifest& Manifest() const;
    const HDAssetEntry* Find(const std::string& identifier) const;
    std::string Absolute_Path(const HDAssetEntry& entry) const;

private:
    bool Loaded = false;
    std::string Directory;
    HDAssetManifest Data;
};

#endif
