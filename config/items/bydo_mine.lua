-- A trap the Bydos leave: it takes health from the ship that touches it (none from a shielded one).
return {
    id = "bydo_mine",
    model = "models/items/pu_damage.glb",
    collider = { shape = "sphere", radius = 30 },
    effects = {
        -- Hit points taken: a ship left with none is lost
        { type = "damage", amount = 30 },
    },
    lifetime = 12,
    speed = 40,
}
