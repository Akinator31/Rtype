-- Cruises along the top or the bottom of the screen and lobs bombs at the players.
local TopY = 170
local BottomY = 910

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
    id = "bydo_bomber",
    ship = "bydo_bomber",
    health = 120,
    speed = 200,
    acceleration = 400,
    weapon = "bomb_launcher",
    score = 380,
    collisionDamage = 40,

    update = function(enemy, game)
        local memory = enemy.memory
        memory.laneY = memory.laneY or (game:randomInteger(0, 1) == 0 and TopY or BottomY)
        local climb = math.max(-250, math.min(250, (memory.laneY - enemy.y) * 2))
        enemy:setVelocity(-enemy.speed, climb)

        local target = findNearestPlayer(enemy, game.players)
        if target and enemy.x <= 1850 and enemy.weapon.ready then
            enemy:fireAt(target.x, target.y)
        end
    end,
}
