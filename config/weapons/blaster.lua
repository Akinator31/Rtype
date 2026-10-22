-- The weapon of the Bydo scouts: fast laser bolts, eight per magazine.
return {
    id = "blaster",
    -- The id of a projectile of config/projectiles/
    projectile = "pulse",
    -- How its projectiles move, whatever their own category (default "bullet")
    mechanic = "laser",
    -- Speed of the projectiles, to the right, in game units per second
    projectileSpeed = 1000,
    -- Ticks between two shots (60 ticks per second): 5 is 12 shots per second
    fireInterval = 5,
    -- Distance a projectile flies before it vanishes, 0 for no limit
    range = 600,
    -- Shots before reloading, 0 for a weapon that never runs out
    magazineSize = 8,
    -- Ticks to reload an empty magazine
    reloadTicks = 90,
}
