#ifndef DND_QUEST_SCRIPT_H
#define DND_QUEST_SCRIPT_H

#include <string>

namespace dnd {

/**
 * Solo story acts — original wording only.
 * Act 1 "Ashen Lantern" (beats 1–6). Act 2 "Millhollow's Debt" (beats 8–13).
 * POST_QUEST (7) = procedural rooms (legacy post–Act 1 saves + after Act 2).
 * Compatible with 5e SRD monster/rule concepts only. Not an official D&D product.
 */
enum class SoloQuestBeat : int {
    NONE = 0,          // Online Host-as-DM / client / crawl — procedural
    // Act 1 — Ashen Lantern
    MILLHOLLOW = 1,
    THORNPATH = 2,
    CRYPT_DOORS = 3,
    BONE_GALLERY = 4,
    LANTERN_VAULT = 5,
    RESOLUTION = 6,    // Ashen Shrine
    POST_QUEST = 7,    // Procedural (legacy + after Act 2)
    // Act 2 — Millhollow's Debt
    ACT2_GREEN = 8,    // Debt collectors on the green
    ACT2_WEIR = 9,     // Millrace / weir path
    ACT2_CELLAR = 10,  // Flooded mill cellar
    ACT2_LOFT = 11,    // Ledger loft (search)
    ACT2_HALL = 12,    // Confront the Collector
    ACT2_SETTLED = 13  // Debt settled / resolution
};

enum class Difficulty : int {
    EASY = 0,
    MEDIUM = 1,
    HARD = 2,
    NIGHTMARE = 3
};

enum class SoloPlayMode : int {
    STORY = 0,
    CRAWL = 1
};

inline const char* difficultyName(int d) {
    switch (static_cast<Difficulty>(d)) {
        case Difficulty::EASY: return "Easy";
        case Difficulty::MEDIUM: return "Medium";
        case Difficulty::HARD: return "Hard";
        case Difficulty::NIGHTMARE: return "Nightmare";
        default: return "Easy";
    }
}

inline const char* soloQuestBeatName(int beat) {
    switch (static_cast<SoloQuestBeat>(beat)) {
        case SoloQuestBeat::MILLHOLLOW: return "Millhollow Road";
        case SoloQuestBeat::THORNPATH: return "Thornpath Woods";
        case SoloQuestBeat::CRYPT_DOORS: return "Crypt Doors";
        case SoloQuestBeat::BONE_GALLERY: return "Bone Gallery";
        case SoloQuestBeat::LANTERN_VAULT: return "Lantern Vault";
        case SoloQuestBeat::RESOLUTION: return "Ashen Shrine";
        case SoloQuestBeat::POST_QUEST: return "After the Lantern";
        case SoloQuestBeat::ACT2_GREEN: return "Millhollow Green";
        case SoloQuestBeat::ACT2_WEIR: return "Millrace Weir";
        case SoloQuestBeat::ACT2_CELLAR: return "Flooded Cellar";
        case SoloQuestBeat::ACT2_LOFT: return "Ledger Loft";
        case SoloQuestBeat::ACT2_HALL: return "Collector's Hall";
        case SoloQuestBeat::ACT2_SETTLED: return "Debt Settled";
        default: return "Wander";
    }
}

inline const char* soloQuestActTitle(int beat) {
    const auto b = static_cast<SoloQuestBeat>(beat);
    if (b >= SoloQuestBeat::ACT2_GREEN && b <= SoloQuestBeat::ACT2_SETTLED)
        return "Millhollow's Debt";
    if (b >= SoloQuestBeat::MILLHOLLOW && b <= SoloQuestBeat::RESOLUTION)
        return "Ashen Lantern";
    return "";
}

inline bool isAct1Beat(int beat) {
    return beat >= static_cast<int>(SoloQuestBeat::MILLHOLLOW)
        && beat <= static_cast<int>(SoloQuestBeat::RESOLUTION);
}

inline bool isAct2Beat(int beat) {
    return beat >= static_cast<int>(SoloQuestBeat::ACT2_GREEN)
        && beat <= static_cast<int>(SoloQuestBeat::ACT2_SETTLED);
}

inline bool isScriptedSoloBeat(int beat) {
    return isAct1Beat(beat) || isAct2Beat(beat);
}

/** Previous scripted beat for wipe rollback (Act 2 first rolls back to Act 1 shrine). */
inline int previousScriptedBeat(int beat) {
    if (beat == static_cast<int>(SoloQuestBeat::ACT2_GREEN))
        return static_cast<int>(SoloQuestBeat::RESOLUTION);
    if (isAct2Beat(beat) && beat > static_cast<int>(SoloQuestBeat::ACT2_GREEN))
        return beat - 1;
    if (isAct1Beat(beat) && beat > static_cast<int>(SoloQuestBeat::MILLHOLLOW))
        return beat - 1;
    return beat;
}

struct QuestBeatScript {
    int beatId;
    const char* title;
    const char* roomDescription;
    const char* dmEnter;
    const char* journalOnEnter;
};

// Act 1 — Ashen Lantern (beat IDs 1–6). Original wording only.
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
        "There — the Ashen Lantern. A Skeleton Champion guards it — watch for one brutal surge, then claim the light.",
        "The party found the Lantern Vault and the stolen Ashen Lantern."
    },
    {
        6,
        "Ashen Shrine",
        "A shallow shrine niche faces the vault path. Soot rings the altar where the Ashen Lantern once burned. "
        "You may Rest here, then return the light — or carry it onward into deeper dark.",
        "The shrine accepts the lantern's glow. Millhollow's night thins. "
        "Act 1 complete — Rest if you need, then Onward: Millhollow's Debt waits on the green.",
        "The Ashen Lantern was restored at the crypt shrine. Millhollow can sleep again."
    }
};

// Act 2 — Millhollow's Debt (beat IDs 8–13). Original plot; SRD foes only (goblin, bandit-flavored fighter, etc.).
inline const QuestBeatScript kMillhollowDebtBeats[] = {
    {
        8,
        "Millhollow Green",
        "Dawn finds Millhollow's green crowded by hard-eyed collectors. "
        "They claim a grain debt sealed under the old mill seal — ledgers no elder remembers signing. "
        "A scarred sergeant points toward the river mill and weir.",
        "Act 2 — Millhollow's Debt. Collectors demand tithe the village never owed. "
        "Follow the millrace; find the true ledger; settle the debt without bleeding Millhollow dry.",
        "Debt collectors arrived on Millhollow Green after the lantern's return."
    },
    {
        9,
        "Millrace Weir",
        "The millrace foams over a timber weir. Wet boards groan; a side path ducks under dripping moss toward the mill cellar. "
        "Bootprints and grain sacks mark a smugglers' route.",
        "The weir path is slick. Something scavenges among the sacks — quick and mean.",
        "The party followed the millrace toward the flooded cellar."
    },
    {
        10,
        "Flooded Cellar",
        "Knee-deep water fills the mill cellar. Crates float; a rusted grate leads up toward dry lofts. "
        "Bones and gnawed rope litter the steps.",
        "The cellar is not empty. Clear a path to the loft where the ledgers are kept.",
        "The party entered the flooded mill cellar."
    },
    {
        11,
        "Ledger Loft",
        "A dry loft above the mill. Shelves of wax-sealed ledgers; one book lies open with fresh ink — "
        "names of Millhollow families and sums that grow each night. A quill still wet.",
        "Search the loft for the coerced debt ledger. Soft open — no fight unless you dawdle into trouble later.",
        "The party reached the Ledger Loft above the mill."
    },
    {
        12,
        "Collector's Hall",
        "The mill's upper hall. Grain dust hangs in lantern light. The Collector waits with hired blades — "
        "a bandit captain wearing a false mill seal on a chain.",
        "The Collector will not surrender the forged seal lightly. End the shakedown here.",
        "The party confronted the Collector in the mill hall."
    },
    {
        13,
        "Debt Settled",
        "Millhollow's elders gather on the green. The coerced ledger burns in a clay bowl; ash drifts like snow. "
        "The false seal is broken. You may Rest, return to the menu, or press Onward into uncharted rooms.",
        "Millhollow's Debt is settled. The collectors scatter. Rest if you need — or Onward for procedural rooms.",
        "Millhollow's Debt complete — coerced ledger destroyed; false seal broken."
    }
};

inline const QuestBeatScript* findQuestBeat(int beatId) {
    for (const auto& b : kAshenLanternBeats) {
        if (b.beatId == beatId) return &b;
    }
    for (const auto& b : kMillhollowDebtBeats) {
        if (b.beatId == beatId) return &b;
    }
    return nullptr;
}

} // namespace dnd

#endif // DND_QUEST_SCRIPT_H
