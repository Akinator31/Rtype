-- A small bolt of energy. Fields: docs/guides/configure-enemies-and-weapons.md
return {
    id = "pulse",
    -- Path of the 3D model, drawn by the client (optional: none yet)
    model = "",
    -- "bullet", "missile", "laser" or "plasma" (default "bullet")
    category = "laser",
    -- The shape that hits, centred on the projectile
    collider = { shape = "box", halfWidth = 12, halfHeight = 4 },
}
