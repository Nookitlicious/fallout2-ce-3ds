#include "sfall_ini.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "config.h"
#include "platform_compat.h"

namespace fallout {

/// The max length of `fileName` chunk in the triplet.
static constexpr size_t kFileNameMaxSize = 63;

/// The max length of `section` chunk in the triplet.
static constexpr size_t kSectionMaxSize = 32;

/// Special .ini file names which are accessed without adding base path.
static constexpr const char* kSystemConfigFileNames[] = {
    "ddraw.ini",
    "f2_res.ini",
};

static char basePath[COMPAT_MAX_PATH];

/// Parses "fileName|section|key" triplet into parts. `fileName` and `section`
/// chunks are copied into appropriate variables. Returns the pointer to `key`,
/// or `nullptr` on any error.
static const char* parse_ini_triplet(const char* triplet, char* fileName, char* section)
{
    const char* fileNameSectionSep = strchr(triplet, '|');
    if (fileNameSectionSep == nullptr) {
        return nullptr;
    }

    size_t fileNameLength = fileNameSectionSep - triplet;
    if (fileNameLength > kFileNameMaxSize) {
        return nullptr;
    }

    const char* sectionKeySep = strchr(fileNameSectionSep + 1, '|');
    if (sectionKeySep == nullptr) {
        return nullptr;
    }

    size_t sectionLength = sectionKeySep - fileNameSectionSep - 1;
    if (sectionLength > kSectionMaxSize) {
        return nullptr;
    }

    strncpy(fileName, triplet, fileNameLength);
    fileName[fileNameLength] = '\0';

    strncpy(section, fileNameSectionSep + 1, sectionLength);
    section[sectionLength] = '\0';

    return sectionKeySep + 1;
}

/// Returns `true` if given `fileName` is a special system .ini file name.
static bool is_system_file_name(const char* fileName)
{
    for (auto& systemFileName : kSystemConfigFileNames) {
        if (compat_stricmp(systemFileName, fileName) == 0) {
            return true;
        }
    }

    return false;
}

void sfall_ini_set_base_path(const char* path)
{
    if (path != nullptr) {
        snprintf(basePath, sizeof(basePath), "%s", path);

        size_t length = strlen(basePath);
        if (length > 0) {
            if (basePath[length - 1] == '\\' || basePath[length - 1] == '/') {
                basePath[length - 1] = '\0';
            }
        }
    } else {
        basePath[0] = '\0';
    }
}

bool sfall_ini_get_int(const char* triplet, int* value)
{
    char string[20];
    if (!sfall_ini_get_string(triplet, string, sizeof(string))) {
        return false;
    }

    *value = atol(string);

    return true;
}

bool sfall_ini_get_string(const char* triplet, char* value, size_t size)
{
    char fileName[kFileNameMaxSize + 1];
    char section[kSectionMaxSize + 1];

    const char* key = parse_ini_triplet(triplet, fileName, section);
    if (key == nullptr) {
        return false;
    }

    Config config;
    if (!configInit(&config)) {
        return false;
    }

    char path[COMPAT_MAX_PATH];
    bool loaded = false;

    if (basePath[0] != '\0' && !is_system_file_name(fileName)) {
        // Attempt to load requested file in base directory.
        snprintf(path, sizeof(path), "%s\\%s", basePath, fileName);
        loaded = configRead(&config, path, false);
    }

    if (!loaded) {
        // There was no base path set, requested file is a system config, or
        // non-system config file was not found the base path - attempt to load
        // from current working directory.
        strcpy(path, fileName);
        loaded = configRead(&config, path, false);
    }

    // NOTE: Sfall's `GetIniSetting` returns error code (-1) only when it cannot
    // parse triplet. Otherwise the default for string settings is empty string.
    value[0] = '\0';

    if (loaded) {
        char* stringValue;
        if (configGetString(&config, section, key, &stringValue)) {
            strncpy(value, stringValue, size - 1);
            value[size - 1] = '\0';
        }
    }

    configFree(&config);

    return true;
}

// Keep script-supplied paths relative to the active game's working directory.
// Accept Windows separators and a leading ./, but reject parent traversal.
static bool relativeIniPath(std::string& path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    if (path.empty() || path[0] == '/' || path.find(':') != std::string::npos
        || path.size() >= COMPAT_MAX_PATH) {
        return false;
    }
    size_t start = 0;
    while (start < path.size()) {
        size_t end = path.find('/', start);
        if (path.substr(start, end - start) == "..") return false;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return true;
}

static bool readIniForArray(Config* config, const char* fileName)
{
    if (fileName == nullptr) return false;
    std::string file(fileName);
    if (!relativeIniPath(file)) return false;

    if (basePath[0] != '\0' && !is_system_file_name(file.c_str())) {
        std::string base(basePath);
        if (relativeIniPath(base)) {
            std::string path = base + "/" + file;
            if (path.size() < COMPAT_MAX_PATH && configRead(config, path.c_str(), false)) return true;
        }
    }
    return configRead(config, file.c_str(), false);
}

static ProgramValue iniStringValue(Program* program, char* string)
{
    ProgramValue value;
    value.opcode = VALUE_TYPE_DYNAMIC_STRING;
    value.integerValue = programPushString(program, string);
    return value;
}

ArrayId sfall_ini_get_section(Program* program, const char* fileName, const char* sectionName)
{
    const ArrayId id = CreateTempArray(-1, 0);
    Config config;
    if (!configInit(&config)) return id;
    if (sectionName != nullptr && readIniForArray(&config, fileName)) {
        int index = dictionaryGetIndexByKey(&config, sectionName);
        if (index >= 0) {
            auto* section = static_cast<ConfigSection*>(config.entries[index].value);
            for (int i = 0; i < section->entriesLength; ++i) {
                auto& entry = section->entries[i];
                auto key = iniStringValue(program, entry.key);
                auto value = iniStringValue(program, *static_cast<char**>(entry.value));
                SetArray(id, key, value, false, program);
            }
        }
    }
    configFree(&config);
    return id;
}

ArrayId sfall_ini_get_sections(Program* program, const char* fileName)
{
    Config config;
    if (!configInit(&config)) return CreateTempArray(0, 0);
    const bool loaded = readIniForArray(&config, fileName);
    const ArrayId id = CreateTempArray(loaded ? config.entriesLength : 0, 0);
    if (loaded) {
        for (int i = 0; i < config.entriesLength; ++i) {
            auto value = iniStringValue(program, config.entries[i].key);
            SetArray(id, ProgramValue(i), value, false, program);
        }
    }
    configFree(&config);
    return id;
}

bool sfall_ini_set_int(const char* triplet, int value)
{
    char stringValue[20];
    compat_itoa(value, stringValue, 10);

    return sfall_ini_set_string(triplet, stringValue);
}

bool sfall_ini_set_string(const char* triplet, const char* value)
{
    char fileName[kFileNameMaxSize + 1];
    char section[kSectionMaxSize + 1];

    const char* key = parse_ini_triplet(triplet, fileName, section);
    if (key == nullptr) {
        return false;
    }

    Config config;
    if (!configInit(&config)) {
        return false;
    }

    char path[COMPAT_MAX_PATH];
    bool loaded = false;

    if (basePath[0] != '\0' && !is_system_file_name(fileName)) {
        // Attempt to load requested file in base directory.
        snprintf(path, sizeof(path), "%s\\%s", basePath, fileName);
        loaded = configRead(&config, path, false);
    }

    if (!loaded) {
        // There was no base path set, requested file is a system config, or
        // non-system config file was not found the base path - attempt to load
        // from current working directory.
        strcpy(path, fileName);
        loaded = configRead(&config, path, false);
    }

    configSetString(&config, section, key, value);

    bool saved = configWrite(&config, path, false);

    configFree(&config);

    return saved;
}

} // namespace fallout
