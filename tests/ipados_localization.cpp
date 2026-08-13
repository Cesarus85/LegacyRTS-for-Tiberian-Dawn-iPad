#include "common/ipados_localization.h"

#include <cassert>
#include <cstring>

int main()
{
    assert(Resolve_IPad_Language(IPAD_LANGUAGE_SYSTEM, "de-DE") == IPAD_EFFECTIVE_GERMAN);
    assert(Resolve_IPad_Language(IPAD_LANGUAGE_SYSTEM, "DE") == IPAD_EFFECTIVE_GERMAN);
    assert(Resolve_IPad_Language(IPAD_LANGUAGE_SYSTEM, "en-US") == IPAD_EFFECTIVE_ENGLISH);
    assert(Resolve_IPad_Language(IPAD_LANGUAGE_SYSTEM, "fr-FR") == IPAD_EFFECTIVE_ENGLISH);
    assert(Resolve_IPad_Language(IPAD_LANGUAGE_GERMAN, "en") == IPAD_EFFECTIVE_GERMAN);
    assert(Resolve_IPad_Language(IPAD_LANGUAGE_ENGLISH, "de") == IPAD_EFFECTIVE_ENGLISH);
    assert(std::strcmp(IPad_Localized_Text(IPAD_EFFECTIVE_ENGLISH, "main_saves"), "Save Games / Files") == 0);
    assert(std::strcmp(IPad_Localized_Text(IPAD_EFFECTIVE_GERMAN, "main_saves"), "Spielstaende / Dateien") == 0);
    assert(std::strcmp(IPad_Localized_Text(IPAD_EFFECTIVE_ENGLISH, "visual_mode_sharp"), "Sharp") == 0);
    assert(std::strcmp(IPad_Localized_Text(IPAD_EFFECTIVE_GERMAN, "visual_mode_classic"), "Klassisch") == 0);
    assert(std::strcmp(IPad_Localized_Text(IPAD_EFFECTIVE_ENGLISH, "visual_artwork_modern"), "Modern") == 0);
    assert(std::strcmp(IPad_Localized_Text(IPAD_EFFECTIVE_GERMAN, "visual_artwork_original"), "Original") == 0);
    assert(std::strcmp(IPad_Localized_Text(IPAD_EFFECTIVE_ENGLISH, "missing-key"), "missing-key") == 0);
    return 0;
}
