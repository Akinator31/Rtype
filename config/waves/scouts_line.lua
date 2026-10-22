-- Two scouts at once, one high and one low: a "manual" wave lists each enemy and where it appears.
return {
    name = "scouts_line",
    label = "Scout line",
    description = "Two scouts from the right, one high and one low",
    -- How hard the wave is, 1 (easiest) to 5
    difficulty = 1,
    placement = "manual",
    -- Seconds between two enemies, 0 for all at once
    spawnInterval = 0,
    -- In the order they appear; the quantity is their number
    spawns = {
        { enemy = "bydo_scout", x = 2000, y = 200 },
        { enemy = "bydo_scout", x = 2000, y = 800 },
    },
}
