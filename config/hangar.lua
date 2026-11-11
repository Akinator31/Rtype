-- The level of the account from which each ship of the hangar can be equipped. A ship left out of the list
-- is open from level 1. The weapons the players choose two of, in this order, and the level each one needs:
-- a weapon left out of the list cannot be chosen. Fields: docs/guides/configure-enemies-and-weapons.md
return {
    ships = {
        -- `ship` is the code of a ship of the hangar, `level` the level it needs, 1 or more
        { ship = "VX-01", level = 1 },
        { ship = "VX-02", level = 2 },
        { ship = "VX-03", level = 2 },
        { ship = "VX-04", level = 3 },
        { ship = "VX-06", level = 3 },
        { ship = "VX-05", level = 4 },
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
