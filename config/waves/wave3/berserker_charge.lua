-- Berserkers pressure both lanes with support from fast fighters.
return {
    name = "berserker_charge",
    label = "Berserker charge",
    description = "Five berserkers attack with interceptor support",
    difficulty = 3,
    placement = "manual",
    spawnInterval = 0.7,
    spawns = {
        { enemy = "bydo_interceptor", x = 2000, y = 180 },
        { enemy = "bydo_berserker", x = 2000, y = 300 },
        { enemy = "bydo_berserker", x = 2000, y = 780 },
        { enemy = "bydo_interceptor", x = 2000, y = 900 },
        { enemy = "bydo_berserker", x = 2000, y = 540 },
        { enemy = "bydo_interceptor", x = 2000, y = 360 },
        { enemy = "bydo_berserker", x = 2000, y = 660 },
        { enemy = "bydo_interceptor", x = 2000, y = 720 },
        { enemy = "bydo_berserker", x = 2000, y = 420 },
        { enemy = "bydo_interceptor", x = 2000, y = 540 },
    },
}
