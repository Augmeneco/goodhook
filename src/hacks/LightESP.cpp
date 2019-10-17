#include <cdll_int.h>
#include <math.h>
#include <mathlib/vector.h>
#include <string>

#include "config.h"
#include "Bool.hpp"
#include "HookTools.hpp"
#include "Registered.hpp"
#include "colors.hpp"
#include "drawing.hpp"
#include "entitycache.hpp"
#include "entityhitboxcache.hpp"
#include "init.hpp"
#include "interfaces.hpp"
#include "localplayer.hpp"
#include "playerlist.hpp"
#if ENABLE_VISUALS

namespace hacks::shared::lightesp
{
static settings::Boolean enable{ "lightesp.enable", "false" };

static Vector hitp[33];
static Vector minp[33];
static Vector maxp[33];
static bool drawEsp[33];

rgba_t LightESPColor(CachedEntity *ent)
{
    if (!playerlist::IsDefault(ent))
    {
        return playerlist::Color(ent);
    }
    return colors::green;
}

static void cm()
{
    if (!*enable)
        return;
    for (int i = 1; i <= g_IEngine->GetMaxClients(); i++)
    {
        if (g_pLocalPlayer->entity_idx == i)
            continue;
        CachedEntity *pEntity = ENTITY(i);
        if (CE_BAD(pEntity) || !pEntity->m_bAlivePlayer())
        {
            drawEsp[i] = false;
            continue;
        }
        if (pEntity->m_iTeam() == LOCAL_E->m_iTeam() && playerlist::IsDefault(pEntity))
        {
            drawEsp[i] = false;
            continue;
        }
        if (!pEntity->hitboxes.GetHitbox(0))
            continue;
        hitp[i]    = pEntity->hitboxes.GetHitbox(0)->center;
        minp[i]    = pEntity->hitboxes.GetHitbox(0)->min;
        maxp[i]    = pEntity->hitboxes.GetHitbox(0)->max;
        drawEsp[i] = true;
    }
}

void draw()
{
    if (!enable)
        return;
    for (int i = 1; i <= g_IEngine->GetMaxClients(); i++)
    {
        if (!drawEsp[i])
            continue;
        CachedEntity *pEntity = ENTITY(i);
        if (CE_BAD(pEntity) || !pEntity->m_bAlivePlayer())
            continue;
        if (pEntity == LOCAL_E)
            continue;
        Vector out;
        if (draw::WorldToScreen(hitp[i], out))
        {
            float size = 0.0f;
            Vector pout, pout2;
            if (draw::WorldToScreen(minp[i], pout) && draw::WorldToScreen(maxp[i], pout2))
                size = fmaxf(fabsf(pout2.x - pout.x), fabsf(pout2.y - pout.y));

            float minsize = 20.0f;
            if (size < minsize)
                size = minsize;
            draw::Rectangle(out.x - fabsf(pout.x - pout2.x) / 4, out.y - fabsf(pout.y - pout2.y) / 4, fabsf(pout.x - pout2.x) / 2, fabsf(pout.y - pout2.y) / 2, hacks::shared::lightesp::LightESPColor(pEntity));
            draw::Rectangle(out.x - size / 8, out.y - size / 8, size / 4, size / 4, colors::red);
        }
    }
}

static InitRoutine init([]() {
    EC::Register(EC::CreateMove, cm, "cm_lightesp", EC::average);
#if ENABLE_VISUALS
    EC::Register(EC::Draw, draw, "draw_lightesp", EC::average);
#endif
});

} // namespace hacks::shared::lightesp
#endif
