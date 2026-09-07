# Companion controls — v1.8 (versionCode 26)

**Depends on:** PR #35 (Act 2 / Crawl / Difficulty). **Merge this PR after #35.**

**Legal:** Original companion names/dialogue. SRD 5.1 class concepts only. See `ATTRIBUTION.md`.

## Features

| Feature | Behavior |
|---------|----------|
| **Companion class** | At Story / Crawl start, pick Fighter (Bren), Wizard (Melf), Rogue (Sable), or Cleric (Miren). Out of combat: Ally → Change class. |
| **Companion sheet** | Top-bar **Ally** button, or tap the companion sprite (when not in Potion targeting mode). |
| **Auto vs Player** | Ally dialog toggle. **Auto** = existing ally AI. **Player** = on their turn you use Attack / Special / Potion (same targeting UI). |
| **Persist** | Class + `aiControlled` flag in save/Continue (backward compatible with older saves). |
| **Online** | Host-authoritative; DM table has no solo companion (Ally hidden). Solo/Crawl is primary. |

## Preserve

Audio, graphics pack, quest acts, crawl, difficulty wipe rules, tap-target combat, death locks.

## Device playtest checklist

1. **Build → Clean Project**, then **Rebuild** (native C++ + Kotlin changed).
2. Story/Crawl → hero class → **companion class** → Auto or Player-controlled.
3. Ally button / tap companion → sheet; toggle Auto ↔ Player; change class out of combat.
4. Player-controlled: companion turn banner → Attack/Special/Potion works; Auto still AI-acts.
5. Force-stop mid-run → **Continue** restores companion class + control mode.
6. Easy–Hard wipe / Nightmare wipe still behave as in #35.
7. Host online DM table: no Ally companion UI; Join path unchanged.
8. Audio, graphics, Search once/room, death locks preserved.

## Out of scope

Next graphics layer.
