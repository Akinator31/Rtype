-- A "random" wave: one kind of enemy, placed at random in an area.
return {
    name = "gunners_random",
    label = "Gunners",
    description = "Eight gunners from the right",
    -- How hard the wave is, 1 (easiest) to 5
    difficulty = 2,
    placement = "random",
    enemy = "bydo_gunner",
    quantity = 8,
    spawnInterval = 0.7,
    -- Where they appear, in game units (default: a band just past the right edge, over the whole height)
    area = { minX = 1920, maxX = 2020, minY = 100, maxY = 980 },
}
