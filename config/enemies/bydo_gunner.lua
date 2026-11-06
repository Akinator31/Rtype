-- Slower than a scout, harder to bring down, and its orbs zigzag. It comes into the game area, stops, waits a
-- moment, fires three orbs at the nearest player, then flies away to the left.

-- Where it stops, in game units from the left edge
local StopX = 1500
-- Seconds it waits once stopped before it fires
local Aim = 1
-- Orbs it fires before it leaves, and the most seconds it stays stopped without a player to fire at
local Shots = 3
local StayAtMost = 6

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
    id = "bydo_gunner",
    ship = "bydo_scout",
    health = 80,
    speed = 450,
    weapon = "plasma_gun",
    score = 250,
    collisionDamage = 40,

    -- Called at every tick for each gunner, with its memory (docs/guides/configure-enemies-and-weapons.md)
    update = function(enemy, game)
        local memory = enemy.memory
        memory.shots = memory.shots or 0
        if memory.shots >= Shots or (memory.stoppedAt and game.time - memory.stoppedAt > StayAtMost) then
            enemy:setVelocity(-enemy.speed, 0)
            return
        end

        enemy:moveTo(StopX, enemy.y)
        if not enemy.arrived then
            return
        end
        memory.stoppedAt = memory.stoppedAt or game.time
        local target = findNearestPlayer(enemy, game.players)
        if target and enemy.weapon.ready and game.time - memory.stoppedAt >= Aim then
            enemy:fireAt(target.x, target.y)
            memory.shots = memory.shots + 1
        end
    end,
}
