-- A fragile missile fighter that darts between random points before fleeing.
return {
    id = "bydo_mosquito",
    ship = "bydo_mosquito",
    health = 25,
    speed = 340,
    weapon = "micro_missile_launcher",
    score = 200,
    collisionDamage = 15,

    update = function(enemy, game)
        if enemy.age > 12 or enemy.x < 200 then
            enemy:setVelocity(-enemy.speed, 0)
            return
        end
        local memory = enemy.memory
        if not memory.targetX or enemy.arrived then
            memory.targetX = enemy.x - game:randomNumber(180, 320)
            memory.targetY = game:randomNumber(120, 960)
        end
        enemy:moveTo(memory.targetX, memory.targetY)
        if enemy.x <= 1920 and enemy.weapon.ready then
            enemy:fire()
        end
    end,
}
