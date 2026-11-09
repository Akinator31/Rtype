-- Interceptors protect two slow heavy missile ships.
return {
    name = "heavy_escort",
    label = "Heavy escort",
    description = "Eight fast fighters escort two heavy missile ships",
    difficulty = 3,
    placement = "manual",
    spawnInterval = 0.55,
    spawns = {
        { enemy = "bydo_interceptor", x = 2000, y = 260 },
        { enemy = "bydo_interceptor", x = 2000, y = 820 },
        { enemy = "bydo_heavy", x = 2000, y = 540 },
        { enemy = "bydo_interceptor", x = 2000, y = 400 },
        { enemy = "bydo_interceptor", x = 2000, y = 680 },
        { enemy = "bydo_interceptor", x = 2000, y = 320 },
        { enemy = "bydo_interceptor", x = 2000, y = 760 },
        { enemy = "bydo_heavy", x = 2000, y = 540 },
        { enemy = "bydo_interceptor", x = 2000, y = 460 },
        { enemy = "bydo_interceptor", x = 2000, y = 620 },
    },
}
