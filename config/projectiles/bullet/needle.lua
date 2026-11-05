-- A thin rectangular bullet with a 40-by-8 collision box.
return {
    id = "needle",
    model = "models/projectiles/enemy/needle.glb",
    category = "bullet",
    collider = { shape = "box", halfWidth = 20, halfHeight = 4 },
}
