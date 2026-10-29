-- Heavy missile ships arrive in a column with mosquito escorts.
return {
    name = "heavy_column",
    label = "Heavy column",
    description = "Six heavy missile ships with erratic escorts",
    difficulty = 4,
    placement = "manual",
    spawnInterval = 0.5,
    spawns = {
        { enemy = "bydo_mosquito", x = 2000, y = 200 },
        { enemy = "bydo_heavy", x = 2000, y = 300 },
        { enemy = "bydo_mosquito", x = 2000, y = 540 },
        { enemy = "bydo_heavy", x = 2000, y = 540 },
        { enemy = "bydo_mosquito", x = 2000, y = 880 },
        { enemy = "bydo_heavy", x = 2000, y = 780 },
        { enemy = "bydo_mosquito", x = 2000, y = 300 },
        { enemy = "bydo_heavy", x = 2000, y = 400 },
        { enemy = "bydo_mosquito", x = 2000, y = 660 },
        { enemy = "bydo_heavy", x = 2000, y = 680 },
        { enemy = "bydo_mosquito", x = 2000, y = 480 },
        { enemy = "bydo_heavy", x = 2000, y = 540 },
    },
}
