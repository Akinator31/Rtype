-- Empties a magazine of 5 quick shots: aimed one after another at different points, they make a fan.
return {
    id = "scatter_cannon",
    projectile = "spread_shot",
    mechanic = "bullet",
    projectileSpeed = 750,
    fireInterval = 6,
    range = 900,
    magazineSize = 5,
    reloadTicks = 120,
    damage = 12,
}
