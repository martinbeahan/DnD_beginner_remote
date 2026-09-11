# Endgame / Act 3 / Boss Raids — v2.6.4 (versionCode 41)

**Base:** master after Text.DnD (#45); incorporates #46 drop-nerf weights (separate drop/rarity rolls; shop Rare/Epic gated deeper). **Legal:** original Act 3 / raid / endgame boss names & flavor; SRD-safe monsters only; no WotC module text. No new art assets (reuse existing CC0 stages/sprites — see `ATTRIBUTION.md`).

## Gates (Martin priority)

**Endgame is ONLY after story fully complete (Acts 1–3).**

| Content | Gate |
|---|---|
| Boss Raid menu | `story_fully_complete` (SharedPreferences) / `questAct3Complete_` — locked shows **Finish the story first** |
| Legendary loot | `allowLegendary` on `generateLoot` — **only** Hollow Crown / Ember Hydra / Nightfang / Boss Raid defeat (not Act 3 flag alone; never pity/Search/shop/early bosses) |
| Hollow Crown / Ember Hydra / Nightfang Matriarch | Same endgame gate + deep crawl room (~18–22+) |
| Early crawl GK / SK / Drake | Unchanged — may appear before story complete |

## 1) Act 3 — Emberdeep Breach

Beats 14–19 (scripted, Continue mid-act via save):

1. Wellside Rumor → 2. Old Well Descent → 3. Root Labyrinth → 4. Ember Seal Niche (Search) → 5. Breach Threshold (**Breach Warden**) → 6. Breach Sealed

Acts 1+2 unchanged; after Act 2 Settled, Onward starts Act 3 (not procedural). After Act 3 Sealed, Onward → post-story endgame rooms.

## 2) Rarities

- Early/normal drops keep **#46 nerfed** Rare/Epic weights (separate rarity roll; luck/3).
- **Epic** mostly deep / boss luck.
- **Legendary** (gold UI): **7%** of successful drops from endgame boss / Boss Raid defeat only (`allowLegendary`). Never from trash, Search, shop, early GK/SK/Ashen Drake, generic clears, or pity (pity hard-capped at Epic).
- Legendary power: +1 bonus stat; upgrade **15** → two; upgrade **20+** → three + sub-effect (lifesteal / on-hit heal / once-per-fight shield). Caps: Common 5 … Legendary 25.

## 3) Boss Raid

- Main menu **Boss Raid**; costs **1 Raid Key**.
- Keys: max **2 held** and **2 granted per calendar day** (`raid_keys`, `raid_keys_date`, `raid_keys_granted_today`).
- Keys drop from endgame bosses (crawl + raid clears) when story complete.
- Mode = focused endgame boss fight; strong XP/gold/Legendary/key chance.
- **v2.6.3:** Raid loads the **same Continue save** (`beginBossRaidFromCurrent`) — does **not** call `resetGame` / class select. Pre-raid snapshot restores the story hero on wipe/menu; Onward after clear returns to prior adventure mode with spoils kept.

## 4) Endgame bosses (original)

- **Hollow Crown**, **Ember Hydra**, **Nightfang Matriarch** — deep crawl post–Act 3 + raids.
- Better XP/gold/luck than early kings; not in early Easy story.

## Persist / preserve

Act 3 flags + endgame boss-seen in serialize; inventory Legendary token fields; transfer/class lock/sell/upgrade; difficulty wipes; companion; audio; Text.DnD UI.

## 5) Post-story navigation (v2.6.1 hotfix)

- **Story-complete CTA:** when Acts 1–3 first complete, dialog offers **Boss Raid**, **Main menu**, or Keep exploring (no app relaunch).
- **Settings / HUD:** **Return to main menu** saves via `syncAndSave` / prefs — keeps Continue, gear, quest flags, Raid Keys (unlike Abandon).
- **Settings Boss Raid:** visible in-adventure; locked reason if story incomplete; otherwise saves then opens the same Boss Raid flow as the main menu.
- Raid key spend caps and `story_fully_complete` gate unchanged.

## Device checklist

1. Story through Act 3 → menu unlocks Boss Raid; Legendary (~7%) only on Hollow/Hydra/Nightfang or Raid clears — not on post-story trash/early bosses.
2. Before Act 3: Raid locked; no Legendary; no Hollow/Hydra/Nightfang.
3. Raid keys: spend/grant caps; UI explains.
4. Inventory shows Legendary color + bonus/sub-effect text; upgrade to 20+ unlocks sub-effect.
5. Continue mid–Act 3; Acts 1–2 regression; crawl early GK/SK still OK.
