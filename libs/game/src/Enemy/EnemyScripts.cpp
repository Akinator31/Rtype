#include "Enemy/EnemyScripts.hpp"

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/Entities.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/physics/Component/Motion.hpp"
#include "rtype/scripting/State/ScriptError.hpp"

#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <optional>
#include <utility>

namespace rtype::game
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("game");
    return instance;
}

/// Within this distance of its destination, an enemy is on it.
constexpr float ArrivalDistance = 0.5f;

float toSeconds(std::uint32_t ticks)
{
    return static_cast<float>(ticks) / static_cast<float>(TickRate);
}

/// What a behavior sees of a weapon.
struct ScriptWeapon {
    std::string id;
    /// It fires at this tick if it is ordered to.
    bool ready = false;
    /// Shots left before it reloads, math.huge for a weapon that never runs dry.
    float ammo = 0.0f;
    bool reloading = false;
};

/// What a behavior sees of the ship of a player.
struct ScriptPlayer {
    std::uint8_t slot = 0;
    float x = 0.0f;
    float y = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    ScriptWeapon weapon;
};

/// The enemy given to its behavior: what it sees of itself, and the orders it gives, read back after the
/// call. It is a copy owned by Lua: kept by a script after the call, it changes nothing.
class ScriptEnemy
{
public:
    float x = 0.0f;
    float y = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    /// The top speed of its definition, the speed of moveTo().
    float speed = 0.0f;
    /// Seconds since it appeared.
    float age = 0.0f;
    std::uint8_t health = 0;
    /// The health it gets at each phase, to compare `health` with.
    std::uint8_t maxHealth = 0;
    /// Its phase, from 1 to `phases`: it gets all its health back at each one.
    std::uint8_t phase = 1;
    std::uint8_t phases = 1;
    /// It is on the destination of its last moveTo().
    bool arrived = false;
    ScriptWeapon weapon;
    luabridge::LuaRef memory;

    /// The velocity it ordered, if it did.
    std::optional<physics::Velocity> newVelocity;
    EnemyOrders orders;

    explicit ScriptEnemy(luabridge::LuaRef memoryTable) : memory(std::move(memoryTable)) {}

    void setVelocity(float speedX, float speedY)
    {
        newVelocity = physics::Velocity{speedX, speedY};
        orders.destination.reset();
    }

    void moveTo(float destinationX, float destinationY)
    {
        newVelocity.reset();
        orders.destination = ecs::Position{destinationX, destinationY};
    }

    void fire()
    {
        orders.fire = true;
        orders.target.reset();
    }

    void fireAt(float targetX, float targetY)
    {
        orders.fire = true;
        orders.target = ecs::Position{targetX, targetY};
    }

    /// Its stance at this tick, the last one ordered: behind its guard, striking with a blade, aiming its
    /// gun.
    void guard() { orders.stance = EnemyStance::Guard; }
    void slash() { orders.stance = EnemyStance::Slash; }
    void aim() { orders.stance = EnemyStance::Shoot; }

    /// The shots of the players do no damage at this tick, in any stance.
    void protect() { orders.invulnerable = true; }
};

/// What a behavior sees of the game.
class ScriptGame
{
    GameContext *_context;

public:
    std::uint32_t tick = 0;
    /// Seconds since the game started.
    float time = 0.0f;
    /// A list of ScriptPlayer: the players who have a ship, by slot.
    luabridge::LuaRef players;

    ScriptGame(GameContext &context, luabridge::LuaRef playerList)
        : _context(&context), tick(context.tick), time(toSeconds(context.tick)),
          players(std::move(playerList))
    {
    }

    /// From the random numbers of the game, so that it replays the same.
    [[nodiscard]] float randomNumber(float min, float max) const { return _context->randomNumber(min, max); }

    [[nodiscard]] int randomInteger(int first, int last) const
    {
        if (last < first) {
            std::swap(first, last);
        }
        const auto span = static_cast<std::uint32_t>(last - first);
        return first + static_cast<int>(_context->randomBetween(0, span));
    }
};

/// The weapon fires at this tick if asked to: its cooldown and its reload end at this tick or are over.
ScriptWeapon describeWeapon(const GameContext &context, const Armed &armed)
{
    const WeaponDefinition &weapon = context.weapons.getDefinition(armed.weapon);
    const float ammo = weapon.magazineSize == InfiniteAmmo ? std::numeric_limits<float>::infinity()
                                                           : static_cast<float>(armed.ammo);
    return {armed.weapon, armed.cooldown <= 1 && armed.reloadLeft <= 1, ammo, armed.reloadLeft > 0};
}

bool isOnDestination(const ecs::Position &position, const EnemyOrders &orders)
{
    return orders.destination && std::hypot(orders.destination->x - position.x,
                                            orders.destination->y - position.y) <= ArrivalDistance;
}

ScriptEnemy describeEnemy(ecs::Ecs &ecs, GameContext &context, ecs::entity_t entity,
                          const EnemyDefinition &definition, luabridge::LuaRef memory)
{
    ScriptEnemy enemy(std::move(memory));
    const ecs::Position &position = ecs.getComponent<ecs::Position>(entity);
    const physics::Velocity &velocity = ecs.getComponent<physics::Velocity>(entity);
    enemy.x = position.x;
    enemy.y = position.y;
    enemy.velocityX = velocity.x;
    enemy.velocityY = velocity.y;
    enemy.speed = definition.speed;
    enemy.age = toSeconds(ecs.getComponent<Enemy>(entity).age);
    enemy.health = ecs.getComponent<Health>(entity).points;
    enemy.maxHealth = definition.health;
    enemy.phase = ecs.getComponent<Enemy>(entity).phase;
    enemy.phases = definition.phases;
    enemy.weapon = describeWeapon(context, ecs.getComponent<Armed>(entity));
    // The destination is kept from one tick to the next; a shot is ordered again at every tick
    enemy.orders.destination = ecs.getComponent<EnemyOrders>(entity).destination;
    enemy.arrived = isOnDestination(position, enemy.orders);
    return enemy;
}

ScriptPlayer describePlayer(ecs::Ecs &ecs, const GameContext &context, std::uint8_t slot, ecs::entity_t ship)
{
    ScriptPlayer player;
    player.slot = slot;
    const ecs::Position &position = ecs.getComponent<ecs::Position>(ship);
    player.x = position.x;
    player.y = position.y;
    if (ecs.hasComponent<physics::Velocity>(ship)) {
        player.velocityX = ecs.getComponent<physics::Velocity>(ship).x;
        player.velocityY = ecs.getComponent<physics::Velocity>(ship).y;
    }
    if (ecs.hasComponent<Armed>(ship)) {
        player.weapon = describeWeapon(context, ecs.getComponent<Armed>(ship));
    }
    return player;
}

} // namespace

EnemyScripts::EnemyScripts(GameContext &context)
    : _context(context), _state(InstructionLimit), _game(_state.getLuaState())
{
    bindTypes();
}

void EnemyScripts::bindTypes()
{
    // In a table of their own: the scripts get them as arguments, never by their names
    luabridge::getGlobalNamespace(_state.getLuaState())
        .beginNamespace("rtype")
        .beginClass<ScriptWeapon>("Weapon")
        .addProperty("id", &ScriptWeapon::id)
        .addProperty("ready", &ScriptWeapon::ready)
        .addProperty("ammo", &ScriptWeapon::ammo)
        .addProperty("reloading", &ScriptWeapon::reloading)
        .endClass()
        .beginClass<ScriptPlayer>("Player")
        .addProperty("slot", &ScriptPlayer::slot)
        .addProperty("x", &ScriptPlayer::x)
        .addProperty("y", &ScriptPlayer::y)
        .addProperty("velocityX", &ScriptPlayer::velocityX)
        .addProperty("velocityY", &ScriptPlayer::velocityY)
        .addProperty("weapon", &ScriptPlayer::weapon)
        .endClass()
        .beginClass<ScriptEnemy>("Enemy")
        .addProperty("x", &ScriptEnemy::x)
        .addProperty("y", &ScriptEnemy::y)
        .addProperty("velocityX", &ScriptEnemy::velocityX)
        .addProperty("velocityY", &ScriptEnemy::velocityY)
        .addProperty("speed", &ScriptEnemy::speed)
        .addProperty("age", &ScriptEnemy::age)
        .addProperty("health", &ScriptEnemy::health)
        .addProperty("maxHealth", &ScriptEnemy::maxHealth)
        .addProperty("phase", &ScriptEnemy::phase)
        .addProperty("phases", &ScriptEnemy::phases)
        .addProperty("arrived", &ScriptEnemy::arrived)
        .addProperty("weapon", &ScriptEnemy::weapon)
        .addProperty("memory", &ScriptEnemy::memory)
        .addFunction("setVelocity", &ScriptEnemy::setVelocity)
        .addFunction("moveTo", &ScriptEnemy::moveTo)
        .addFunction("fire", &ScriptEnemy::fire)
        .addFunction("fireAt", &ScriptEnemy::fireAt)
        .addFunction("guard", &ScriptEnemy::guard)
        .addFunction("slash", &ScriptEnemy::slash)
        .addFunction("aim", &ScriptEnemy::aim)
        .addFunction("protect", &ScriptEnemy::protect)
        .endClass()
        .beginClass<ScriptGame>("Game")
        .addProperty("tick", &ScriptGame::tick)
        .addProperty("time", &ScriptGame::time)
        .addProperty("players", &ScriptGame::players)
        .addFunction("randomNumber", &ScriptGame::randomNumber)
        .addFunction("randomInteger", &ScriptGame::randomInteger)
        .endClass()
        .endNamespace();
}

void EnemyScripts::startTick(ecs::Ecs &ecs)
{
    _game = luabridge::LuaRef(_state.getLuaState(), ScriptGame(_context, listPlayers(ecs)));
}

luabridge::LuaRef EnemyScripts::listPlayers(ecs::Ecs &ecs) const
{
    luabridge::LuaRef players = luabridge::newTable(_state.getLuaState());
    int index = 1;
    for (std::uint8_t slot = 0; slot < MaxPlayers; ++slot) {
        if (const std::optional<ecs::entity_t> ship = findPlayerShip(ecs, slot)) {
            players[index++] = describePlayer(ecs, _context, slot, *ship);
        }
    }
    return players;
}

bool EnemyScripts::runBehavior(ecs::Ecs &ecs, ecs::entity_t entity, const EnemyDefinition &definition)
{
    const luabridge::LuaRef update = findUpdate(definition);
    if (update.isNil()) {
        return false;
    }
    lua_State *lua = _state.getLuaState();
    const luabridge::LuaRef enemy(lua,
                                  describeEnemy(ecs, _context, entity, definition, getMemory(ecs, entity)));
    const std::array<luabridge::LuaRef, 2> arguments{enemy, _game};
    try {
        _state.callFunction(update, arguments, definition.behavior->file);
    } catch (const scripting::ScriptError &error) {
        stopBehavior(definition, error.what());
        return false;
    }
    const ScriptEnemy orders = enemy.unsafe_cast<ScriptEnemy>();
    if (orders.newVelocity) {
        ecs.getComponent<physics::Velocity>(entity) = *orders.newVelocity;
    }
    ecs.getComponent<EnemyOrders>(entity) = orders.orders;
    return true;
}

luabridge::LuaRef EnemyScripts::findUpdate(const EnemyDefinition &definition)
{
    lua_State *lua = _state.getLuaState();
    if (!definition.behavior || _failed.contains(definition.id)) {
        return luabridge::LuaRef(lua);
    }
    if (const auto found = _updates.find(definition.id); found != _updates.end()) {
        return found->second;
    }
    const EnemyBehavior &behavior = *definition.behavior;
    try {
        const luabridge::LuaRef chunk = _state.runCode(behavior.code, behavior.file);
        luabridge::LuaRef update = chunk.isTable() ? chunk.rawget("update") : luabridge::LuaRef(lua);
        if (!update.isFunction()) {
            stopBehavior(definition, std::format("{}: expected the file to return a table with an `update` "
                                                 "function",
                                                 behavior.file));
            return luabridge::LuaRef(lua);
        }
        _updates.emplace(definition.id, update);
        return update;
    } catch (const scripting::ScriptError &error) {
        stopBehavior(definition, error.what());
    }
    return luabridge::LuaRef(lua);
}

void EnemyScripts::stopBehavior(const EnemyDefinition &definition, const std::string &problem)
{
    logger().error("The behavior of the enemy '{}' is stopped for the rest of the game: {}", definition.id,
                   problem);
    _failed.insert(definition.id);
    _updates.erase(definition.id);
}

luabridge::LuaRef EnemyScripts::getMemory(ecs::Ecs &ecs, ecs::entity_t entity)
{
    // The ids of the ECS are reused: an enemy that just appeared forgets what the one before it remembered
    const auto found = _memories.find(entity);
    if (found != _memories.end() && ecs.getComponent<Enemy>(entity).age > 0) {
        return found->second;
    }
    luabridge::LuaRef memory = luabridge::newTable(_state.getLuaState());
    _memories.insert_or_assign(entity, memory);
    return memory;
}

} // namespace rtype::game
