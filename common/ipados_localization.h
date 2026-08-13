#ifndef TIBERIAN_DAWN_IPADOS_LOCALIZATION_H
#define TIBERIAN_DAWN_IPADOS_LOCALIZATION_H

enum IPadLanguagePreference
{
    IPAD_LANGUAGE_SYSTEM = 0,
    IPAD_LANGUAGE_GERMAN = 1,
    IPAD_LANGUAGE_ENGLISH = 2,
};

enum IPadLanguage
{
    IPAD_EFFECTIVE_ENGLISH = 0,
    IPAD_EFFECTIVE_GERMAN = 1,
};

IPadLanguage Resolve_IPad_Language(int preference, const char* system_language_tag);
const char* IPad_Localized_Text(IPadLanguage language, const char* key);

#endif
