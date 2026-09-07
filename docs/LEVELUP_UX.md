# Level-up UX overhaul — v2.0 (versionCode 28)

**Base:** Graphics layer 2 (#37 / v1.9). **Legal:** UI-only; SRD attribute rules unchanged. See `ATTRIBUTION.md`.

## Problem

Spending level-up points required one dialog pick per point (sheet → Level Up → pick stat → repeat). No way to assign multiple pending points in one pass.

## Solution

- Character sheet shows remaining points and **Assign attribute points**.
- Level-up panel: **+/− per attribute**, live **points remaining**, preview **Now → after**.
- **Confirm once** — spend **any or all** assigned points (Confirm disabled until at least one assigned).
- Partial spend allowed; panel reopens if points remain (solo/host).
- Works for **hero** and **companion** sheets (same panel, character name passed through).
- Online: client sends one host-authoritative **`levelupBatch`** action (`allocations` = `s0,s1,s2,s3,s4,s5`). Legacy single `levelup` / `increaseStat` still accepted.

## Preserve

Graphics layer 2, companion controls, difficulty/wipe, story/crawl, audio, tap-target sizes (48dp +/−).

## Device playtest checklist

1. Clean/Rebuild (Kotlin + layouts; native unchanged).
2. Level a hero (or cheat pending points) → sheet shows points → **Assign attribute points**.
3. Assign 2 points across stats with +/−; remaining counter updates; Confirm enables.
4. Confirm → stats/HP (if CON) update; sheet no longer offers points if all spent.
5. Partial: assign 1 of 2 → Confirm → panel returns for the leftover point.
6. Companion sheet with pending points → same multi-assign flow.
7. Online Join client: Confirm → “Sent N point(s) to the DM…”; host state reflects batch.
8. Cancel leaves points unspent.
9. Regression: graphics stages/VFX, companion Auto/Player, difficulty, audio, tap targets.

## Out of scope

Native rule changes (still +2 pending per level, one `increaseAttribute` per point under the hood).
