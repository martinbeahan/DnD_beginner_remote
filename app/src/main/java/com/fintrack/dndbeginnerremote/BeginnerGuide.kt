package com.fintrack.dndbeginnerremote

/**
 * Beginner-friendly copy for tutorial + solo DM coaching.
 * Keeps explanations short and practical (5e SRD-flavored).
 * Stat blurbs describe how *this* game uses attributes in C++ combat/search.
 */
object BeginnerGuide {

    data class Page(val title: String, val body: String)

    val tutorialPages = listOf(
        Page(
            "Welcome, adventurer",
            "This is a simple D&D-style dungeon crawl.\n\n" +
                "You play a hero. The Dungeon Master (DM) describes the world, " +
                "runs monsters, and helps you learn the buttons.\n\n" +
                "You do not need to know the rulebooks to start."
        ),
        Page(
            "Your turn — Attack",
            "Attack = swing your weapon.\n\n" +
                "The game rolls a d20, adds your attack bonus, and compares it to the enemy's Armor Class (AC).\n\n" +
                "• Equal or higher than AC → Hit, then roll damage\n" +
                "• Natural 20 → Critical hit (extra damage)\n" +
                "• Natural 1 → Automatic miss"
        ),
        Page(
            "Special (your class power)",
            "Special uses a limited resource (like a spell slot or feature use):\n\n" +
                "• Fighter — Action Surge: attack twice\n" +
                "• Rogue — Sneak Attack: extra damage dice\n" +
                "• Wizard — Magic Missile: auto-hit force darts\n" +
                "• Cleric — Healing Word: heal the most hurt ally\n\n" +
                "If Special is greyed out or fails, you may be out of uses — try a Short Rest."
        ),
        Page(
            "Potion, Rest, Search",
            "• Potion — drink a Potion of Healing (2d4+2 HP). Costs one supply/resource.\n\n" +
                "• Short Rest — catch your breath: recover some HP and some special uses. " +
                "Not a full night's sleep.\n\n" +
                "• Search (Srch) — look for treasure or danger (skill check). Can find gold… or a trap."
        ),
        Page(
            "Stats & leveling",
            "Clear rooms to earn XP (see the gold XP bar under your turn banner).\n\n" +
                "When you level up you get 2 attribute points — open Sheet to spend them.\n\n" +
                "Your class cares most about one attack/heal stat plus Constitution for hit points. " +
                "Tap Help → What do stats do? anytime."
        ),
        Page(
            "Playing solo",
            "Story adventure runs original quests (Ashen Lantern, then Millhollow's Debt):\n\n" +
                "• Act 1: recover the Ashen Lantern from Hollowbarrow Crypt\n" +
                "• Act 2: settle Millhollow's coerced grain debt at the mill\n" +
                "• Dungeon Crawl skips story for procedural rooms from the start\n\n" +
                "Pick Difficulty on the main menu (default Easy). On a full party wipe:\n" +
                "Easy/Medium keep gear; Hard strips gold/gear; Nightmare ends the run.\n\n" +
                "Continue mid-quest works — your beat is saved.\n" +
                "Compatible with 5e SRD concepts only (not an official D&D product)."
        )
    )

    fun actionReference(): String = """
        |Quick reference
        |
        |Attack — Weapon strike vs Armor Class (d20 + bonuses).
        |Special — Your class feature (uses a resource).
        |Potion — Heal 2d4+2 HP (uses a resource).
        |Short Rest — Recover some HP and resources.
        |Search — Investigate the room (risk/reward).
        |Sheet — Your stats, HP, AC, XP, gear.
        |Log — Adventure journal.
        |
        |Tip: AC is how hard you are to hit. Higher is better.
        |Tip: Resources power Specials and Potions — rest to regain some.
        |Tip: XP fills as you clear rooms; level-ups grant attribute points.
    """.trimMargin()

    /** Plain-English blurbs matching this build's C++ rules (SRD-safe wording). */
    fun statBlurb(statIndex: Int): String = when (statIndex) {
        0 -> "Strength — attack rolls & weapon damage for Fighters (and most melee)."
        1 -> "Dexterity — Rogue attacks/damage; everyone's Armor Class and initiative."
        2 -> "Constitution — hit points each level; helps Short Rest recovery. Spending CON also adds HP now."
        3 -> "Intelligence — Wizard weapon attacks/damage; Search checks for loot (or traps)."
        4 -> "Wisdom — Cleric weapon attacks/damage; boosts Healing Word amount."
        5 -> "Charisma — classic sixth score; this adventure barely uses it in combat."
        else -> "Attribute score — higher is usually better."
    }

    fun statShortHint(statIndex: Int): String = when (statIndex) {
        0 -> "Melee hits & damage (Fighter)"
        1 -> "AC, initiative, Rogue hits"
        2 -> "More HP; better rests"
        3 -> "Wizard hits; Search checks"
        4 -> "Cleric hits; Healing Word"
        5 -> "Rarely used here"
        else -> ""
    }

    fun classStatTip(classId: Int): String = when (classId) {
        0 -> "Fighter tip: prioritize Strength, then Constitution. Dexterity still helps AC."
        1 -> "Wizard tip: prioritize Intelligence, then Constitution. Dexterity helps AC (light armor)."
        2 -> "Rogue tip: prioritize Dexterity (hits, damage, AC), then Constitution."
        3 -> "Cleric tip: prioritize Wisdom (attacks + Healing Word), then Constitution."
        else -> "Boost your class's main attack stat and Constitution for survivability."
    }

    fun statsHelpMessage(): String = buildString {
        appendLine("How stats work in this game")
        appendLine()
        for (i in 0..5) appendLine("• ${statBlurb(i)}")
        appendLine()
        appendLine("By class")
        appendLine("• ${classStatTip(0)}")
        appendLine("• ${classStatTip(1)}")
        appendLine("• ${classStatTip(2)}")
        appendLine("• ${classStatTip(3)}")
        appendLine()
        append("Level-up: clear rooms → earn XP → open Sheet when you (or an ally) level up and spend points.")
    }

    fun specialBlurb(classId: Int): String = when (classId) {
        0 -> "Action Surge: take an extra burst and attack twice this turn."
        1 -> "Magic Missile: three darts that always hit — great when you keep missing."
        2 -> "Sneak Attack: add extra damage dice when you catch a foe off-guard."
        3 -> "Healing Word: heal the ally with the lowest HP (includes you)."
        else -> "Your Special is a limited class power. Use it when it matters."
    }

    /**
     * Solo DM coach line for the current combat status string from native code.
     * Returns null if no new tip is needed.
     */
    fun turnCoachTip(
        status: String,
        localPlayerName: String,
        isShop: Boolean,
        specialName: String,
        classId: Int
    ): String? {
        if (isShop) {
            return "DM: Safe room. Tap Shop to buy gear, or Leave/Short Rest area controls to move on."
        }
        if (status.contains("Game Over", ignoreCase = true)) {
            return "DM: The party has fallen. Tap Menu, then Solo adventure to begin again (this save is cleared)."
        }
        val turnMarker = "Turn: "
        val turnLine = status.lineSequence().firstOrNull { it.contains(turnMarker) } ?: return null
        val whose = turnLine.substringAfter(turnMarker).trim()
        if (whose.equals("Safe", true) || whose.equals("None", true)) return null

        val companionLine = status.lineSequence().firstOrNull { it.startsWith("Companion:") }
        val companionPlayer = companionLine != null &&
            companionLine.contains("[Player]", ignoreCase = true) &&
            companionLine.contains(whose)
        val myTurn = whose.equals(localPlayerName, true) || whose.equals("You", true) || companionPlayer
        if (!myTurn) {
            return "DM: It's $whose's turn. Watch the log — enemies and Auto allies act automatically."
        }
        if (companionPlayer && !whose.equals(localPlayerName, true)) {
            return "DM: $whose's turn (you control them). Use Attack / Special / Potion just like your hero."
        }

        val hurt = status.lineSequence().any { line ->
            line.startsWith(localPlayerName) && line.contains("HP:") && run {
                val hp = line.substringAfter("HP:").substringBefore(" ").trim()
                val parts = hp.split("/")
                if (parts.size == 2) {
                    val cur = parts[0].toIntOrNull() ?: return@run false
                    val max = parts[1].toIntOrNull() ?: return@run false
                    max > 0 && cur * 2 <= max
                } else false
            }
        }

        return buildString {
            append("DM: Your turn, $localPlayerName. ")
            append("Attack = weapon swing vs their AC. ")
            append("Special ($specialName) = ${specialBlurb(classId)} ")
            if (hurt) append("You're bloodied — consider Potion or Healing Word if you have it. ")
            append("Short Rest when the room is clear and you're drained.")
        }
    }
}
