-- Scouts shield sporecasters approaching from several heights.
return {
    name = "spore_screen",
    label = "Spore screen",
    description = "Five scouts shield three sporecasters",
    difficulty = 2,
    placement = "manual",
    spawnInterval = 0.8,
    spawns = {
        { enemy = "bydo_scout", x = 2000, y = 300 },
        { enemy = "bydo_sporecaster", x = 2000, y = 540 },
        { enemy = "bydo_scout", x = 2000, y = 780 },
        { enemy = "bydo_scout", x = 2000, y = 430 },
        { enemy = "bydo_sporecaster", x = 2000, y = 720 },
        { enemy = "bydo_scout", x = 2000, y = 650 },
        { enemy = "bydo_sporecaster", x = 2000, y = 360 },
        { enemy = "bydo_scout", x = 2000, y = 540 },
    },
}
