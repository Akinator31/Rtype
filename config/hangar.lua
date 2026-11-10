-- The level of the account from which each ship of the hangar can be equipped. A ship left out of the list
-- is open from level 1. Fields: docs/guides/configure-enemies-and-weapons.md
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
}
