-- Once every wave is beaten, the shogun of the Bydos comes alone to finish the players.
-- Offline only, in the story mode of the client: the server leaves the waves of difficulty 6 out.
return {
    name = "shogun_duel",
    label = "Shogun duel",
    description = "The final boss: the Bydo shogun guards, slashes and shoots through four phases",
    difficulty = 6,
    music = "sounds/boss_theme.mp3",
    placement = "manual",
    spawns = {
        { enemy = "bydo_shogun", x = 2050, y = 540 },
    },
}
