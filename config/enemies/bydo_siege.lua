-- Stops to fire three siege missiles, then retreats to the left.
return {
    id = "bydo_siege",
    ship = "bydo_heavy",
    health = 200,
    speed = 85,
    acceleration = 170,
    weapon = "siege_launcher",
    score = 600,
    collisionDamage = 45,

    update = function(enemy, game)
        local memory = enemy.memory
        memory.shots = memory.shots or 0
        if memory.shots >= 3 or enemy.age > 14 then
            enemy:setVelocity(-enemy.speed, 0)
            return
        end
        enemy:moveTo(1580, enemy.y)
        if enemy.arrived and enemy.weapon.ready then
            enemy:fire()
            memory.shots = memory.shots + 1
        end
    end,
}
