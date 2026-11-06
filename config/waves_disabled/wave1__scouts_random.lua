-- Another easy wave: the game draws one of the waves of each difficulty.
return {
    name = "scouts_random",
    label = "Scout patrol",
    description = "Eight scouts, one after the other",
    difficulty = 1,
    placement = "random",
    enemy = "bydo_scout",
    quantity = 8,
    spawnInterval = 0.8,
    area = { minX = 1980, maxX = 2020, minY = 150, maxY = 930 },
}
