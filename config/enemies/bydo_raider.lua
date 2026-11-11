-- Rushes the nearest player, spits one burst of flame at close range, then escapes to the right.
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
    id = "bydo_raider",
    ship = "bydo_raider",
    health = 28,
    speed = 800,
    weapon = "flame_burst",
    score = 275,
    collisionDamage = 30,

    update = function(enemy, game)
        local memory = enemy.memory
        local target = nearestPlayer(enemy, game.players)
        if memory.retreating or enemy.age >= 8 or not target then
            memory.retreating = true
            enemy:setVelocity(enemy.speed * 1.25, 0)
            return
        end

        local dx, dy = target.x - enemy.x, target.y - enemy.y
        if dx * dx + dy * dy <= 220 * 220 and enemy.x <= 1920 and enemy.weapon.ready then
            enemy:fireAt(target.x, target.y)
            memory.retreating = true
            enemy:setVelocity(enemy.speed * 1.25, 0)
            return
        end
        enemy:moveTo(target.x + 200, target.y)
    end,
}
