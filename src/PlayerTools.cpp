/*
  Created on 23.06.18.
*/

#include "common.hpp"
#include <unordered_map>
#include <hoovy.hpp>
#include <playerlist.hpp>
#include "PlayerTools.hpp"
#include "entitycache.hpp"
#include "settings/Bool.hpp"

namespace player_tools
{

static settings::Int betrayal_limit{ "player-tools.betrayal-limit", "2" };

static settings::Boolean taunting{ "player-tools.ignore.taunting", "true" };
static settings::Boolean hoovy{ "player-tools.ignore.hoovy", "true" };
static settings::Boolean ignoreCathook{ "player-tools.ignore.cathook", "true" };

static std::unordered_map<unsigned, unsigned> betrayal_list{};

static CatCommand forgive_all("pt_forgive_all", "Clear betrayal list", []() { betrayal_list.clear(); });

bool shouldTargetSteamId(unsigned id)
{
    if (betrayal_limit)
    {
        if (betrayal_list[id] > int(betrayal_limit))
            return false;
    }

    auto &pl = playerlist::AccessData(id);
    if (pl.state == playerlist::k_EState::CAT && *ignoreCathook)
        return true;
    return false;
}
bool shouldTarget(CachedEntity *entity)
{
    if (entity->m_Type() == ENTITY_PLAYER)
    {
        if (hoovy && IsHoovy(entity))
            return false;
        if (taunting && HasCondition<TFCond_Taunting>(entity) && CE_INT(entity, netvar.m_iTauntIndex) == 3)
            return false;
        if (HasCondition<TFCond_HalloweenGhostMode>(entity))
            return false;
        return shouldTargetSteamId(entity->player_info.friendsID);
    }

    return false;
}

bool shouldAlwaysRenderEspSteamId(unsigned id)
{
    if (id == 0)
        return false;

    auto &pl = playerlist::AccessData(id);
    if (pl.state != playerlist::k_EState::DEFAULT)
        return true;
    return false;
}
bool shouldAlwaysRenderEsp(CachedEntity *entity)
{
    if (entity->m_Type() == ENTITY_PLAYER)
    {
        return shouldAlwaysRenderEspSteamId(entity->player_info.friendsID);
    }

    return false;
}

#if ENABLE_VISUALS
std::optional<colors::rgba_t> forceEspColorSteamId(unsigned id)
{
    if (id == 0)
        return std::nullopt;

    auto pl = playerlist::Color(id);
    if (pl != colors::empty)
        return std::optional<colors::rgba_t>{ pl };

    return std::nullopt;
}
std::optional<colors::rgba_t> forceEspColor(CachedEntity *entity)
{
    if (entity->m_Type() == ENTITY_PLAYER)
    {
        return forceEspColorSteamId(entity->player_info.friendsID);
    }

    return std::nullopt;
}
#endif

void onKilledBy(unsigned id)
{
    auto &pl = playerlist::AccessData(id);
    if (!shouldTargetSteamId(id) && !playerlist::IsFriendly(pl.state))
    {
        // We ignored the gamer, but they still shot us
        if (betrayal_list.find(id) == betrayal_list.end())
            betrayal_list[id] = 0;
        betrayal_list[id]++;
    }
}

void onKilledBy(CachedEntity *entity)
{
    onKilledBy(entity->player_info.friendsID);
}

class PlayerToolsEventListener : public IGameEventListener2
{
    void FireGameEvent(IGameEvent *event) override
    {

        int killer_id = g_IEngine->GetPlayerForUserID(event->GetInt("attacker"));
        int victim_id = g_IEngine->GetPlayerForUserID(event->GetInt("userid"));

        if (victim_id == g_IEngine->GetLocalPlayer())
        {
            onKilledBy(ENTITY(killer_id));
            return;
        }
    }
};

PlayerToolsEventListener &listener()
{
    static PlayerToolsEventListener object{};
    return object;
}

static InitRoutine register_event([]() {
    g_IEventManager2->AddListener(&listener(), "player_death", false);
    EC::Register(
        EC::Shutdown, []() { g_IEventManager2->RemoveListener(&listener()); }, "playerlist_shutdown");
});
} // namespace player_tools
