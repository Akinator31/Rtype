-- A thin segment of laser with a 100-by-12 collision box.
return {
    id = "reflect_laser",
    model = "models/projectiles/player/laser_reflect_segment.glb",
    category = "laser",
    collider = { shape = "box", halfWidth = 50, halfHeight = 6 },
}
