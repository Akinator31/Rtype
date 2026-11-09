-- A loose swarm of unpredictable missile fighters.
return {
    name = "mosquito_swarm",
    label = "Mosquito swarm",
    description = "Six fragile missile fighters dart across the battlefield",
    difficulty = 1,
    placement = "random",
    enemy = "bydo_mosquito",
    quantity = 6,
    spawnInterval = 0.9,
    area = { minX = 1980, maxX = 2040, minY = 140, maxY = 940 },
}
