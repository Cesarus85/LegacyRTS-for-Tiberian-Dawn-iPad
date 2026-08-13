#include "common/hd_asset_pack.h"

#include <cassert>
#include <string>

int main()
{
    const std::string valid =
        "[pack]\n"
        "format=1\n"
        "id=org.tiberiandawn.demo\n"
        "name=Demo HD Pack\n"
        "scale=4\n"
        "\n[assets]\n"
        "cursor.default=ui/cursor.png\n"
        "unit.mtnk=units/mtnk.png\n";
    HDAssetManifest manifest;
    std::string error;
    assert(Parse_HD_Asset_Manifest(valid, manifest, error));
    assert(manifest.FormatVersion == 1);
    assert(manifest.Scale == 4);
    assert(manifest.Assets.size() == 2);
    assert(manifest.Assets[0].Identifier == "cursor.default");
    assert(Is_Safe_HD_Asset_Path("terrain/desert/clear1.png"));
    assert(!Is_Safe_HD_Asset_Path("../outside.png"));
    assert(!Is_Safe_HD_Asset_Path("/absolute.png"));
    assert(!Is_Safe_HD_Asset_Path("folder\\asset.png"));

    std::string traversal = valid;
    traversal.replace(traversal.find("ui/cursor.png"), std::string("ui/cursor.png").size(), "../cursor.png");
    assert(!Parse_HD_Asset_Manifest(traversal, manifest, error));

    std::string duplicate = valid + "unit.mtnk=units/duplicate.png\n";
    assert(!Parse_HD_Asset_Manifest(duplicate, manifest, error));

    std::string unsupported = valid;
    unsupported.replace(unsupported.find("format=1"), std::string("format=1").size(), "format=2");
    assert(!Parse_HD_Asset_Manifest(unsupported, manifest, error));
    return 0;
}
