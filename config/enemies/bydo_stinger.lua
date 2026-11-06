-- Takes position at the top or the bottom, then sweeps across the screen firing swarms of micro missiles.
local StandX = 1350
local TopY = 130
local BottomY = 950

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
    id = "bydo_stinger",
    ship = "bydo_stinger",
    health = 45,
    speed = 480,
    acceleration = 960,
    weapon = "swarm_launcher",
    score = 260,
    collisionDamage = 20,

    update = function(enemy, game)
        local memory = enemy.memory
        if not memory.fromY then
            local top = game:randomInteger(0, 1) == 0
            memory.fromY = top and TopY or BottomY
            memory.toY = top and BottomY or TopY
        end

        if memory.sweeping then
            local direction = memory.toY > memory.fromY and 1 or -1
            if (enemy.y - memory.toY) * direction >= -40 then
                enemy:setVelocity(-enemy.speed, 0)
                return
            end
            enemy:setVelocity(-enemy.speed * 0.1, direction * enemy.speed * 0.45)
            local target = findNearestPlayer(enemy, game.players)
            if target and enemy.weapon.ready then
                enemy:fireAt(target.x, target.y)
            end
            return
        end

        enemy:moveTo(StandX, memory.fromY)
        memory.sweeping = enemy.arrived
    end,
}
