-- Stays at the right edge, follows a player's height, freezes to aim, then fires a beam.
local HoldX = 1800
local Track = 1.5
local Aim = 0.6
local Shots = 3

local function clamp(value, min, max)
    return math.max(min, math.min(max, value))
end

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
    id = "bydo_sniper",
    ship = "bydo_wasp",
    health = 23,
    speed = 420,
    acceleration = 840,
    weapon = "charge_beam",
    score = 350,
    collisionDamage = 25,

    update = function(enemy, game)
        local memory = enemy.memory
        memory.shots = memory.shots or 0
        local target = findNearestPlayer(enemy, game.players)
        if memory.shots >= Shots or enemy.age > 16 or not target then
            enemy:setVelocity(-enemy.speed, 0)
            return
        end

        memory.since = memory.since or enemy.age
        local elapsed = enemy.age - memory.since
        if elapsed < Track then
            enemy:moveTo(HoldX, clamp(target.y, 140, 940))
            if not enemy.arrived then
                memory.since = enemy.age
            end
            return
        end

        enemy:setVelocity(0, 0)
        if elapsed >= Track + Aim and enemy.weapon.ready then
            enemy:fireAt(target.x, target.y)
            memory.shots = memory.shots + 1
            memory.since = enemy.age
        end
    end,
}
