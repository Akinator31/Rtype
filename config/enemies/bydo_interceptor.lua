-- A fast patrol that weaves across its path and fires often.
return {
    id = "bydo_interceptor",
    ship = "bydo_interceptor",
    health = 11,
    speed = 390,
    acceleration = 780,
    weapon = "rapid_rifle",
    score = 180,
    collisionDamage = 15,

    update = function(enemy, game)
        enemy:setVelocity(-enemy.speed * 0.8, 80 * math.pi * math.cos(math.pi * enemy.age))
        if enemy.x <= 1920 and enemy.weapon.ready then
            enemy:fire()
        end
    end,
}
