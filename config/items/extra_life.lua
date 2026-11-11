-- One life more for the player who picks it up: rare.
return {
    id = "extra_life",
    model = "models/items/pu_extra_life.glb",
    collider = { shape = "sphere", radius = 30 },
    effects = {
        -- Lives given (default 1)
        { type = "extra_life", amount = 1 },
    },
    lifetime = 8,
    speed = 80,
}
