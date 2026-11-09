-- Raiders rush in from several lanes behind gunner support.
return {
    name = "raider_attack",
    label = "Raider attack",
    description = "Six close-range raiders attack behind two gunners",
    difficulty = 2,
    placement = "manual",
    spawnInterval = 0.7,
    spawns = {
        { enemy = "bydo_gunner", x = 2000, y = 540 },
        { enemy = "bydo_raider", x = 2000, y = 300 },
        { enemy = "bydo_raider", x = 2000, y = 780 },
        { enemy = "bydo_raider", x = 2000, y = 440 },
        { enemy = "bydo_gunner", x = 2000, y = 700 },
        { enemy = "bydo_raider", x = 2000, y = 620 },
        { enemy = "bydo_raider", x = 2000, y = 220 },
        { enemy = "bydo_raider", x = 2000, y = 860 },
    },
}
