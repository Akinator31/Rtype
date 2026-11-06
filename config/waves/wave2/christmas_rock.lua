return {
    name = "christmas_rock",
    label = "Christmas rock",
    description = "The Bydos rock along",
    difficulty = 2,
    placement = "music",
    music = "sounds/christmas_rock.mp3",
    lead = 1.5,
    beats = {
        bass = { enemies = { "bydo_gunner", "bydo_raider" }, minGap = 4.5 },
        mid = { enemies = { "bydo_scout", "bydo_gunner", "bydo_interceptor" }, minGap = 3.5 },
        high = { enemies = { "bydo_scout", "bydo_drone", "bydo_stinger" }, minGap = 3.5 },
    },
    area = { minX = 1980, maxX = 2020, minY = 100, maxY = 980 },
}
