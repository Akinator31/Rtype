return {
    name = "epic_anthem",
    label = "The last obstacle",
    description = "Peace is out of reach while the Bydos Shogun still stands",
    difficulty = 5,
    placement = "music",
    music = "sounds/epic_anthem.mp3",
    lead = 1.5,
    beats = {
        bass = { enemies = { "bydo_heavy", "bydo_phalanx", "bydo_carrier", "bydo_siege", "bydo_bomber" }, minGap = 4.5 },
        mid = { enemies = { "bydo_raider", "bydo_orbiter", "bydo_sporecaster", "bydo_mortar", "bydo_berserker" }, minGap = 3.5 },
        high = { enemies = { "bydo_stinger", "bydo_sniper", "bydo_interceptor", "bydo_drone", "bydo_striker" }, minGap = 3.5 },
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
