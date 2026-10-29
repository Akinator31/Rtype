-- Spores and siege missiles create overlapping hazards.
return {
    name = "spore_siege",
    label = "Spore siege",
    description = "Siege missiles and spores cover the whole lane",
    difficulty = 4,
    placement = "manual",
    spawnInterval = 0.6,
    spawns = {
        { enemy = "bydo_sporecaster", x = 2000, y = 210 },
        { enemy = "bydo_siege", x = 2000, y = 360 },
        { enemy = "bydo_wanderer", x = 2000, y = 540 },
        { enemy = "bydo_siege", x = 2000, y = 720 },
        { enemy = "bydo_sporecaster", x = 2000, y = 870 },
        { enemy = "bydo_wanderer", x = 2000, y = 300 },
        { enemy = "bydo_sporecaster", x = 2000, y = 420 },
        { enemy = "bydo_siege", x = 2000, y = 540 },
        { enemy = "bydo_wanderer", x = 2000, y = 780 },
        { enemy = "bydo_sporecaster", x = 2000, y = 660 },
        { enemy = "bydo_siege", x = 2000, y = 340 },
        { enemy = "bydo_scout", x = 2000, y = 900 },
    },
}
