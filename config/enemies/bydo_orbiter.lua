-- Circles around a point of the screen, firing needles at the nearest player as it goes.
local CenterX = 1450
local Radius = 230
local Pulsation = 1.4
local StayAtMost = 11

local function findNearestPlayer(enemy, players)
    local nearest, nearestDistance = nil, math.huge
    for _, player in ipairs(players) do
        local distance = (player.x - enemy.x) ^ 2 + (player.y - enemy.y) ^ 2
        if distance < nearestDistance then
            nearest, nearestDistance = player, distance
        end
    end
    return nearest
end

return {
    id = "bydo_orbiter",
    ship = "bydo_scout",
    health = 90,
    speed = 460,
    acceleration = 1200,
    weapon = "wriggler_gun",
    score = 300,
    collisionDamage = 25,

    update = function(enemy, game)
        local memory = enemy.memory
        if enemy.age > StayAtMost then
            enemy:setVelocity(-enemy.speed, 0)
            return
        end

        memory.centerY = memory.centerY or game:randomNumber(340, 740)
        local angle = enemy.age * Pulsation
        local goalX = CenterX + (Radius * math.cos(angle))
        local goalY = memory.centerY + (Radius * math.sin(angle))
        local vx, vy = (goalX - enemy.x) * 6, (goalY - enemy.y) * 6
        local length = math.sqrt((vx * vx) + (vy * vy))
        if length > enemy.speed then
            vx, vy = vx / length * enemy.speed, vy / length * enemy.speed
        end
        enemy:setVelocity(vx, vy)

        local target = findNearestPlayer(enemy, game.players)
        if target and enemy.x <= 1900 and enemy.weapon.ready then
            enemy:fireAt(target.x, target.y)
        end
    end,
}
