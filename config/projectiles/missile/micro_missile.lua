-- A small rectangular missile with a 32-by-10 collision box.
return {
    id = "micro_missile",
    model = "models/projectiles/player/missile_homing.glb",
    category = "missile",
    collider = { shape = "box", halfWidth = 16, halfHeight = 5 },
}
