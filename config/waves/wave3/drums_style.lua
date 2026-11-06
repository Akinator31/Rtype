-- A music wave: the Bydos come on the beats of the music, which the clients play when the wave starts.
-- The server and the client analyse the file to find the beats; the bass, mid and high bands are the kick
-- drum, the snare and the cymbals.
return {
    name = "drums_style",
    label = "The Shogun project",
    description = "Human and Bydos technology are fused into a new arsenal",
    difficulty = 3,
    placement = "music",
    music = "sounds/drums_style.mp3",
    lead = 1.5,
    beats = {
        bass = { enemies = { "bydo_heavy", "bydo_bomber", "bydo_phalanx" }, minGap = 4.5 },
        mid = { enemies = { "bydo_scout", "bydo_gunner", "bydo_striker", "bydo_orbiter", "bydo_mortar" }, minGap = 3.5 },
        high = { enemies = { "bydo_scout", "bydo_drone", "bydo_stinger", "bydo_sniper" }, minGap = 3.5 },
    },
    area = { minX = 1980, maxX = 2020, minY = 100, maxY = 980 },
}
