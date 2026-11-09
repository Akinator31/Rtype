-- A rifle fighter that speeds up and aims at the nearest player when wounded.
local function nearestPlayer(enemy, players)
    local nearest, distance = nil, math.huge
    for _, player in ipairs(players) do
        local dx, dy = player.x - enemy.x, player.y - enemy.y
        local squared = dx * dx + dy * dy
        if squared < distance then
            nearest, distance = player, squared
        end
    end
    return nearest
end

return {
    id = "bydo_berserker",
    ship = "bydo_berserker",
    health = 90,
    speed = 360,
    acceleration = 720,
    weapon = "rapid_rifle",
    score = 300,
    collisionDamage = 30,

    update = function(enemy, game)
        local target = nearestPlayer(enemy, game.players)
        if enemy.health <= 40 then
            local climb = target and (target.y - enemy.y) * 2 or 0
            climb = math.max(-enemy.speed * 0.5, math.min(enemy.speed * 0.5, climb))
            enemy:setVelocity(-enemy.speed * 0.85, climb)
            if enemy.x <= 1920 and target and enemy.weapon.ready then
                enemy:fireAt(target.x, target.y)
            end
            return
        end
        enemy:setVelocity(-enemy.speed * 0.5, 0)
        if enemy.x <= 1920 and enemy.weapon.ready then
            enemy:fire()
        end
    end,
}
