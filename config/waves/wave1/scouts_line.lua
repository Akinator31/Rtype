-- Three pairs of scouts approach from high and low lanes.
return {
    name = "scouts_line",
    label = "Scout line",
    description = "Three pairs of scouts alternate between high and low lanes",
    -- How hard the wave is, 1 (easiest) to 5
    difficulty = 1,
    placement = "manual",
    -- Seconds between two enemies, 0 for all at once
    spawnInterval = 0.4,
    -- In the order they appear; the quantity is their number
    spawns = {
        { enemy = "bydo_scout", x = 2000, y = 200 },
        { enemy = "bydo_scout", x = 2000, y = 800 },
        { enemy = "bydo_scout", x = 2000, y = 300 },
        { enemy = "bydo_scout", x = 2000, y = 700 },
        { enemy = "bydo_scout", x = 2000, y = 400 },
        { enemy = "bydo_scout", x = 2000, y = 600 },
    },
}
