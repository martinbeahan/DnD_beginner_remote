# Inventory Sell — v2.3 (versionCode 32)

**Base:** master `fc320ce` (#41 loot / inventory / bosses, v2.2). **Legal:** UI + economy only; no new WotC content. See `ATTRIBUTION.md`.

## Features

1. **Sell from Inventory** — Each owned item (equipped or bag) shows **Sell (Ng)**. Shop stays buy-only.
2. **Confirm dialog** — “Sell X for Ng?” before gold is added and the item is removed.
3. **Pricing** — Sell price is a fair fraction of `shopCost()`, scaled by rarity and upgrade level (~40–60%):
   - Common 40%, Uncommon 45%, Rare 50%, Epic 55%
   - +2% per upgrade tier, capped at 60%
4. **Equipped sell** — Unequips first (clears slot), then pays gold. Selling the last weapon leaves fists (null weapon; attack bonus 0). Selling armor recalculates AC (10 + DEX).
5. **Persist** — Gold and inventory changes go through existing `syncAndSave` / serialize (no new save fields).

## Preserve

Loot rarity, class gear, upgrades, bosses (#41), shop buy, stats/level-up, companion, difficulty, story/crawl, audio, graphics, online DM.

## Device checklist

1. Rebuild (Kotlin + native).
2. Sheet → Inventory: Sell on starters / loot; confirm → gold up, item gone.
3. Sell equipped weapon mid-run: still attack (fists); AC updates if armor sold.
4. Continue / reload: gold and bag match.
5. Merchant still buy-only; Inventory Sell available from shop’s Inventory button.
6. Regression: upgrades, equip class tags, bosses, XP bar, companion, Host DM.
