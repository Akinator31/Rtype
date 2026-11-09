-- The most common Bydo: fragile and quick.
return {
    id = "bydo_scout",
    -- The id of a ship of config/ships/
    ship = "bydo_scout",
    -- Hit points, 1 to 255
    health = 30,
    -- Top speed, in game units per second (default 300)
    speed = 650,
    -- Speed gained per second until the top speed (default 600)
    acceleration = 1950,
    -- The id of a weapon of config/weapons/
    weapon = "blaster",
    -- Points for the player who destroys it (default 100)
    score = 100,
    -- Hit points a player ship loses when it runs into it, 0 to 255 (default 20)
    collisionDamage = 20,
}
