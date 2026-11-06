return {
    name = "cyberpunk_short",
    label = "Cyberpunk short",
    description = "The Bydos jack into a short cyberpunk loop",
    difficulty = 1,
    placement = "music",
    music = "sounds/cyberpunk_short.wav",
    lead = 1.5,
    beats = {
        bass = { enemies = { "bydo_wanderer", "bydo_scout" }, minGap = 4.5 },
        mid = { enemies = { "bydo_scout", "bydo_mosquito" }, minGap = 4.0 },
        high = { enemies = { "bydo_mosquito", "bydo_drone" }, minGap = 4.0 },
    },
    area = { minX = 1980, maxX = 2020, minY = 100, maxY = 980 },
}
