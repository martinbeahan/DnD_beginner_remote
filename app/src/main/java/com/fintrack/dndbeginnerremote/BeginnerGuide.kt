package com.fintrack.dndbeginnerremote

/**
 * Beginner-friendly copy for tutorial + solo DM coaching.
 * Keeps explanations short and practical (5e SRD-flavored).
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
        |Sheet — Your stats, HP, AC, gear.
        |Log — Adventure journal.
        |
        |Tip: AC is how hard you are to hit. Higher is better.
        |Tip: Resources power Specials and Potions — rest to regain some.
    """.trimMargin()

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

        val myTurn = whose.equals(localPlayerName, true) || whose.equals("You", true)
        if (!myTurn) {
            return "DM: It's $whose's turn. Watch the log — enemies and allies act automatically."
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
