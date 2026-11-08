return {
    name = "cyberpunk_main",
    label = "Hijacked fleet",
    description = "The Bydos take control of human ships, one beat at a time",
    difficulty = 4,
    placement = "music",
    music = "sounds/cyberpunk_main.wav",
    lead = 1.5,
    beats = {
        bass = { enemies = { "bydo_heavy", "bydo_raider", "bydo_bomber", "bydo_phalanx", "bydo_siege" }, minGap = 4.5 },
        mid = { enemies = { "bydo_gunner", "bydo_striker", "bydo_orbiter", "bydo_mortar", "bydo_sporecaster" }, minGap = 3.5 },
        high = { enemies = { "bydo_drone", "bydo_stinger", "bydo_sniper", "bydo_interceptor", "bydo_mosquito" }, minGap = 3.5 },
    },
    area = { minX = 1980, maxX = 2020, minY = 100, maxY = 980 },
}
