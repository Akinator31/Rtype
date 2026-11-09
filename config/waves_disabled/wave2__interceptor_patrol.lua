-- Eight fast interceptors arrive from alternating heights.
return {
    name = "interceptor_patrol",
    label = "Interceptor patrol",
    description = "Fast fighters alternate between high and low approaches",
    difficulty = 2,
    placement = "manual",
    spawnInterval = 0.9,
    spawns = {
        { enemy = "bydo_interceptor", x = 2000, y = 240 },
        { enemy = "bydo_interceptor", x = 2000, y = 840 },
        { enemy = "bydo_interceptor", x = 2000, y = 540 },
        { enemy = "bydo_interceptor", x = 2000, y = 320 },
        { enemy = "bydo_interceptor", x = 2000, y = 760 },
        { enemy = "bydo_interceptor", x = 2000, y = 440 },
        { enemy = "bydo_interceptor", x = 2000, y = 640 },
        { enemy = "bydo_interceptor", x = 2000, y = 540 },
    },
}
