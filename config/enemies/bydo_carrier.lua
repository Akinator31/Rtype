-- A large Bydo that patrols vertically and aims plasma cores at the players.
return {
    id = "bydo_carrier",
    ship = "bydo_carrier",
    health = 255,
    speed = 150,
    acceleration = 150,
    weapon = "plasma_core_cannon",
    score = 1000,
    collisionDamage = 60,

    update = function(enemy, game)
        if enemy.age > 22 then
            enemy:setVelocity(-enemy.speed, 0)
            return
        end
        local memory = enemy.memory
        memory.targetY = memory.targetY or enemy.y
        if enemy.arrived then
            memory.targetY = memory.targetY < 540 and 780 or 300
        end
        enemy:moveTo(1580, memory.targetY)
        local target = game.players[1]
        if enemy.x <= 1920 and target and enemy.weapon.ready then
            enemy:fireAt(target.x, target.y)
        end
    end,
}
