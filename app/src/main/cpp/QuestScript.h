#ifndef DND_QUEST_SCRIPT_H
#define DND_QUEST_SCRIPT_H

#include <string>

namespace dnd {

/**
 * Solo quest "Ashen Lantern" — original story beats.
 * Compatible with 5e SRD monster/rule concepts only (goblin, wolf, skeleton, ogre-lite).
 * Not an official D&D product; do not copy WotC adventure text or set pieces.
 */
enum class SoloQuestBeat : int {
    NONE = 0,          // Online Host-as-DM / client — procedural rooms
    MILLHOLLOW = 1,    // Village road intro
    THORNPATH = 2,     // Woods ambush
    CRYPT_DOORS = 3,   // Puzzle-lite / short fight
    BONE_GALLERY = 4,  // Skeletons
    LANTERN_VAULT = 5, // Boss-ish vault
    RESOLUTION = 6,    // Shrine ending + Rest
    POST_QUEST = 7     // Procedural after main quest; banner remains
};

inline const char* soloQuestBeatName(int beat) {
    switch (static_cast<SoloQuestBeat>(beat)) {
        case SoloQuestBeat::MILLHOLLOW: return "Millhollow Road";
        case SoloQuestBeat::THORNPATH: return "Thornpath Woods";
        case SoloQuestBeat::CRYPT_DOORS: return "Crypt Doors";
        case SoloQuestBeat::BONE_GALLERY: return "Bone Gallery";
        case SoloQuestBeat::LANTERN_VAULT: return "Lantern Vault";
        case SoloQuestBeat::RESOLUTION: return "Ashen Shrine";
        case SoloQuestBeat::POST_QUEST: return "After the Lantern";
        default: return "Wander";
    }
}

struct QuestBeatScript {
    int beatId;
    const char* title;
    const char* roomDescription;
    const char* dmEnter;
    const char* journalOnEnter;
};

// Data-driven solo script (beat IDs 1–6). Original wording only.
inline const QuestBeatScript kAshenLanternBeats[] = {
    {
        1,
        "Millhollow Road",
        "Night drapes Millhollow like wet wool. Cottage windows stay shuttered; "
        "the Ashen Lantern that once burned on the green is gone — stolen into Hollowbarrow Crypt. "
        "A muddy track leads toward the dark tree-line.",
        "Millhollow has gone dark. The Ashen Lantern kept night-things at bay — "
        "until thieves dragged it into Hollowbarrow Crypt. Recover it, or light it at the crypt shrine, "
        "and the village may sleep again.",
        "Millhollow is dark. The Ashen Lantern was stolen into Hollowbarrow Crypt."
    },
    {
        2,
        "Thornpath Woods",
        "Thornpath claws at your cloak. Roots twist underfoot; somewhere a low growl answers the wind. "
        "Broken lantern-glass glints in the mud — a trail toward the barrow.",
        "The woods remember light. Something else remembers hunger. Stay sharp.",
        "The party entered Thornpath Woods on the trail of the Ashen Lantern."
    },
    {
        3,
        "Crypt Doors",
        "Stone doors seal Hollowbarrow. Ash-stained runes spiral around a cold keyhole. "
        "A draft smells of old bone and extinguished oil.",
        "The crypt doors wait. Search for a rune-key if you can — or force a way past whatever watches.",
        "The party reached the sealed doors of Hollowbarrow Crypt."
    },
    {
        4,
        "Bone Gallery",
        "A long gallery of niches. Pale bones sit too neatly. Dust motes hang where lantern-light once walked.",
        "Steel yourselves. The dead here do not sleep kindly.",
        "The party entered the Bone Gallery beneath Hollowbarrow."
    },
    {
        5,
        "Lantern Vault",
        "The vault chamber opens on a stone plinth. The Ashen Lantern rests there — cold iron, grey glass, "
        "still faintly warm. Something large shifts in the dark beyond the plinth.",
        "There — the Ashen Lantern. Claim it, but the vault does not give gifts freely.",
        "The party found the Lantern Vault and the stolen Ashen Lantern."
    },
    {
        6,
        "Ashen Shrine",
        "A shallow shrine niche faces the vault path. Soot rings the altar where the Ashen Lantern once burned. "
        "You may Rest here, then return the light — or carry it onward into deeper dark.",
        "The shrine accepts the lantern's glow. Millhollow's night thins. "
        "Main quest complete — Rest if you need, then return to the menu, or press Onward into uncharted rooms.",
        "The Ashen Lantern was restored at the crypt shrine. Millhollow can sleep again."
    }
};

inline const QuestBeatScript* findQuestBeat(int beatId) {
    for (const auto& b : kAshenLanternBeats) {
        if (b.beatId == beatId) return &b;
    }
    return nullptr;
}

} // namespace dnd

#endif // DND_QUEST_SCRIPT_H
