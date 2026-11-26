#ifndef FALLOUT_WORLDMAP_AREA_NAME_H
#define FALLOUT_WORLDMAP_AREA_NAME_H

#include <cstring>
#include <climits>

#include "message.h"

namespace fallout {

// Prefer the localized map.msg entry. If it is absent, use the active
// game's city.txt area_name, not a hardcoded vanilla area list.
// Callers must only use this for areas whose names may be disclosed.
inline void copyWorldmapAreaName(MessageList* messages, int areaId,
    const char* configName, char* dest, size_t capacity)
{
    if (dest == nullptr || capacity == 0) {
        return;
    }

    const char* text = configName != nullptr && configName[0] != '\0'
        ? configName : "Unknown";
    MessageListItem item{};
    if (areaId >= 0 && areaId <= INT_MAX - 1500) {
        item.num = areaId + 1500;
        if (messageListGetItem(messages, &item) && item.text != nullptr && item.text[0] != '\0') {
            text = item.text;
        }
    }
    std::strncpy(dest, text, capacity - 1);
    dest[capacity - 1] = '\0';
}

} // namespace fallout
#endif
