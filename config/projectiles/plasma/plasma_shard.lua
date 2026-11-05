-- A thin rectangular plasma shot with a 30-by-8 collision box.
return {
    id = "plasma_shard",
    model = "models/projectiles/enemy/shot_spiral.glb",
    category = "plasma",
    collider = { shape = "box", halfWidth = 15, halfHeight = 4 },
}
