-- What a scout looks and sounds like, whoever flies it.
return {
    id = "bydo_scout",
    -- Path of the 3D model, under the assets of the client (required)
    model = "models/drop_ship_v2.glb",
    -- Paths of the sounds, all optional: no sound file yet
    sounds = {
        move = "",
        destruction = "",
        collision = "",
    },
    -- The shape that hits and is hit, centred on the ship (default: a box of 48 by 24). The client draws the
    -- model of the Bydos to the width of this box: its height is that of the hull of the model at that width.
    collider = { shape = "box", halfWidth = 70, halfHeight = 27 },
}
