-- A music wave: the Bydos come on the beats of the music, which the clients play when the wave starts.
-- The server and the client analyse the file to find the beats; the bass, mid and high bands are the kick
-- drum, the snare and the cymbals.
return {
    name = "drums_style",
    label = "The Shogun project",
    description = "Human and Bydos technology are fused into a new arsenal",
    difficulty = 3,
    placement = "music",
    music = "sounds/drums_style.mp3",
    lead = 1.5,
    beats = {
        bass = { enemies = { "bydo_heavy", "bydo_bomber", "bydo_phalanx", "bydo_wanderer" }, minGap = 4.5 },
        mid = { enemies = { "bydo_gunner", "bydo_striker", "bydo_orbiter", "bydo_mortar", "bydo_raider" }, minGap = 3.5 },
        high = { enemies = { "bydo_scout", "bydo_drone", "bydo_stinger", "bydo_sniper", "bydo_mosquito" }, minGap = 3.5 },
    },
    -- Items that appear at random places and times during the wave: `quantity` of each, or a number
    -- drawn from `quantity` to `maxQuantity`
    items = {
        { item = "repair_kit", quantity = 4, maxQuantity = 8 },
        { item = "rapid_fire", quantity = 2, maxQuantity = 4 },
        { item = "shield", quantity = 2, maxQuantity = 3 },
        { item = "bydo_mine", quantity = 5, maxQuantity = 10 },
        { item = "extra_life", quantity = 1 },
    },
    area = { minX = 1980, maxX = 2020, minY = 100, maxY = 980 },
}
