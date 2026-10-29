-- Picks one random waypoint, fires four plasma shards, then leaves.
return {
    id = "bydo_wanderer",
    ship = "bydo_scout",
    health = 65,
    speed = 180,
    acceleration = 360,
    weapon = "plasma_shard_cannon",
    score = 260,
    collisionDamage = 25,

    update = function(enemy, game)
        local memory = enemy.memory
        memory.stopX = memory.stopX or game:randomNumber(1200, 1600)
        memory.stopY = memory.stopY or game:randomNumber(180, 900)
        memory.shots = memory.shots or 0
        if memory.shots >= 4 or enemy.age > 12 then
            enemy:setVelocity(-enemy.speed, 0)
            return
        end
        enemy:moveTo(memory.stopX, memory.stopY)
        if enemy.arrived and enemy.weapon.ready then
            enemy:fire()
            memory.shots = memory.shots + 1
        end
    end,
}
