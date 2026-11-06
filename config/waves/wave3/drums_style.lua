-- A music wave: the Bydos come on the beats of the music, which the clients play when the wave starts.
-- The server and the client analyse the file to find the beats; the bass, mid and high bands are the kick
-- drum, the snare and the cymbals.
return {
    name = "drums_style",
    label = "Drums style",
    description = "The Bydos follow the drums",
    difficulty = 3,
    placement = "music",
    music = "sounds/drums_style.mp3",
    lead = 1.5,
    beats = {
        bass = { enemies = { "bydo_heavy" }, minGap = 1.5 },
        mid = { enemies = { "bydo_scout", "bydo_gunner" }, minGap = 1.2 },
        high = { enemies = { "bydo_scout" }, minGap = 1.2 },
    },
    area = { minX = 1980, maxX = 2020, minY = 100, maxY = 980 },
}
