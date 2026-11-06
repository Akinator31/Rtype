return {
    name = "the_sports_music",
    label = "Reversal of fortune",
    description = "Humanity regains the upper hand, and the Bydos strike back",
    difficulty = 4,
    placement = "music",
    music = "sounds/the_sports_music.mp3",
    lead = 1.5,
    beats = {
        bass = { enemies = { "bydo_heavy", "bydo_raider", "bydo_bomber", "bydo_phalanx" }, minGap = 4.5 },
        mid = { enemies = { "bydo_scout", "bydo_gunner", "bydo_striker", "bydo_orbiter", "bydo_mortar" }, minGap = 3.5 },
        high = { enemies = { "bydo_scout", "bydo_drone", "bydo_stinger", "bydo_sniper" }, minGap = 3.5 },
    },
    area = { minX = 1980, maxX = 2020, minY = 100, maxY = 980 },
}
