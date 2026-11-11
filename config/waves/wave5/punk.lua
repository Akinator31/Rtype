return {
    name = "punk",
    label = "The Shogun threat",
    description = "The Shogun, seized by the Bydos, storms across the galaxy",
    difficulty = 5,
    placement = "music",
    music = "sounds/punk.wav",
    lead = 1.5,
    beats = {
        bass = { enemies = { "bydo_heavy", "bydo_berserker", "bydo_bomber", "bydo_siege", "bydo_carrier" }, minGap = 4.0 },
        mid = { enemies = { "bydo_raider", "bydo_gunner", "bydo_striker", "bydo_mortar", "bydo_sporecaster" }, minGap = 3.0 },
        high = { enemies = { "bydo_interceptor", "bydo_stinger", "bydo_sniper", "bydo_drone", "bydo_mosquito" }, minGap = 3.0 },
    },
    -- Items that appear at random places and times during the wave: `quantity` of each, or a number
    -- drawn from `quantity` to `maxQuantity`
    items = {
        { item = "repair_kit", quantity = 5, maxQuantity = 10 },
        { item = "rapid_fire", quantity = 2, maxQuantity = 5 },
        { item = "shield", quantity = 2, maxQuantity = 4 },
        { item = "bydo_mine", quantity = 7, maxQuantity = 14 },
        { item = "extra_life", quantity = 1, maxQuantity = 2 },
    },
    area = { minX = 1980, maxX = 2020, minY = 100, maxY = 980 },
}
