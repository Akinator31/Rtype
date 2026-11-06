return {
    name = "epic_anthem",
    label = "Epic anthem",
    description = "The Bydos march to the anthem",
    difficulty = 5,
    placement = "music",
    music = "sounds/epic_anthem.mp3",
    lead = 1.5,
    beats = {
        bass = { enemies = { "bydo_heavy", "bydo_phalanx", "bydo_carrier", "bydo_siege" }, minGap = 4.5 },
        mid = { enemies = { "bydo_raider", "bydo_orbiter", "bydo_sporecaster", "bydo_mortar" }, minGap = 3.5 },
        high = { enemies = { "bydo_stinger", "bydo_sniper", "bydo_interceptor", "bydo_drone" }, minGap = 3.5 },
    },
    area = { minX = 1980, maxX = 2020, minY = 100, maxY = 980 },
}
