return {
    name = "cyberpunk_short",
    label = "The warning",
    description = "The decoded message announces a war that cannot be avoided",
    difficulty = 1,
    placement = "music",
    music = "sounds/cyberpunk_short.wav",
    lead = 1.5,
    beats = {
        bass = { enemies = { "bydo_wanderer", "bydo_scout", "bydo_orbiter" }, minGap = 4.5 },
        mid = { enemies = { "bydo_scout", "bydo_mosquito", "bydo_raider" }, minGap = 4.0 },
        high = { enemies = { "bydo_mosquito", "bydo_drone", "bydo_stinger" }, minGap = 4.0 },
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
