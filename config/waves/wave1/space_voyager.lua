return {
    name = "space_voyager",
    label = "The first signal",
    description = "An unknown signal crosses the stars, and the Bydos drift along it",
    difficulty = 1,
    placement = "music",
    music = "sounds/space_voyager.wav",
    lead = 1.5,
    beats = {
        bass = { enemies = { "bydo_wanderer", "bydo_gunner", "bydo_orbiter" }, minGap = 5.0 },
        mid = { enemies = { "bydo_scout", "bydo_mosquito", "bydo_sporecaster" }, minGap = 4.5 },
        high = { enemies = { "bydo_raider", "bydo_drone", "bydo_interceptor" }, minGap = 4.5 },
    },
    -- Items that appear at random places and times during the wave: `quantity` of each, or a number
    -- drawn from `quantity` to `maxQuantity`
    items = {
        { item = "repair_kit", quantity = 3, maxQuantity = 6 },
        { item = "rapid_fire", quantity = 2, maxQuantity = 3 },
        { item = "shield", quantity = 1, maxQuantity = 2 },
        { item = "bydo_mine", quantity = 3, maxQuantity = 6 },
    },
    area = { minX = 1980, maxX = 2020, minY = 100, maxY = 980 },
}
