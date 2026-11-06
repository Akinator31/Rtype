return {
    name = "the_sports_music",
    label = "The sports music",
    description = "The Bydos follow the rhythm",
    difficulty = 4,
    placement = "music",
    music = "sounds/the_sports_music.mp3",
    lead = 1.5,
    beats = {
        bass = { enemies = { "bydo_heavy", "bydo_raider" }, minGap = 1.5 },
        mid = { enemies = { "bydo_scout", "bydo_gunner" }, minGap = 1.2 },
        high = { enemies = { "bydo_scout" }, minGap = 1.2 },
    },
    area = { minX = 1980, maxX = 2020, minY = 100, maxY = 980 },
}
