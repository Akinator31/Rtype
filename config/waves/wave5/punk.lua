return {
    name = "punk",
    label = "Punk",
    description = "The Bydos storm in",
    difficulty = 5,
    placement = "music",
    music = "sounds/punk.wav",
    lead = 1.5,
    beats = {
        bass = { enemies = { "bydo_heavy", "bydo_berserker", "bydo_bomber", "bydo_siege" }, minGap = 4.0 },
        mid = { enemies = { "bydo_raider", "bydo_gunner", "bydo_striker", "bydo_mortar" }, minGap = 3.0 },
        high = { enemies = { "bydo_interceptor", "bydo_stinger", "bydo_sniper", "bydo_drone" }, minGap = 3.0 },
    },
    area = { minX = 1980, maxX = 2020, minY = 100, maxY = 980 },
}
