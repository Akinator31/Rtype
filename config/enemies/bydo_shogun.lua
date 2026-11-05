-- The final boss. The shogun comes in and guards, then fights from the right of the game area: mostly it
-- raises its blade and dashes straight at the nearest ship, past it, then flies back; the rest of the time it
-- shoots bursts of plasma. Its health is filled again at each of its four phases: it guards a moment, then
-- fights faster, and in the last two it chains two dashes. In the last one, under 40% of its health, it
-- becomes unkillable and attacks relentlessly, so that the players soon lose: then, offline, allies come to the
-- rescue and the combined attack brings it down (RescueSystem, docs/architecture/game.md).

-- Where it stands, in game units from the left edge
local StandX = 1600
-- Where its box stays: the game area is 1920 by 1080, its box 160 by 316
local MinX = 120
local MaxX = 1800
local MinY = 200
local MaxY = 880
-- Seconds it guards when it arrives and at each new phase
local Recover = 1.5
-- Seconds it flies between two attacks, in its first phase; the last phase halves it
local Fly = 1.2
-- Seconds it fires a burst, in its first phase; the last phase doubles it
local Burst = 1.5
-- Its blade raised before a dash, in seconds: the time the players have to dodge, shorter as it gets angry
local WindUp = 0.5
local AngryWindUp = 0.28
-- The speed of a dash, how far past the ship it goes, and the most seconds it lasts
local DashSpeed = 1450
local AngryDashSpeed = 1900
local Overshoot = 220
local DashAtMost = 1.6
-- Seconds it stays still after a dash: the moment to hit it
local Stagger = 0.4
-- Within this distance of the end of its dash, it is there
local DashArrival = 30
-- On its last life, under this share of its health it cannot be killed any more and attacks relentlessly:
-- seconds it flies between two attacks, its blade raised before a dash, and it stays after one; the chance
-- of a dash, and the dashes in a row
local FrenzyHealth = 0.4
local FrenzyFly = 0.3
local FrenzyWindUp = 0.18
local FrenzyStagger = 0.1
local FrenzyDashChance = 0.8
local FrenzyDashes = 3

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

local function clamp(value, min, max)
    return math.max(min, math.min(max, value))
end

local function lerp(from, to, share)
    return from + ((to - from) * share)
end

-- Starts an action; `memory.since` is when
local function start(enemy, action, duration)
    local memory = enemy.memory
    memory.action = action
    memory.since = enemy.age
    memory.duration = duration
end

-- Where a dash at a ship ends: past it, on the line from the shogun to the ship, inside the game area
local function aimDash(enemy, target)
    local memory = enemy.memory
    local dx, dy = target.x - enemy.x, target.y - enemy.y
    local distance = math.max(1, math.sqrt((dx * dx) + (dy * dy)))
    memory.dashX = clamp(target.x + (dx / distance * Overshoot), MinX, MaxX)
    memory.dashY = clamp(target.y + (dy / distance * Overshoot), MinY, MaxY)
end

return {
    id = "bydo_shogun",
    ship = "bydo_shogun",
    boss = true,
    health = 255,
    phases = 4,
    speed = 700,
    weapon = "shogun_gun_sword",
    score = 10000,
    collisionDamage = 60,

    update = function(enemy, game)
        local memory = enemy.memory
        if not memory.action then
            memory.phase = enemy.phase
            start(enemy, "enter")
        end
        -- 0 in the first phase, 1 in the last one
        local rage = (enemy.phase - 1) / math.max(1, enemy.phases - 1)
        local target = findNearestPlayer(enemy, game.players)
        local elapsed = enemy.age - memory.since

        -- On its last life, under this share of its health it cannot be killed any more and goes on a
        -- rampage: the players can only survive so long
        local frenzy = enemy.phase == enemy.phases and enemy.health < enemy.maxHealth * FrenzyHealth
        if frenzy then
            enemy:protect()
            if memory.action == "guard" or memory.action == "enter" then
                memory.y = clamp(target and target.y or 540, MinY, MaxY)
                start(enemy, "fly", FrenzyFly)
                elapsed = 0
            end
        end
        local flyTime = frenzy and FrenzyFly or lerp(Fly, Fly / 2, rage)
        local windUpTime = frenzy and FrenzyWindUp or lerp(WindUp, AngryWindUp, rage)

        if memory.phase ~= enemy.phase then
            memory.phase = enemy.phase
            enemy:setVelocity(0, 0)
            start(enemy, "guard", Recover)
            elapsed = 0
        end

        if memory.action == "enter" then
            enemy:moveTo(StandX, 540)
            if enemy.arrived then
                start(enemy, "guard", Recover)
            end
        elseif memory.action == "guard" then
            enemy:guard()
            if elapsed >= memory.duration then
                memory.y = clamp(target and target.y or 540, MinY, MaxY)
                start(enemy, "fly", flyTime)
            end
        elseif memory.action == "fly" then
            enemy:moveTo(StandX, memory.y)
            if elapsed >= memory.duration and enemy.arrived then
                if target and game:randomNumber(0, 1) < (frenzy and FrenzyDashChance or lerp(0.4, 0.55, rage)) then
                    -- One dash, two in a row in the last two phases, three in a frenzy
                    memory.dashesLeft = frenzy and FrenzyDashes or (rage >= 0.5 and 2 or 1)
                    start(enemy, "windUp", windUpTime)
                else
                    start(enemy, "shoot", frenzy and Burst or lerp(Burst, Burst * 2, rage))
                end
            end
        elseif memory.action == "shoot" then
            enemy:aim()
            -- Follows the ship slowly while it fires
            if target then
                enemy:moveTo(StandX, clamp(enemy.y + ((target.y - enemy.y) * 0.03), MinY, MaxY))
                if enemy.weapon.ready then
                    enemy:fireAt(target.x, target.y)
                end
            end
            if elapsed >= memory.duration then
                memory.y = clamp(game:randomNumber(MinY, MaxY), MinY, MaxY)
                start(enemy, "fly", flyTime)
            end
        elseif memory.action == "windUp" then
            -- Blade raised, it steps back from the ship it watches, then dashes where the ship is then
            enemy:slash()
            if target then
                local dx, dy = target.x - enemy.x, target.y - enemy.y
                local distance = math.max(1, math.sqrt((dx * dx) + (dy * dy)))
                enemy:setVelocity(-dx / distance * 60, -dy / distance * 60)
            else
                enemy:setVelocity(0, 0)
            end
            if elapsed >= memory.duration then
                if target then
                    aimDash(enemy, target)
                    start(enemy, "dash", DashAtMost)
                else
                    start(enemy, "back")
                end
            end
        elseif memory.action == "dash" then
            enemy:slash()
            local dx, dy = memory.dashX - enemy.x, memory.dashY - enemy.y
            local distance = math.sqrt((dx * dx) + (dy * dy))
            if distance <= DashArrival or elapsed >= memory.duration then
                enemy:setVelocity(0, 0)
                memory.dashesLeft = memory.dashesLeft - 1
                start(enemy, "stagger", frenzy and FrenzyStagger or Stagger)
            else
                local speed = lerp(DashSpeed, AngryDashSpeed, rage)
                enemy:setVelocity(dx / distance * speed, dy / distance * speed)
            end
        elseif memory.action == "stagger" then
            enemy:setVelocity(0, 0)
            if elapsed >= memory.duration then
                if memory.dashesLeft > 0 and target then
                    start(enemy, "windUp", windUpTime)
                else
                    memory.y = clamp(enemy.y, MinY, MaxY)
                    start(enemy, "back")
                end
            end
        elseif memory.action == "back" then
            enemy:moveTo(StandX, memory.y or enemy.y)
            if enemy.arrived then
                start(enemy, "guard", Recover * 0.5)
            end
        end
    end,
}
