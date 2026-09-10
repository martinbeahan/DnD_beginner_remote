# Loot / Inventory / Bosses — v2.2 (versionCode 31)

**Base:** master `5279fe4` (v2.1). **Legal:** original boss names/flavor; SRD-safe monsters only; no WotC module text. Boss sprites reuse existing CC0 frames (see `ATTRIBUTION.md`).

## Features

1. **Item rarity + class gear** — Common / Uncommon / Rare / Epic (color-coded in Shop + Inventory). Items may be class-tagged (Fighter/Wizard/Rogue/Cleric) or Any; equipping enforces the tag. Shop stocks a rarity mix (cheap commons → expensive rares/epics by depth).
2. **Inventory UI** — Sheet → **Open Inventory** (also from Merchant). Lists equipped + bag: rarity, class, Equip/Unequip, **Upgrade (Ng)**. Purchases and room drops go to the bag (potions still drink on buy).
3. **Gold upgrades** — Spend gold to +1 bonus / upgrade tier on owned or equipped gear; cost scales with rarity and level. Fails clearly if broke.
4. **Bosses** — Chance encounters (not early Easy soft-locks):
   - **Goblin King** — crawl from room ~6–8+; Medium+ rare Act 2 Weir ambush
   - **Skeleton King** — crawl mid-deep; Medium+ rare Bone Gallery
   - **Ashen Drake** (Dragon stand-in) — late crawl only (~16–18+)
   Defeat: more XP/gold, higher Rare/Epic drop luck (bag loot).

## Persist

Inventory + rarity/class/upgrade tokens in `serialize` / Continue (hero + companion). Legacy saves without bag still load.

## Preserve

Stats persist, multi-point level-up, XP bar, companion Auto/Player, difficulty wipes, story/crawl, audio, graphics, online DM.

## Device checklist

1. Rebuild (Kotlin + native).
2. Sheet → Inventory: see starters; Upgrade costs gold; Unequip → Equip class check.
3. Merchant: rarity colors; buy → bag; Inventory from shop.
4. Clear rooms / bosses: loot in bag; Continue keeps inventory.
5. Crawl deep for Goblin/Skeleton King / Ashen Drake; Easy stays gentler early.
6. Regression: XP bar, level-up, companion, audio, graphics, Host DM.

## See also

Inventory Sell (v2.3): `docs/INVENTORY_SELL.md`.
