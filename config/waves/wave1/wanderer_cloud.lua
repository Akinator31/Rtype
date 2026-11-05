-- A crowd of wanderers fires plasma shards from changing positions.
return {
    name = "wanderer_cloud",
    label = "Wanderer cloud",
    description = "Six wandering shooters fire plasma from separate positions",
    difficulty = 1,
    placement = "random",
    enemy = "bydo_wanderer",
    quantity = 6,
    spawnInterval = 0.9,
    area = { minX = 1980, maxX = 2050, minY = 120, maxY = 960 },
}
