-- Stops, then fires fans of five shots spread around the nearest player.
local StopX = 1400
local Volleys = 3
local Fan = { -220, -110, 0, 110, 220 }

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
    id = "bydo_striker",
    ship = "bydo_interceptor",
    health = 70,
    speed = 420,
    acceleration = 840,
    weapon = "scatter_cannon",
    score = 320,
    collisionDamage = 25,

    update = function(enemy, game)
        local memory = enemy.memory
        memory.shots = memory.shots or 0
        local target = findNearestPlayer(enemy, game.players)
        if memory.shots >= Volleys * #Fan or enemy.age > 14 or not target then
            enemy:setVelocity(-enemy.speed, 0)
            return
        end

        enemy:moveTo(StopX, enemy.y)
        if enemy.arrived and enemy.weapon.ready then
            local offset = Fan[(memory.shots % #Fan) + 1]
            enemy:fireAt(target.x, target.y + offset)
            memory.shots = memory.shots + 1
        end
    end,
}
