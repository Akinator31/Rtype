#include "Config/DefinitionReaders.hpp"

#include <array>
#include <cstdint>
#include <limits>

namespace rtype::game
{

namespace
{

constexpr std::uint64_t MaxU8 = std::numeric_limits<std::uint8_t>::max();
constexpr std::uint64_t MaxU16 = std::numeric_limits<std::uint16_t>::max();
constexpr std::uint64_t MaxU32 = std::numeric_limits<std::uint32_t>::max();

using Shape = decltype(physics::Collider::shape);

/// In the order of the values of ProjectileCategory.
constexpr std::array<std::string_view, 4> CategoryNames{"bullet", "missile", "laser", "plasma"};
/// In the order of the values of WavePlacement.
constexpr std::array<std::string_view, 2> PlacementNames{"manual", "random"};
/// In the order of ShapeReaders.
constexpr std::array<std::string_view, 2> ShapeNames{"box", "sphere"};

ProjectileCategory readCategory(TableReader &table, const std::string &name)
{
    return static_cast<ProjectileCategory>(
        table.readChoice(name, CategoryNames, static_cast<std::size_t>(ProjectileCategory::Bullet)));
}

/// `halfWidth = 12, halfHeight = 4`. Flat, like the game: no depth.
Shape readBox(TableReader &table)
{
    const float halfWidth = table.readNumber("halfWidth", NumberRange::AboveZero);
    const float halfHeight = table.readNumber("halfHeight", NumberRange::AboveZero);
    return physics::Aabb{{}, {halfWidth, halfHeight, 0.0f}};
}

/// `radius = 6`.
Shape readSphere(TableReader &table)
{
    return physics::Sphere{{}, table.readNumber("radius", NumberRange::AboveZero)};
}

/// In the order of ShapeNames.
constexpr std::array<Shape (*)(TableReader &), 2> ShapeReaders{readBox, readSphere};

/// `{ shape = "box", halfWidth = 12, halfHeight = 4 }` or `{ shape = "sphere", radius = 6 }`, with an
/// optional offsetX, offsetY and trigger.
physics::Collider readCollider(TableReader table)
{
    physics::Collider collider;
    collider.shape = ShapeReaders[table.readChoice("shape", ShapeNames)](table);
    collider.offset.x = table.readNumber("offsetX", NumberRange::Any, 0.0f);
    collider.offset.y = table.readNumber("offsetY", NumberRange::Any, 0.0f);
    collider.trigger = table.readBoolean("trigger", false);
    table.checkAllFieldsRead();
    return collider;
}

ShipSounds readShipSounds(TableReader table)
{
    ShipSounds sounds;
    sounds.move = table.readString("move", "");
    sounds.destruction = table.readString("destruction", "");
    sounds.collision = table.readString("collision", "");
    table.checkAllFieldsRead();
    return sounds;
}

WaveSpawn readWaveSpawn(TableReader table)
{
    WaveSpawn spawn;
    spawn.enemy = table.readString("enemy");
    spawn.x = table.readNumber("x", NumberRange::Any);
    spawn.y = table.readNumber("y", NumberRange::Any);
    table.checkAllFieldsRead();
    return spawn;
}

WaveArea readWaveArea(TableReader table)
{
    WaveArea area;
    area.minX = table.readNumber("minX", NumberRange::Any, area.minX);
    area.maxX = table.readNumber("maxX", NumberRange::Any, area.maxX);
    area.minY = table.readNumber("minY", NumberRange::Any, area.minY);
    area.maxY = table.readNumber("maxY", NumberRange::Any, area.maxY);
    table.checkAllFieldsRead();
    return area;
}

} // namespace

ProjectileDefinition readProjectileDefinition(TableReader &table)
{
    ProjectileDefinition definition;
    definition.id = table.readString("id");
    definition.model = table.readString("model", "");
    definition.category = readCategory(table, "category");
    definition.collider = readCollider(table.readTable("collider"));
    table.checkAllFieldsRead();
    return definition;
}

WeaponDefinition readWeaponDefinition(TableReader &table)
{
    WeaponDefinition definition;
    definition.id = table.readString("id");
    definition.projectile = table.readString("projectile");
    definition.mechanic = readCategory(table, "mechanic");
    definition.projectileSpeed =
        table.readNumber("projectileSpeed", NumberRange::AboveZero, definition.projectileSpeed);
    definition.fireInterval =
        static_cast<std::uint32_t>(table.readInteger("fireInterval", 1, MaxU32, definition.fireInterval));
    definition.range = table.readNumber("range", NumberRange::ZeroOrMore, definition.range);
    definition.magazineSize =
        static_cast<std::uint32_t>(table.readInteger("magazineSize", 0, MaxU32, definition.magazineSize));
    definition.reloadTicks =
        static_cast<std::uint32_t>(table.readInteger("reloadTicks", 0, MaxU32, definition.reloadTicks));
    definition.damage = static_cast<std::uint8_t>(table.readInteger("damage", 1, MaxU8, definition.damage));
    table.checkAllFieldsRead();
    return definition;
}

ShipDefinition readShipDefinition(TableReader &table)
{
    ShipDefinition definition;
    definition.id = table.readString("id");
    definition.model = table.readString("model");
    if (table.hasField("sounds")) {
        definition.sounds = readShipSounds(table.readTable("sounds"));
    }
    if (table.hasField("collider")) {
        definition.collider = readCollider(table.readTable("collider"));
    }
    table.checkAllFieldsRead();
    return definition;
}

EnemyDefinition readEnemyDefinition(TableReader &table)
{
    EnemyDefinition definition;
    definition.id = table.readString("id");
    definition.ship = table.readString("ship");
    definition.health = static_cast<std::uint8_t>(table.readInteger("health", 1, MaxU8));
    definition.speed = table.readNumber("speed", NumberRange::AboveZero, definition.speed);
    definition.acceleration =
        table.readNumber("acceleration", NumberRange::ZeroOrMore, definition.acceleration);
    definition.weapon = table.readString("weapon");
    definition.score = static_cast<std::uint32_t>(table.readInteger("score", 0, MaxU32, definition.score));
    definition.collisionDamage =
        static_cast<std::uint8_t>(table.readInteger("collisionDamage", 0, MaxU8, definition.collisionDamage));
    table.checkAllFieldsRead();
    return definition;
}

WaveDefinition readWaveDefinition(TableReader &table)
{
    WaveDefinition definition;
    definition.name = table.readString("name");
    definition.label = table.readString("label", "");
    definition.description = table.readString("description", "");
    definition.difficulty =
        static_cast<std::uint8_t>(table.readInteger("difficulty", MinWaveDifficulty, MaxWaveDifficulty));
    definition.spawnInterval =
        table.readNumber("spawnInterval", NumberRange::ZeroOrMore, definition.spawnInterval);
    definition.placement = static_cast<WavePlacement>(table.readChoice("placement", PlacementNames));
    for (TableReader &spawn : table.readList("spawns")) {
        definition.spawns.push_back(readWaveSpawn(spawn));
    }
    definition.enemy = table.readString("enemy", "");
    if (table.hasField("area")) {
        definition.area = readWaveArea(table.readTable("area"));
    }
    // A manual wave has as many enemies as it lists: its quantity can be left out
    const bool listed = definition.placement == WavePlacement::Manual && !definition.spawns.empty();
    definition.quantity =
        listed
            ? static_cast<std::uint16_t>(table.readInteger("quantity", 1, MaxU16, definition.spawns.size()))
            : static_cast<std::uint16_t>(table.readInteger("quantity", 1, MaxU16));
    table.checkAllFieldsRead();
    return definition;
}

PlayerDefinition readPlayerDefinition(TableReader &table)
{
    PlayerDefinition definition;
    definition.ship = table.readString("ship");
    definition.weapon = table.readString("weapon");
    definition.health = static_cast<std::uint8_t>(table.readInteger("health", 1, MaxU8, definition.health));
    table.checkAllFieldsRead();
    return definition;
}

} // namespace rtype::game
