-- A small bolt of energy. Fields: docs/guides/configure-enemies-and-weapons.md
return {
    id = "pulse",
    model = "models/projectiles/enemy/shot_spiral.glb",
    -- "bullet", "missile", "laser" or "plasma" (default "bullet")
    category = "laser",
    -- The shape that hits, centred on the projectile
    collider = { shape = "box", halfWidth = 34, halfHeight = 11 },
}
