# Ashen Lantern (Solo quest) — playtest notes

**Version:** 1.5 (versionCode 21)  
**Legal:** Original plot/locations/NPCs/dialogue. SRD 5.1 monster/rule concepts only. Compatible with 5e SRD — not an official D&D product. See `ATTRIBUTION.md`.

## Beats (Solo New Game)

| # | Beat | Content |
|---|------|---------|
| 1 | Millhollow Road | Intro narration; light Goblin Scout; Search → crypt clue |
| 2 | Thornpath Woods | Wolf (+ optional Goblin); DM tone lines |
| 3 | Crypt Doors | Soft-open puzzle-lite; Search once → Ashen Rune-Key |
| 4 | Bone Gallery | Two Skeletons; gold/XP via existing systems |
| 5 | Lantern Vault | Skeleton Champion (ogre-lite FIGHTER); victory → `questLanternRecovered` + Ashen Lantern item |
| 6 | Ashen Shrine | Resolution narration + journal; Rest; Quest complete banner |
| 7+ | Post-quest | Existing procedural rooms; “Main quest done” banner |

## Persistence

Save header stores `questBeat`, `questLanternRecovered`, `questComplete`, `questCryptKeyFound`. **Continue** mid-quest restores the current beat.

## Online

Host-as-DM / Join paths call `resetSoloQuestState()` — procedural rooms unchanged.

## Device playtest checklist

1. **Build → Clean Project**, then **Rebuild** (native C++ changed: `QuestScript.h`, `Game.cpp` / `Game.h`).
2. Solo **New Game** → confirm Millhollow hook and DM lines.
3. Clear scout → **Search** clue → **Onward** through woods → crypt.
4. At Crypt Doors, **Search** for rune-key (one attempt lock still applies).
5. Bone Gallery fight → Vault → defeat champion → lantern recovered message.
6. Shrine: Rest, journal shows quest complete; banner “Main quest done”.
7. Force-stop app mid-quest (e.g. after Thornpath) → **Continue** resumes same beat.
8. Host online table → confirm no Ashen Lantern script (procedural Room 1).

## Out of scope

2D/3D art overhaul, copying WotC modules, multiplayer campaign sync of quest beats.
