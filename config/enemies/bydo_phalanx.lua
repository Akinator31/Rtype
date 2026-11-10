-- A slow wall: it guards for two seconds, takes no damage, then opens up and fires missiles.
local StopX = 1500
local Cycle = 4.5
local Guard = 2
local StayAtMost = 22

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
    id = "bydo_phalanx",
    ship = "bydo_bastion",
    health = 92,
    speed = 110,
    acceleration = 220,
    weapon = "trail_pod",
    score = 700,
    collisionDamage = 55,

    update = function(enemy, game)
        local memory = enemy.memory
        if enemy.age > StayAtMost then
            enemy:setVelocity(-enemy.speed, 0)
            return
        end

        enemy:moveTo(StopX, enemy.y)
        if not enemy.arrived then
            return
        end
        memory.arrivedAt = memory.arrivedAt or enemy.age
        local phase = (enemy.age - memory.arrivedAt) % Cycle
        if phase < Guard then
            enemy:guard()
            return
        end

        local target = findNearestPlayer(enemy, game.players)
        if target and enemy.weapon.ready then
            enemy:fireAt(target.x, target.y)
        end
    end,
}
