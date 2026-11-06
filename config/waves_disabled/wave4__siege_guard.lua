-- Two siege ships fire from behind a moving screen of smaller enemies.
return {
    name = "siege_guard",
    label = "Siege guard",
    description = "Two siege ships advance behind scouts and gunners",
    difficulty = 4,
    placement = "manual",
    spawnInterval = 0.55,
    spawns = {
        { enemy = "bydo_scout", x = 2000, y = 200 },
        { enemy = "bydo_gunner", x = 2000, y = 400 },
        { enemy = "bydo_siege", x = 2000, y = 540 },
        { enemy = "bydo_gunner", x = 2000, y = 680 },
        { enemy = "bydo_scout", x = 2000, y = 880 },
        { enemy = "bydo_scout", x = 2000, y = 300 },
        { enemy = "bydo_gunner", x = 2000, y = 500 },
        { enemy = "bydo_siege", x = 2000, y = 720 },
        { enemy = "bydo_scout", x = 2000, y = 780 },
        { enemy = "bydo_gunner", x = 2000, y = 360 },
        { enemy = "bydo_scout", x = 2000, y = 540 },
        { enemy = "bydo_interceptor", x = 2000, y = 900 },
    },
}
