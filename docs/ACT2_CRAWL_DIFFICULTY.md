# Act 2 + Dungeon Crawl + Difficulty — v1.7 (versionCode 25)

**Legal:** Original plot/locations/dialogue. SRD 5.1 monster/rule concepts only. Compatible with 5e SRD — not an official D&D product. See `ATTRIBUTION.md`.

## Menu

| Button | Behavior |
|--------|----------|
| **Story adventure** | Solo scripted Act 1 → Act 2 → procedural |
| **Dungeon Crawl** | Skip quest script; procedural rooms from Room 1 |
| **Change difficulty** | Easy (default) / Medium / Hard / Nightmare — preference persisted |
| Continue | Restores mid-run (including Easy–Hard Game Over → wipe dialog) |

Difficulty is **locked for the run** when Story/Crawl starts (shown in Settings).

## Act 1 — Ashen Lantern (unchanged beats 1–6)

Same as before. Onward from Ashen Shrine now starts **Act 2** (not immediate procedural).

## Act 2 — Millhollow's Debt (beats 8–13)

| # | Beat | Notes |
|---|------|-------|
| 8 | Millhollow Green | Debt collectors; light fight |
| 9 | Millrace Weir | River goblins |
| 10 | Flooded Cellar | Skeleton + giant rat |
| 11 | Ledger Loft | Soft open — Search → coerced ledger |
| 12 | Collector's Hall | Bandit-flavored Collector (Fighter) |
| 13 | Debt Settled | Resolution; Onward → procedural (`POST_QUEST` = 7) |

**Saves:** Mid-Act 1 Continue still works. Saves already in legacy post–Act 1 procedural (`questBeat=7`) stay procedural — **start a fresh Story** to play Act 2. New header fields: `questAct2Complete`, `questAct2LedgerFound`, `soloPlayMode`, `difficulty` (backward compatible).

## Party wipe rules

| Difficulty | On entire party fallen |
|------------|------------------------|
| **Easy** | Keep stats/gear/gold; Continue → one room/beat back; **full HP** |
| **Medium** | Keep stats/gear/gold; Continue → one back; **half HP** |
| **Hard** | Keep stats; **strip gold + gear** to starters; Continue → one back |
| **Nightmare** | Complete restart — wipe save / main menu |

## Device playtest checklist

1. Clean/Rebuild (native + Kotlin changed).
2. Menu shows Story, Crawl, Difficulty (default Easy).
3. Story → Millhollow → … → Shrine → Onward → **Millhollow's Debt** green.
4. Act 2 loft Search → ledger → Collector → Debt Settled → procedural.
5. Crawl → Room 1 procedural, no quest banner.
6. Easy wipe → Continue keeps gear, full HP, prior chamber.
7. Hard wipe → Continue strips gold/gear.
8. Nightmare wipe → menu, no Continue revive.
9. Settings shows locked difficulty during a run.
10. Host online still procedural (no solo script).
11. Audio, graphics pack, tap targets, Search once/room, death locks preserved.

## Out of scope

Next graphics layer.
