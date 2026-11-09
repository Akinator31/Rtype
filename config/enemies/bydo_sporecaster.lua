-- Approaches the players, fires six spores, then leaves.
return {
    id = "bydo_sporecaster",
    ship = "bydo_sporecaster",
    health = 55,
    speed = 170,
    acceleration = 340,
    weapon = "spore_sprayer",
    score = 240,
    collisionDamage = 22,

    update = function(enemy, game)
        local memory = enemy.memory
        memory.shots = memory.shots or 0
        if memory.shots >= 6 or enemy.age > 8 then
            enemy:setVelocity(-enemy.speed, 0)
            return
        end
        enemy:moveTo(1520, enemy.y)
        if enemy.arrived and enemy.weapon.ready then
            enemy:fire()
            memory.shots = memory.shots + 1
        end
    end,
}
