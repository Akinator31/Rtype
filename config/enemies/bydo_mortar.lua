-- Patrols up and down at the right of the screen and lobs spores that speed up.
local StandX = 1680
local MinY = 200
local MaxY = 880
local Shots = 9

return {
    id = "bydo_mortar",
    ship = "bydo_mortar",
    health = 70,
    speed = 260,
    acceleration = 520,
    weapon = "spore_mortar",
    score = 280,
    collisionDamage = 25,

    update = function(enemy, game)
        local memory = enemy.memory
        memory.shots = memory.shots or 0
        if memory.shots >= Shots or enemy.age > 14 then
            enemy:setVelocity(-enemy.speed, 0)
            return
        end

        if enemy.x > StandX + 20 then
            enemy:moveTo(StandX, enemy.y)
            return
        end

        memory.direction = memory.direction or (game:randomInteger(0, 1) == 0 and -1 or 1)
        if enemy.y <= MinY then
            memory.direction = 1
        elseif enemy.y >= MaxY then
            memory.direction = -1
        end
        enemy:setVelocity(0, memory.direction * enemy.speed * 0.6)

        if enemy.weapon.ready then
            enemy:fire()
            memory.shots = memory.shots + 1
        end
    end,
}
