-- The basic weapon of the players: 6 shots per second, never out of ammunition.
return {
    id = "player_cannon",
    projectile = "player_shot",
    projectileSpeed = 1500,
    fireInterval = 10,
    -- Hit points each shot takes from a Bydo, 1 to 255 (default 10): 3 shots for a scout
    damage = 10,
}
