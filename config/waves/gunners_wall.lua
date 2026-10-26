return {
    name = "gunners_wall",
    label = "Gunner wall",
    description = "Four gunners at once, from top to bottom",
    difficulty = 4,
    placement = "manual",
    spawnInterval = 0,
    spawns = {
        { enemy = "bydo_gunner", x = 2000, y = 180 },
        { enemy = "bydo_gunner", x = 2000, y = 420 },
        { enemy = "bydo_gunner", x = 2000, y = 660 },
        { enemy = "bydo_gunner", x = 2000, y = 900 },
    },
}
