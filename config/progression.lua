-- The experience the players earn online, where the waves follow each other until every player is out of
-- lives. Fields: docs/guides/configure-enemies-and-weapons.md
return {
    -- Experience for a wave cleared: the ship of the player is alive as it ends
    xpPerWave = 100,
    -- Percent added to the multiplier for each wave cleared in a row after the first: 25 gives x1 for the
    -- first wave, x1.25 for the second, x1.5 for the third... The multiplier counts for the Bydos shot down too
    streakBonus = 25,
    -- Highest multiplier, in percent, 100 or more
    streakCap = 300,
}
