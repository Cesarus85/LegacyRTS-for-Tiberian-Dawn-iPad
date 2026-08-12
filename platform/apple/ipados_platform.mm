#import <AVFAudio/AVFAudio.h>
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "ipados_platform.h"
#include "common/ipados_touch.h"
#include "third_party/SDL2/include/SDL.h"
#include "third_party/unshieldv3/ISArchiveV3.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
NSString* const ProductDirectory = @"LegacyRTS";
NSString* const GameDirectory = @"vanillatd";

uint32_t ReadLE32(const unsigned char* value)
{
    return static_cast<uint32_t>(value[0]) | (static_cast<uint32_t>(value[1]) << 8)
           | (static_cast<uint32_t>(value[2]) << 16) | (static_cast<uint32_t>(value[3]) << 24);
}

std::string Upper(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

class ISO9660
{
public:
    struct Entry
    {
        uint32_t extent;
        uint32_t size;
        bool directory;
    };

    explicit ISO9660(const std::string& path) : stream(path.c_str(), std::ios::binary)
    {
        if (!stream) {
            throw std::runtime_error("ISO konnte nicht geoeffnet werden");
        }
        std::vector<unsigned char> pvd(2048);
        stream.seekg(16 * 2048, std::ios::beg);
        stream.read(reinterpret_cast<char*>(pvd.data()), pvd.size());
        if (stream.gcount() != static_cast<std::streamsize>(pvd.size()) || pvd[0] != 1
            || std::memcmp(pvd.data() + 1, "CD001", 5) != 0) {
            throw std::runtime_error("Keine ISO-9660-CD");
        }
        volume.assign(reinterpret_cast<char*>(pvd.data() + 40), 32);
        while (!volume.empty() && volume.back() == ' ') {
            volume.pop_back();
        }
        const unsigned char* root = pvd.data() + 156;
        ReadDirectory(ReadLE32(root + 2), ReadLE32(root + 10), "", 0);
    }

    const std::string& Volume() const { return volume; }

    bool Has(const std::string& path) const { return entries.find(Upper(path)) != entries.end(); }

    void Extract(const std::string& source, const std::string& destination)
    {
        std::map<std::string, Entry>::const_iterator it = entries.find(Upper(source));
        if (it == entries.end() || it->second.directory) {
            throw std::runtime_error("CD-Datei fehlt: " + source);
        }
        std::ifstream input(filePath.c_str(), std::ios::binary);
        input.seekg(static_cast<std::streamoff>(it->second.extent) * 2048, std::ios::beg);
        std::ofstream output(destination.c_str(), std::ios::binary | std::ios::trunc);
        std::vector<char> buffer(1024 * 1024);
        uint32_t remaining = it->second.size;
        while (remaining > 0) {
            const std::streamsize count = std::min<uint32_t>(remaining, buffer.size());
            input.read(buffer.data(), count);
            if (input.gcount() != count) {
                throw std::runtime_error("CD-Lesefehler");
            }
            output.write(buffer.data(), count);
            remaining -= static_cast<uint32_t>(count);
        }
        if (!output) {
            throw std::runtime_error("Zieldatei konnte nicht geschrieben werden");
        }
    }

    void SetPath(const std::string& path) { filePath = path; }

private:
    void ReadDirectory(uint32_t extent, uint32_t size, const std::string& prefix, int depth)
    {
        if (depth > 4 || size == 0 || size > 32 * 1024 * 1024) {
            return;
        }
        std::vector<unsigned char> data(size);
        stream.seekg(static_cast<std::streamoff>(extent) * 2048, std::ios::beg);
        stream.read(reinterpret_cast<char*>(data.data()), size);
        size_t offset = 0;
        while (offset < data.size()) {
            const unsigned char recordLength = data[offset];
            if (recordLength == 0) {
                offset = ((offset / 2048) + 1) * 2048;
                continue;
            }
            if (offset + recordLength > data.size() || recordLength < 34) {
                break;
            }
            const unsigned char* record = data.data() + offset;
            const unsigned char nameLength = record[32];
            if (33 + nameLength <= recordLength && !(nameLength == 1 && (record[33] == 0 || record[33] == 1))) {
                std::string name(reinterpret_cast<const char*>(record + 33), nameLength);
                const size_t version = name.find(';');
                if (version != std::string::npos) {
                    name.erase(version);
                }
                const std::string path = prefix.empty() ? name : prefix + "/" + name;
                Entry entry = {ReadLE32(record + 2), ReadLE32(record + 10), (record[25] & 2) != 0};
                entries[Upper(path)] = entry;
                if (entry.directory && (depth == 0 || Upper(path) == "INSTALL")) {
                    ReadDirectory(entry.extent, entry.size, path, depth + 1);
                }
            }
            offset += recordLength;
        }
    }

    std::ifstream stream;
    std::string filePath;
    std::string volume;
    std::map<std::string, Entry> entries;
};

NSURL* LibraryDataURL()
{
    NSURL* applicationSupport = [[[NSFileManager defaultManager]
        URLsForDirectory:NSApplicationSupportDirectory
               inDomains:NSUserDomainMask] firstObject];
    return [[applicationSupport URLByAppendingPathComponent:ProductDirectory isDirectory:YES]
        URLByAppendingPathComponent:GameDirectory isDirectory:YES];
}

NSURL* LegacyDataURL()
{
    NSURL* documents = [[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory
                                                               inDomains:NSUserDomainMask] firstObject];
    return [[[documents URLByAppendingPathComponent:ProductDirectory isDirectory:YES]
        URLByAppendingPathComponent:GameDirectory isDirectory:YES] URLByStandardizingPath];
}

bool Exists(NSURL* directory, NSString* relative)
{
    return [[NSFileManager defaultManager] fileExistsAtPath:[[directory URLByAppendingPathComponent:relative] path]];
}

bool ValidData(NSURL* directory)
{
    NSArray<NSString*>* required = @[
        @"CCLOCAL.MIX", @"CONQUER.MIX", @"DESEICNH.MIX", @"LOCAL.MIX", @"SOUNDS.MIX", @"SPEECH.MIX",
        @"TEMPICNH.MIX", @"TRANSIT.MIX", @"UPDATE.MIX", @"UPDATEC.MIX", @"WINTICNH.MIX",
        @"TEMPERAT.MIX", @"DESERT.MIX", @"WINTER.MIX",
        @"gdi/GENERAL.MIX", @"gdi/MOVIES.MIX", @"gdi/SCORES.MIX",
        @"nod/GENERAL.MIX", @"nod/MOVIES.MIX", @"nod/SCORES.MIX"
    ];
    for (NSString* file in required) {
        if (!Exists(directory, file)) {
            return false;
        }
    }
    return true;
}

void ExcludeFromBackup(NSURL* directory)
{
    [directory setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:nil];
}

void AtomicInstall(NSURL* staging, NSURL* destination)
{
    NSFileManager* manager = [NSFileManager defaultManager];
    [manager createDirectoryAtURL:[destination URLByDeletingLastPathComponent]
      withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL* old = [[destination URLByDeletingLastPathComponent] URLByAppendingPathComponent:@"vanillatd.old" isDirectory:YES];
    [manager removeItemAtURL:old error:nil];
    if ([manager fileExistsAtPath:destination.path]) {
        if (![manager moveItemAtURL:destination toURL:old error:nil]) {
            throw std::runtime_error("Vorhandene Daten konnten nicht gesichert werden");
        }
    }
    NSError* error = nil;
    if (![manager moveItemAtURL:staging toURL:destination error:&error]) {
        [manager moveItemAtURL:old toURL:destination error:nil];
        throw std::runtime_error([[error localizedDescription] UTF8String]);
    }
    [manager removeItemAtURL:old error:nil];
    ExcludeFromBackup(destination);
}

void EnsureImportCapacity(NSURL* directory)
{
    NSDictionary* attributes = [[NSFileManager defaultManager] attributesOfFileSystemForPath:directory.path error:nil];
    const unsigned long long available = [attributes[NSFileSystemFreeSize] unsignedLongLongValue];
    const unsigned long long required = 1200ULL * 1024ULL * 1024ULL;
    if (available > 0 && available < required) {
        throw std::runtime_error("Fuer den sicheren Import werden mindestens 1,2 GB freier Speicher benoetigt");
    }
}

void CopyPreparedDirectory(NSURL* source, NSURL* staging)
{
    NSFileManager* manager = [NSFileManager defaultManager];
    [manager createDirectoryAtURL:staging withIntermediateDirectories:YES attributes:nil error:nil];
    NSError* listingError = nil;
    NSArray<NSURL*>* rootItems = [manager contentsOfDirectoryAtURL:source
                                        includingPropertiesForKeys:@[NSURLIsDirectoryKey]
                                                           options:NSDirectoryEnumerationSkipsHiddenFiles
                                                             error:&listingError];
    if (!rootItems) {
        throw std::runtime_error([[listingError localizedDescription] UTF8String]);
    }
    for (NSURL* item in rootItems) {
        NSNumber* isDirectory = nil;
        [item getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil];
        NSString* name = item.lastPathComponent;
        if (isDirectory.boolValue
            && ([name caseInsensitiveCompare:@"gdi"] == NSOrderedSame
                || [name caseInsensitiveCompare:@"nod"] == NSOrderedSame)) {
            NSString* normalizedSide = [name lowercaseString];
            NSURL* sideDestination = [staging URLByAppendingPathComponent:normalizedSide isDirectory:YES];
            [manager createDirectoryAtURL:sideDestination withIntermediateDirectories:YES attributes:nil error:nil];
            for (NSURL* sideItem in [manager contentsOfDirectoryAtURL:item
                                           includingPropertiesForKeys:@[NSURLIsDirectoryKey]
                                                              options:NSDirectoryEnumerationSkipsHiddenFiles
                                                                error:nil]) {
                if ([sideItem.pathExtension caseInsensitiveCompare:@"MIX"] != NSOrderedSame) continue;
                NSError* error = nil;
                if (![manager copyItemAtURL:sideItem
                                      toURL:[sideDestination URLByAppendingPathComponent:[sideItem.lastPathComponent uppercaseString]]
                                      error:&error]) {
                    throw std::runtime_error([[error localizedDescription] UTF8String]);
                }
            }
        } else if (!isDirectory.boolValue && [item.pathExtension caseInsensitiveCompare:@"MIX"] == NSOrderedSame) {
            NSError* error = nil;
            if (![manager copyItemAtURL:item
                                  toURL:[staging URLByAppendingPathComponent:[name uppercaseString]]
                                  error:&error]) {
                throw std::runtime_error([[error localizedDescription] UTF8String]);
            }
        }
    }
}

void WriteBytes(const std::vector<uint8_t>& bytes, const std::string& path)
{
    std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    if (!output) {
        throw std::runtime_error("Installationsdatei konnte nicht geschrieben werden");
    }
}

void ExtractSetupFiles(const std::string& setupPath, NSURL* staging)
{
    ISArchiveV3 archive(setupPath);
    const char* names[] = {"CCLOCAL.MIX", "DESEICNH.MIX", "LOCAL.MIX", "SPEECH.MIX", "TEMPICNH.MIX",
                           "TRANSIT.MIX", "UPDATE.MIX", "UPDATEC.MIX", "WINTICNH.MIX"};
    for (const char* name : names) {
        const std::string archived = std::string("C&C95\\") + name;
        if (archive.exists(archived)) {
            WriteBytes(archive.decompress(archived), [[[staging URLByAppendingPathComponent:@(name)] path] UTF8String]);
        }
    }
}

void ExtractISOs(NSArray<NSURL*>* urls, NSURL* staging)
{
    NSFileManager* manager = [NSFileManager defaultManager];
    [manager createDirectoryAtURL:staging withIntermediateDirectories:YES attributes:nil error:nil];
    [manager createDirectoryAtURL:[staging URLByAppendingPathComponent:@"gdi"] withIntermediateDirectories:YES attributes:nil error:nil];
    [manager createDirectoryAtURL:[staging URLByAppendingPathComponent:@"nod"] withIntermediateDirectories:YES attributes:nil error:nil];

    bool haveGDI = false;
    bool haveNod = false;
    bool setupDone = false;
    const char* common[] = {"CONQUER.MIX", "SOUNDS.MIX", "DESERT.MIX", "TEMPERAT.MIX", "WINTER.MIX"};
    const char* faction[] = {"GENERAL.MIX", "MOVIES.MIX", "SCORES.MIX"};
    for (NSURL* url in urls) {
        const bool accessed = [url startAccessingSecurityScopedResource];
        try {
            ISO9660 iso(url.fileSystemRepresentation);
            iso.SetPath(url.fileSystemRepresentation);
            const std::string volume = Upper(iso.Volume());
            NSString* side = volume.find("GDI") != std::string::npos ? @"gdi" :
                             (volume.find("NOD") != std::string::npos ? @"nod" : nil);
            if (side == nil) {
                throw std::runtime_error("Eine CD ist weder GDI95 noch NOD95");
            }
            haveGDI = haveGDI || [side isEqualToString:@"gdi"];
            haveNod = haveNod || [side isEqualToString:@"nod"];
            for (const char* name : common) {
                NSURL* destination = [staging URLByAppendingPathComponent:@(name)];
                if (iso.Has(name) && ![manager fileExistsAtPath:destination.path]) {
                    iso.Extract(name, destination.fileSystemRepresentation);
                }
            }
            for (const char* name : faction) {
                iso.Extract(name, [[[staging URLByAppendingPathComponent:side] URLByAppendingPathComponent:@(name)] path].UTF8String);
            }
            if (!setupDone && iso.Has("INSTALL/SETUP.Z")) {
                NSURL* setup = [[staging URLByDeletingLastPathComponent] URLByAppendingPathComponent:@"SETUP.Z"];
                iso.Extract("INSTALL/SETUP.Z", setup.fileSystemRepresentation);
                ExtractSetupFiles(setup.fileSystemRepresentation, staging);
                [manager removeItemAtURL:setup error:nil];
                setupDone = true;
            }
        } catch (...) {
            if (accessed) [url stopAccessingSecurityScopedResource];
            throw;
        }
        if (accessed) [url stopAccessingSecurityScopedResource];
    }
    if (!haveGDI || !haveNod) {
        throw std::runtime_error("Bitte die GDI- und die Nod-CD gemeinsam auswaehlen");
    }
}

} // namespace

@interface LegacyRTSImportGuideController : UIViewController <UIDocumentPickerDelegate>
@property(nonatomic, strong) NSArray<NSURL*>* URLs;
@property(nonatomic) BOOL finished;
@end

@implementation LegacyRTSImportGuideController

- (UILabel*)guideLabel:(NSString*)text font:(UIFont*)font color:(UIColor*)color
{
    UILabel* label = [UILabel new];
    label.text = text;
    label.font = font;
    label.textColor = color;
    label.numberOfLines = 0;
    label.adjustsFontForContentSizeCategory = YES;
    return label;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    UIColor* background = [UIColor colorWithRed:0.035 green:0.055 blue:0.055 alpha:1.0];
    UIColor* card = [UIColor colorWithRed:0.075 green:0.105 blue:0.095 alpha:1.0];
    UIColor* primary = [UIColor colorWithRed:0.88 green:0.62 blue:0.10 alpha:1.0];
    UIColor* secondary = [UIColor colorWithWhite:0.80 alpha:1.0];
    self.view.backgroundColor = background;

    UIScrollView* scroll = [UIScrollView new];
    scroll.translatesAutoresizingMaskIntoConstraints = NO;
    scroll.alwaysBounceVertical = YES;
    scroll.accessibilityLabel = @"Anleitung zum Import der Original-Spieldaten";
    [self.view addSubview:scroll];

    UIStackView* content = [UIStackView new];
    content.translatesAutoresizingMaskIntoConstraints = NO;
    content.axis = UILayoutConstraintAxisVertical;
    content.spacing = 14;
    [scroll addSubview:content];

    UIImageView* symbol = [[UIImageView alloc] initWithImage:[UIImage systemImageNamed:@"externaldrive.badge.plus"]];
    symbol.translatesAutoresizingMaskIntoConstraints = NO;
    symbol.tintColor = primary;
    symbol.contentMode = UIViewContentModeScaleAspectFit;
    [symbol.heightAnchor constraintEqualToConstant:58].active = YES;
    symbol.isAccessibilityElement = NO;
    [content addArrangedSubview:symbol];

    UILabel* title = [self guideLabel:@"Original-Spieldaten importieren"
                                 font:[UIFont preferredFontForTextStyle:UIFontTextStyleLargeTitle]
                                color:UIColor.whiteColor];
    title.font = [UIFont systemFontOfSize:34 weight:UIFontWeightBold];
    title.textAlignment = NSTextAlignmentCenter;
    title.accessibilityTraits = UIAccessibilityTraitHeader;
    [content addArrangedSubview:title];

    UILabel* introduction = [self guideLabel:
        @"Legacy RTS enthält aus rechtlichen Gründen keine Originaldaten. Für das Spiel brauchst du deine eigenen Command & Conquer Gold-CDs. Es werden keine Dateien hochgeladen."
                                              font:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]
                                             color:secondary];
    introduction.textAlignment = NSTextAlignmentCenter;
    [content addArrangedSubview:introduction];

    UIView* requirementsCard = [UIView new];
    requirementsCard.backgroundColor = card;
    requirementsCard.layer.cornerRadius = 16;
    requirementsCard.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(18, 18, 18, 18);
    UIStackView* requirements = [UIStackView new];
    requirements.translatesAutoresizingMaskIntoConstraints = NO;
    requirements.axis = UILayoutConstraintAxisVertical;
    requirements.spacing = 9;
    [requirementsCard addSubview:requirements];
    [NSLayoutConstraint activateConstraints:@[
        [requirements.leadingAnchor constraintEqualToAnchor:requirementsCard.layoutMarginsGuide.leadingAnchor],
        [requirements.trailingAnchor constraintEqualToAnchor:requirementsCard.layoutMarginsGuide.trailingAnchor],
        [requirements.topAnchor constraintEqualToAnchor:requirementsCard.layoutMarginsGuide.topAnchor],
        [requirements.bottomAnchor constraintEqualToAnchor:requirementsCard.layoutMarginsGuide.bottomAnchor]
    ]];
    UILabel* needTitle = [self guideLabel:@"Du benötigst"
                                     font:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
                                    color:UIColor.whiteColor];
    needTitle.accessibilityTraits = UIAccessibilityTraitHeader;
    [requirements addArrangedSubview:needTitle];
    [requirements addArrangedSubview:[self guideLabel:
        @"• CNC95_GDI.iso – die originale GDI Gold-CD\n• CNC95_Nod.iso – die originale Nod Gold-CD\n• mindestens 1,2 GB freien Speicher"
                                                  font:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]
                                                 color:secondary]];
    [content addArrangedSubview:requirementsCard];

    UILabel* stepsTitle = [self guideLabel:@"So funktioniert es"
                                      font:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
                                     color:UIColor.whiteColor];
    stepsTitle.accessibilityTraits = UIAccessibilityTraitHeader;
    [content addArrangedSubview:stepsTitle];
    [content addArrangedSubview:[self guideLabel:
        @"1. Lege beide ISO-Dateien in Dateien, iCloud Drive oder auf einem angeschlossenen USB-Laufwerk ab.\n\n2. Tippe unten auf „Spieldaten auswählen“.\n\n3. Markiere GDI und Nod gemeinsam und bestätige mit „Öffnen“.\n\n4. Lass die App geöffnet, während sie die CDs prüft und installiert."
                                                font:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]
                                               color:secondary]];

    UILabel* alternative = [self guideLabel:
        @"Alternativ kannst du genau einen bereits vorbereiteten vanillatd-Ordner auswählen. Ultimate Collection, The First Decade und Remastered Collection werden nicht unterstützt."
                                             font:[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
                                            color:[UIColor colorWithWhite:0.66 alpha:1.0]];
    [content addArrangedSubview:alternative];

    UIStackView* actions = [UIStackView new];
    actions.translatesAutoresizingMaskIntoConstraints = NO;
    actions.axis = UILayoutConstraintAxisVertical;
    actions.spacing = 8;
    actions.backgroundColor = background;
    actions.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(12, 24, 18, 24);
    actions.layoutMarginsRelativeArrangement = YES;
    [self.view addSubview:actions];

    UIButton* selectButton = [UIButton buttonWithType:UIButtonTypeSystem];
    selectButton.translatesAutoresizingMaskIntoConstraints = NO;
    [selectButton setTitle:@"Spieldaten auswählen" forState:UIControlStateNormal];
    [selectButton setTitleColor:[UIColor colorWithRed:0.04 green:0.05 blue:0.04 alpha:1.0] forState:UIControlStateNormal];
    selectButton.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    selectButton.titleLabel.adjustsFontForContentSizeCategory = YES;
    selectButton.backgroundColor = primary;
    selectButton.layer.cornerRadius = 12;
    selectButton.accessibilityHint = @"Öffnet Dateien. Wähle dort die GDI- und die Nod-ISO gemeinsam aus.";
    [selectButton.heightAnchor constraintGreaterThanOrEqualToConstant:52].active = YES;
    [selectButton addTarget:self action:@selector(selectSources:) forControlEvents:UIControlEventTouchUpInside];
    [actions addArrangedSubview:selectButton];

    UIButton* laterButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [laterButton setTitle:@"Später einrichten" forState:UIControlStateNormal];
    [laterButton setTitleColor:secondary forState:UIControlStateNormal];
    laterButton.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    [laterButton.heightAnchor constraintGreaterThanOrEqualToConstant:44].active = YES;
    laterButton.accessibilityHint = @"Schließt die App. Beim nächsten Start erscheint diese Anleitung erneut.";
    [laterButton addTarget:self action:@selector(importLater:) forControlEvents:UIControlEventTouchUpInside];
    [actions addArrangedSubview:laterButton];

    UILayoutGuide* safe = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [scroll.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],
        [scroll.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor],
        [scroll.topAnchor constraintEqualToAnchor:safe.topAnchor],
        [scroll.bottomAnchor constraintEqualToAnchor:actions.topAnchor],
        [content.leadingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.leadingAnchor constant:24],
        [content.trailingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.trailingAnchor constant:-24],
        [content.topAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.topAnchor constant:24],
        [content.bottomAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.bottomAnchor constant:-24],
        [content.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor constant:-48],
        [actions.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [actions.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [actions.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor]
    ]];
}

- (void)selectSources:(id)sender
{
    UIDocumentPickerViewController* picker = [[UIDocumentPickerViewController alloc]
        initForOpeningContentTypes:@[UTTypeDiskImage, UTTypeFolder] asCopy:NO];
    picker.allowsMultipleSelection = YES;
    picker.delegate = self;
    picker.title = @"GDI und Nod gemeinsam auswählen";
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)importLater:(id)sender
{
    self.URLs = @[];
    self.finished = YES;
}

- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls
{
    self.URLs = urls;
    self.finished = YES;
}
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller
{
    // Return to the guide so the instructions remain available. "Später"
    // is an explicit decision on the guide itself.
}
@end

namespace
{

NSArray<NSURL*>* PickSources()
{
    __block NSArray<NSURL*>* result = nil;
    void (^present)(void) = ^{
        LegacyRTSImportGuideController* guide = [LegacyRTSImportGuideController new];

        UIWindowScene* scene = nil;
        for (UIScene* candidate in UIApplication.sharedApplication.connectedScenes) {
            if ([candidate isKindOfClass:UIWindowScene.class] && candidate.activationState != UISceneActivationStateUnattached) {
                scene = (UIWindowScene*)candidate;
                break;
            }
        }
        UIWindow* host = [[UIWindow alloc] initWithWindowScene:scene];
        host.rootViewController = guide;
        host.windowLevel = UIWindowLevelAlert;
        [host makeKeyAndVisible];

        while (!guide.finished) {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
        }
        result = guide.URLs;
        host.hidden = YES;
    };
    if (NSThread.isMainThread) {
        present();
    } else {
        dispatch_sync(dispatch_get_main_queue(), present);
    }
    return result ?: @[];
}

UIWindowScene* ActiveWindowScene()
{
    for (UIScene* candidate in UIApplication.sharedApplication.connectedScenes) {
        if ([candidate isKindOfClass:UIWindowScene.class]
            && candidate.activationState != UISceneActivationStateUnattached) {
            return (UIWindowScene*)candidate;
        }
    }
    return nil;
}

void RunImportTask(NSString* status, const std::function<void()>& operation)
{
    UIWindow* host = [[UIWindow alloc] initWithWindowScene:ActiveWindowScene()];
    UIViewController* controller = [UIViewController new];
    controller.view.backgroundColor = [UIColor colorWithRed:0.035 green:0.055 blue:0.055 alpha:1.0];
    host.rootViewController = controller;
    host.windowLevel = UIWindowLevelAlert;
    [host makeKeyAndVisible];

    UIActivityIndicatorView* activity = [[UIActivityIndicatorView alloc]
        initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleLarge];
    activity.translatesAutoresizingMaskIntoConstraints = NO;
    activity.color = [UIColor colorWithRed:0.88 green:0.62 blue:0.10 alpha:1.0];
    [activity startAnimating];

    UILabel* title = [UILabel new];
    title.translatesAutoresizingMaskIntoConstraints = NO;
    title.text = @"Spieldaten werden eingerichtet";
    title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle1];
    title.textColor = UIColor.whiteColor;
    title.textAlignment = NSTextAlignmentCenter;
    title.adjustsFontForContentSizeCategory = YES;
    title.numberOfLines = 0;

    UILabel* detail = [UILabel new];
    detail.translatesAutoresizingMaskIntoConstraints = NO;
    detail.text = status;
    detail.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    detail.textColor = [UIColor colorWithWhite:0.80 alpha:1.0];
    detail.textAlignment = NSTextAlignmentCenter;
    detail.adjustsFontForContentSizeCategory = YES;
    detail.numberOfLines = 0;

    UILabel* note = [UILabel new];
    note.translatesAutoresizingMaskIntoConstraints = NO;
    note.text = @"Bitte die App geöffnet lassen. Die ISO-Dateien werden nur gelesen und nicht kopiert oder hochgeladen.";
    note.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    note.textColor = [UIColor colorWithWhite:0.62 alpha:1.0];
    note.textAlignment = NSTextAlignmentCenter;
    note.adjustsFontForContentSizeCategory = YES;
    note.numberOfLines = 0;

    UIStackView* stack = [[UIStackView alloc] initWithArrangedSubviews:@[activity, title, detail, note]];
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    stack.axis = UILayoutConstraintAxisVertical;
    stack.alignment = UIStackViewAlignmentFill;
    stack.spacing = 18;
    [controller.view addSubview:stack];
    [NSLayoutConstraint activateConstraints:@[
        [stack.centerYAnchor constraintEqualToAnchor:controller.view.safeAreaLayoutGuide.centerYAnchor],
        [stack.leadingAnchor constraintEqualToAnchor:controller.view.safeAreaLayoutGuide.leadingAnchor constant:36],
        [stack.trailingAnchor constraintEqualToAnchor:controller.view.safeAreaLayoutGuide.trailingAnchor constant:-36]
    ]];

    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification, title);
    __block BOOL finished = NO;
    __block NSString* failure = nil;
    const std::function<void()> work = operation;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSString* localFailure = nil;
        @autoreleasepool {
            try {
                work();
            } catch (const std::exception& error) {
                localFailure = @(error.what());
            } catch (...) {
                localFailure = @"Unbekannter Fehler beim Import";
            }
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            failure = localFailure;
            finished = YES;
        });
    });

    while (!finished) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                  beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
    }
    host.hidden = YES;
    if (failure) {
        throw std::runtime_error(failure.UTF8String);
    }
}

bool ShowImportError(NSString* error)
{
    __block BOOL finished = NO;
    __block BOOL retry = NO;
    UIWindow* host = [[UIWindow alloc] initWithWindowScene:ActiveWindowScene()];
    UIViewController* controller = [UIViewController new];
    controller.view.backgroundColor = [UIColor colorWithRed:0.035 green:0.055 blue:0.055 alpha:1.0];
    host.rootViewController = controller;
    host.windowLevel = UIWindowLevelAlert;
    [host makeKeyAndVisible];

    NSString* help = [NSString stringWithFormat:
        @"%@\n\nPrüfe bitte:\n• GDI- und Nod-Gold-ISO gemeinsam ausgewählt\n• Dateien vollständig auf dem iPad, in iCloud Drive oder auf USB verfügbar\n• mindestens 1,2 GB freier Speicher\n\nAndere C&C-Ausgaben werden nicht unterstützt.",
        error];
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Import nicht abgeschlossen"
                                                                    message:help
                                                             preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Später"
                                              style:UIAlertActionStyleCancel
                                            handler:^(UIAlertAction* action) {
        finished = YES;
    }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Erneut versuchen"
                                              style:UIAlertActionStyleDefault
                                            handler:^(UIAlertAction* action) {
        retry = YES;
        finished = YES;
    }]];
    [controller presentViewController:alert animated:NO completion:nil];
    while (!finished) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                  beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
    }
    host.hidden = YES;
    return retry;
}

void ShowImportCompleted(void)
{
    __block BOOL finished = NO;
    UIWindow* host = [[UIWindow alloc] initWithWindowScene:ActiveWindowScene()];
    UIViewController* controller = [UIViewController new];
    controller.view.backgroundColor = [UIColor colorWithRed:0.035 green:0.055 blue:0.055 alpha:1.0];
    host.rootViewController = controller;
    host.windowLevel = UIWindowLevelAlert;
    [host makeKeyAndVisible];
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Import abgeschlossen"
        message:@"GDI- und Nod-Daten wurden geprüft und sicher installiert. Spielstände und Einstellungen bleiben getrennt in Dateien zugänglich."
        preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Spiel starten"
                                              style:UIAlertActionStyleDefault
                                            handler:^(UIAlertAction* action) {
        finished = YES;
    }]];
    [controller presentViewController:alert animated:NO completion:nil];
    while (!finished) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                  beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
    }
    host.hidden = YES;
}

void ShowMessage(NSString* title, NSString* message)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, message);
        NSLog(@"Legacy RTS: %@: %@", title, message);
    });
}

UILabel* CompactWarningLabel()
{
    static UILabel* label = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        label = [UILabel new];
        label.text = @"Fenster vergroessern – die Spielflaeche ist hier zu klein";
        label.textAlignment = NSTextAlignmentCenter;
        label.numberOfLines = 2;
        label.textColor = UIColor.whiteColor;
        label.backgroundColor = [UIColor colorWithRed:0.45 green:0.08 blue:0.06 alpha:0.94];
        label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
        label.adjustsFontForContentSizeCategory = YES;
        label.isAccessibilityElement = YES;
        label.accessibilityTraits = UIAccessibilityTraitStaticText;
        label.hidden = YES;
    });
    return label;
}
}

bool LegacyRTS_PrepareGameData(void)
{
    @autoreleasepool {
        LegacyRTS_ConfigureAudioSession();
        NSURL* destination = LibraryDataURL();
        if (ValidData(destination)) {
            ExcludeFromBackup(destination);
            return true;
        }

        NSFileManager* manager = [NSFileManager defaultManager];
        NSURL* parent = [destination URLByDeletingLastPathComponent];
        [manager createDirectoryAtURL:parent withIntermediateDirectories:YES attributes:nil error:nil];
        NSURL* staging = [parent URLByAppendingPathComponent:@"vanillatd.importing" isDirectory:YES];
        [manager removeItemAtURL:staging error:nil];

        NSURL* legacy = LegacyDataURL();
        while (ValidData(legacy)) {
            try {
                RunImportTask(@"Vorhandene Spieldaten werden geprüft und in den optimierten Speicher verschoben.", [&] {
                    EnsureImportCapacity(parent);
                    [manager removeItemAtURL:staging error:nil];
                    CopyPreparedDirectory(legacy, staging);
                    if (!ValidData(staging)) throw std::runtime_error("Die Migration ist unvollständig");
                    AtomicInstall(staging, destination);
                    // Remove only immutable assets after the verified copy. Saves and INI files remain visible in Documents.
                    NSArray<NSString*>* assets = [manager contentsOfDirectoryAtPath:legacy.path error:nil];
                    for (NSString* name in assets) {
                        if ([name.pathExtension caseInsensitiveCompare:@"MIX"] == NSOrderedSame
                            || [name caseInsensitiveCompare:@"gdi"] == NSOrderedSame
                            || [name caseInsensitiveCompare:@"nod"] == NSOrderedSame) {
                            [manager removeItemAtURL:[legacy URLByAppendingPathComponent:name] error:nil];
                        }
                    }
                });
                return true;
            } catch (const std::exception& error) {
                [manager removeItemAtURL:staging error:nil];
                if (!ShowImportError(@(error.what()))) return false;
            }
        }

        while (true) {
            NSArray<NSURL*>* selected = PickSources();
            if (selected.count == 0) return false;
            try {
                RunImportTask(@"Die ausgewählten Quellen werden gelesen, extrahiert und vollständig geprüft.", [&] {
                    EnsureImportCapacity(parent);
                    [manager removeItemAtURL:staging error:nil];
                    NSURL* first = selected.firstObject;
                    NSNumber* directory = nil;
                    [first getResourceValue:&directory forKey:NSURLIsDirectoryKey error:nil];
                    if (selected.count == 1 && directory.boolValue) {
                        const bool accessed = [first startAccessingSecurityScopedResource];
                        try {
                            CopyPreparedDirectory(first, staging);
                        } catch (...) {
                            if (accessed) [first stopAccessingSecurityScopedResource];
                            throw;
                        }
                        if (accessed) [first stopAccessingSecurityScopedResource];
                    } else {
                        ExtractISOs(selected, staging);
                    }
                    if (!ValidData(staging)) {
                        throw std::runtime_error("Die Quellen enthalten nicht alle benötigten C&C-Gold-Dateien");
                    }
                    AtomicInstall(staging, destination);
                });
                ShowImportCompleted();
                return true;
            } catch (const std::exception& error) {
                [manager removeItemAtURL:staging error:nil];
                if (!ShowImportError(@(error.what()))) return false;
            }
        }
    }
}

void LegacyRTS_ConfigureAudioSession(void)
{
    @autoreleasepool {
        AVAudioSession* session = AVAudioSession.sharedInstance;
        NSError* error = nil;
        [session setCategory:AVAudioSessionCategoryAmbient
                        mode:AVAudioSessionModeDefault
                     options:AVAudioSessionCategoryOptionAllowBluetoothA2DP
                       error:&error];
        [session setActive:YES error:&error];

        static dispatch_once_t once;
        dispatch_once(&once, ^{
            NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
            [center addObserverForName:AVAudioSessionInterruptionNotification object:session queue:NSOperationQueue.mainQueue
                            usingBlock:^(NSNotification* note) {
                const NSInteger type = [note.userInfo[AVAudioSessionInterruptionTypeKey] integerValue];
                SDL_Event event = {};
                event.type = SDL_USEREVENT;
                event.user.code = type == AVAudioSessionInterruptionTypeBegan ? IPADOS_EVENT_AUDIO_PAUSE : IPADOS_EVENT_AUDIO_RESUME;
                SDL_PushEvent(&event);
            }];
            [center addObserverForName:AVAudioSessionRouteChangeNotification object:session queue:NSOperationQueue.mainQueue
                            usingBlock:^(NSNotification* note) {
                SDL_Event event = {};
                event.type = SDL_USEREVENT;
                event.user.code = IPADOS_EVENT_AUDIO_ROUTE_CHANGED;
                SDL_PushEvent(&event);
            }];
            [center addObserverForName:AVAudioSessionMediaServicesWereResetNotification object:session queue:NSOperationQueue.mainQueue
                            usingBlock:^(NSNotification* note) {
                LegacyRTS_ConfigureAudioSession();
                SDL_Event event = {};
                event.type = SDL_USEREVENT;
                event.user.code = IPADOS_EVENT_AUDIO_RESUME;
                SDL_PushEvent(&event);
            }];
        });
    }
}

void LegacyRTS_SetCompactWindowWarning(bool visible)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        UILabel* label = CompactWarningLabel();
        UIWindow* window = UIApplication.sharedApplication.keyWindow;
        if (!window) {
            for (UIWindowScene* scene in UIApplication.sharedApplication.connectedScenes) {
                if (![scene isKindOfClass:UIWindowScene.class]) continue;
                for (UIWindow* candidate in scene.windows) if (candidate.isKeyWindow) window = candidate;
            }
        }
        if (visible && window && label.superview != window) {
            [label removeFromSuperview];
            label.translatesAutoresizingMaskIntoConstraints = NO;
            [window addSubview:label];
            [NSLayoutConstraint activateConstraints:@[
                [label.leadingAnchor constraintEqualToAnchor:window.safeAreaLayoutGuide.leadingAnchor constant:12],
                [label.trailingAnchor constraintEqualToAnchor:window.safeAreaLayoutGuide.trailingAnchor constant:-12],
                [label.topAnchor constraintEqualToAnchor:window.safeAreaLayoutGuide.topAnchor constant:12]
            ]];
        }
        if (label.hidden == visible) {
            label.hidden = !visible;
            if (visible) UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, label.text);
        }
    });
}
