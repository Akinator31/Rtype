-- Weaves in, locks on the place a player is at, then dives into it.
local LockX = 1500
local DiveSpeed = 1.5

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
    id = "bydo_drone",
    ship = "bydo_drone",
    health = 15,
    speed = 520,
    acceleration = 1500,
    weapon = "wriggler_gun",
    score = 120,
    collisionDamage = 30,

    update = function(enemy, game)
        local memory = enemy.memory
        if memory.diveX then
            enemy:setVelocity(memory.diveX, memory.diveY)
            return
        end

        local target = findNearestPlayer(enemy, game.players)
        if enemy.x > LockX or not target then
            enemy:setVelocity(-enemy.speed * 0.7, 140 * math.sin(enemy.age * 5))
            return
        end

        local dx, dy = target.x - enemy.x, target.y - enemy.y
        local distance = math.max(1, math.sqrt((dx * dx) + (dy * dy)))
        memory.diveX = dx / distance * enemy.speed * DiveSpeed
        memory.diveY = dy / distance * enemy.speed * DiveSpeed
        enemy:fireAt(target.x, target.y)
    end,
}
