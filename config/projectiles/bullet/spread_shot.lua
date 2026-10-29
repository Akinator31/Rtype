-- A small rectangular bullet with a 36-by-12 collision box.
return {
    id = "spread_shot",
    model = "models/projectiles/player/shot_spread.glb",
    category = "bullet",
    collider = { shape = "box", halfWidth = 18, halfHeight = 6 },
}
