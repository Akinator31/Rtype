-- The level of the account from which each ship of the hangar can be equipped, and its characteristics. A
-- ship left out of the list is open from level 1, with the characteristics of the rules. The weapons the
-- players choose two of, in this order, and the level each one needs: a weapon left out of the list cannot
-- be chosen. Fields: docs/guides/configure-enemies-and-weapons.md
return {
    ships = {
        -- `ship` is the code of a ship of the hangar, `level` the level it needs, 1 or more. `speed`, `power`
        -- and `armor`, 0 to 1 (default 0.5), multiply its speed, the damage of its shots and its health by
        -- 0.75 at 0, 1 at 0.5, 1.25 at 1
        { ship = "VX-01", level = 1, speed = 0.90, power = 0.30, armor = 0.25 },
        { ship = "VX-02", level = 2, speed = 0.70, power = 0.55, armor = 0.35 },
        { ship = "VX-03", level = 2, speed = 0.30, power = 0.60, armor = 0.95 },
        { ship = "VX-04", level = 3, speed = 0.80, power = 0.70, armor = 0.50 },
        { ship = "VX-06", level = 3, speed = 0.25, power = 0.90, armor = 0.80 },
        { ship = "VX-05", level = 4, speed = 0.60, power = 0.85, armor = 0.60 },
    },
    weapons = {
        -- `weapon` is the id of a weapon of config/weapons/, `level` the level it needs, 1 or more
        { weapon = "player_cannon", level = 1 },
        { weapon = "force_cannon_lv1", level = 1 },
        { weapon = "rapid_rifle", level = 1 },
        { weapon = "scatter_cannon", level = 2 },
        { weapon = "micro_missile_launcher", level = 2 },
        { weapon = "force_cannon_lv2", level = 2 },
        { weapon = "plasma_shard_cannon", level = 3 },
        { weapon = "heavy_missile_launcher", level = 3 },
        { weapon = "force_cannon_lv3", level = 3 },
        { weapon = "plasma_gun", level = 4 },
        { weapon = "charge_beam", level = 4 },
    },
}
