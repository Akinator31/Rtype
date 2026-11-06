return {
    name = "space_voyager",
    label = "The first signal",
    description = "An unknown signal crosses the stars, and the Bydos drift along it",
    difficulty = 1,
    placement = "music",
    music = "sounds/space_voyager.wav",
    lead = 1.5,
    beats = {
        bass = { enemies = { "bydo_wanderer", "bydo_scout" }, minGap = 5.0 },
        mid = { enemies = { "bydo_scout", "bydo_mosquito" }, minGap = 4.5 },
        high = { enemies = { "bydo_mosquito", "bydo_drone" }, minGap = 4.5 },
    },
    area = { minX = 1980, maxX = 2020, minY = 100, maxY = 980 },
}
