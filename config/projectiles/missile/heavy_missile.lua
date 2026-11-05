-- A large rectangular missile with an 80-by-30 collision box.
return {
    id = "heavy_missile",
    model = "models/projectiles/enemy/missile_heavy.glb",
    category = "missile",
    collider = { shape = "box", halfWidth = 40, halfHeight = 15 },
}
