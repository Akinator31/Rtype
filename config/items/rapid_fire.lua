-- The weapon of the ship that picks it up fires faster for a while.
return {
    id = "rapid_fire",
    model = "models/items/pu_fire_rate.glb",
    collider = { shape = "sphere", radius = 30 },
    effects = {
        -- Twice as fast, for 8 seconds
        { type = "rapid_fire", factor = 2, duration = 8 },
    },
    lifetime = 10,
    speed = 60,
}
