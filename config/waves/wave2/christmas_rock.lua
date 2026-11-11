return {
    name = "christmas_rock",
    label = "First contact",
    description = "The Bydos reach humanity, and their technology far outclasses it",
    difficulty = 2,
    placement = "music",
    music = "sounds/christmas_rock.mp3",
    lead = 1.5,
    beats = {
        bass = { enemies = { "bydo_gunner", "bydo_raider", "bydo_orbiter", "bydo_wanderer" }, minGap = 4.5 },
        mid = { enemies = { "bydo_scout", "bydo_gunner", "bydo_interceptor", "bydo_striker" }, minGap = 3.5 },
        high = { enemies = { "bydo_sporecaster", "bydo_drone", "bydo_stinger", "bydo_mosquito" }, minGap = 3.5 },
    },
    -- Items that appear at random places and times during the wave: `quantity` of each, or a number
    -- drawn from `quantity` to `maxQuantity`
    items = {
        { item = "repair_kit", quantity = 4, maxQuantity = 7 },
        { item = "rapid_fire", quantity = 2, maxQuantity = 4 },
        { item = "shield", quantity = 1, maxQuantity = 3 },
        { item = "bydo_mine", quantity = 4, maxQuantity = 8 },
        { item = "extra_life", quantity = 1 },
    },
    area = { minX = 1980, maxX = 2020, minY = 100, maxY = 980 },
}
