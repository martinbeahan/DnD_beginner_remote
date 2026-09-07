# Beginner stats / XP / level-up notify — v2.1 (versionCode 30)

**Base:** master `1494e5f` (#38 level-up panel + #39 stats persist). **Legal:** original/SRD-safe blurbs only; no PHB dumps. See `ATTRIBUTION.md`.

## Features

1. **What stats do** — Sheet + level-up panel show short plain-English tips matching this build’s C++ rules (attack/AC/HP/Search/Healing Word). Help → “What do stats do?” plus a tutorial page. Per-class tips for Fighter / Wizard / Rogue / Cleric.
2. **XP bar** — HUD under the turn banner (`XP L#` + gold bar + `current/need`). Sheet also shows XP toward next level. JNI: `getXpProgress(name)`, `getPartyXpProgress()`; sheet text includes `XP: cur/need`.
3. **Level-up notification** — When pending points *increase* for hero or ally: toast, combat-feed line, dialog (“open Sheet to spend points”), and pulsing **Sheet!** button while points remain. Solo + companion; online party via `getPartyXpProgress`. Continue seeds snapshot so existing pending highlights without re-dialog.

## Preserve

Multi-point level-up assign panel, stats-persist fix, audio/graphics/story/difficulty/companion.

## Device checklist

1. Rebuild (Kotlin + native).
2. Play → clear a room → HUD XP rises; at threshold toast/dialog + Sheet! pulse.
3. Sheet shows XP bar, class tip, attribute hints; Assign panel still multi-point with per-stat blurbs.
4. Companion level-up also notifies; Ally sheet can spend.
5. Help → What do stats do? / tutorial page “Stats & leveling”.
6. Regression: persist stats across level-up, audio, graphics, companion Auto/Player, difficulty.
