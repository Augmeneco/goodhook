/*
 * prediction.cpp
 *
 *  Created on: Dec 5, 2016
 *      Author: nullifiedcat
 */

#include <bspflags.h>
#include <cdll_int.h>
#include <checksum_md5.h>
#include <const.h>
#include <convar.h>
#include <engine/ICollideable.h>
#include <engine/IEngineTrace.h>
#include <ext/alloc_traits.h>
#include <icliententity.h>
#include <inetchannelinfo.h>
#include <math.h>
#include <mathlib/vector.h>
#include <string.h>
#include <core/sdk.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Registered.hpp"
#include "Settings.hpp"
#include "colors.hpp"
#include "conditions.hpp"
#include "config.h"
#include "crits.hpp"
#include "drawing.hpp"
#include "entitycache.hpp"
#include "enums.hpp"
#include "globals.h"
#include "helpers.hpp"
#include "interfaces.hpp"
#include "localplayer.hpp"
#include "netvars.hpp"
#include "prediction.hpp"
#include "profiler.hpp"
#include "trace.hpp"
#include "usercmd.hpp"
#include "velocity.hpp"

class CBasePlayer;
class IPrediction;

static settings::Boolean debug_enginepred{ "debug.engine-pred-others", "false" };
static settings::Boolean debug_pp_extrapolate{ "debug.pp-extrapolate", "false" };
static settings::Boolean debug_pp_rockettimeping{ "debug.pp-rocket-time-ping", "false" };
static settings::Boolean debug_pp_draw{ "debug.pp-draw", "false" };
static settings::Int projpred_ticks{ "debug.prediction-ticks", "1" };
// TODO there is a Vector() object created each call.

Vector SimpleLatencyPrediction(CachedEntity *ent, int hb)
{
    if (!ent)
        return Vector();
    Vector result;
    GetHitbox(ent, hb, result);
    float latency = g_IEngine->GetNetChannelInfo()->GetLatency(FLOW_OUTGOING) + g_IEngine->GetNetChannelInfo()->GetLatency(FLOW_INCOMING);
    result += CE_VECTOR(ent, netvar.vVelocity) * latency;
    return result;
}

float PlayerGravityMod(CachedEntity *player)
{
    //      int movetype = CE_INT(player, netvar.movetype);
    //      if (movetype == MOVETYPE_FLY || movetype == MOVETYPE_NOCLIP) return
    //      0.0f;
    if (HasCondition<TFCond_Parachute>(player))
        return 0.448f;
    return 1.0f;
}

bool PerformProjectilePrediction(CachedEntity *target, int hitbox)
{
    Vector src, vel, hit;
    ;
    // src = vfunc<Vector(*)(IClientEntity*)>(RAW_ENT(target),
    // 299)(RAW_ENT(target));

    return true;
}

std::vector<std::vector<Vector>> predicted_players_engine{};
std::vector<std::vector<Vector>> player_vel{};
int predicted_player_count = 0;

void Prediction_CreateMove()
{
    static bool setup = false;
    if (!setup)
    {
        setup = true;
        predicted_players_engine.resize(33);
        player_vel.resize(33);
    }
    for (int i = 1; i <= g_GlobalVars->maxClients; i++)
    {
        CachedEntity *ent = ENTITY(i);
        if (CE_GOOD(ent) && ent->m_bAlivePlayer())
        {
            Vector vel;
            if (velocity::EstimateAbsVelocity)
                velocity::EstimateAbsVelocity(RAW_ENT(ent), vel);
            else
                vel = CE_VECTOR(ent, netvar.vVelocity);
            player_vel[i].push_back(vel);
            while (player_vel[i].size() && player_vel[i].size() > *projpred_ticks)
                player_vel[i].erase(player_vel[i].begin());
        }
        else
        {
            player_vel[i] = {};
        }
    }
    if (debug_enginepred)
        for (int i = 1; i <= g_GlobalVars->maxClients; i++)
        {
            CachedEntity *ent = ENTITY(i);
            if (CE_GOOD(ent))
            {
                Vector original = ent->m_vecOrigin();
                predicted_players_engine[i].clear();
                for (int j = 0; j < 32; j++)
                {
                    Vector r                                           = EnginePrediction(ent, g_GlobalVars->interval_per_tick);
                    const_cast<Vector &>(RAW_ENT(ent)->GetAbsOrigin()) = r;
                    CE_VECTOR(ent, 0x354)                              = r;
                    predicted_players_engine[i].push_back(std::move(r));
                }
                const_cast<Vector &>(RAW_ENT(ent)->GetAbsOrigin()) = original;
                CE_VECTOR(ent, 0x354)                              = original;
                // logging::Info("Predicted %d to be at [%.2f, %.2f, %.2f] vs [%.2f,
                // %.2f, %.2f]", i, r.x,r.y,r.z, o.x, o.y, o.z);
                predicted_player_count = i;
            }
        }
}

Vector Predict(Vector &pos, Vector &vel, Vector acceleration, std::optional<float> grounddistance, float &time)
{
    PROF_SECTION(PredictNew);
    Vector result = pos;
    // Quadratic + linear function to map position to time (v+a*t)*t
    vel += acceleration * time;
    result += vel * time;
    if (grounddistance)
        if (result.z < pos.z - *grounddistance)
            result.z = pos.z - *grounddistance;
    return result;
}

Vector PredictStep(Vector pos, Vector &vel, Vector acceleration, std::pair<Vector, Vector> &minmax, float time, float steplength = g_GlobalVars->interval_per_tick, bool vischeck = true, std::optional<float> grounddistance = std::nullopt)
{
    PROF_SECTION(PredictNew)
    Vector result = pos;
    // Quadratic + linear function to go one step into the future

    vel += acceleration * steplength;
    result += vel * steplength;
    if (vischeck)
    {
        Vector modify = result;
        // Standing Player height is 83
        modify.z   = pos.z + 83.0f;
        Vector low = modify;
        low.z -= 8912.0f;

        {
            PROF_SECTION(PredictTraces)
            Ray_t ray;
            trace_t trace;
            ray.Init(modify, low, minmax.first, minmax.second);
            g_ITrace->TraceRay(ray, MASK_PLAYERSOLID, &trace::filter_no_player, &trace);

            float dist = pos.z - trace.endpos.z;
            if (trace.m_pEnt && std::fabs(dist) < 63.0f)
                grounddistance = dist;
        }
    }
    if (grounddistance)
        if (result.z < pos.z - *grounddistance)
            result.z = pos.z - *grounddistance;
    return result;
}

std::vector<Vector> Predict(Vector pos, float offset, Vector vel, Vector acceleration, std::pair<Vector, Vector> minmax, float time, int count, bool vischeck)
{
    std::vector<Vector> positions;
    positions.reserve(count);
    Vector prev;

    pos.z -= offset;

    float dist = DistanceToGround(pos, minmax.first, minmax.second);
    Vector prediction;

    for (int i = 0; i < count; i++)
    {
        time += 1.0f * g_GlobalVars->interval_per_tick;
        if (positions.size() > 0)
        {
            prev = positions.back();
            if (vischeck)
                prediction = PredictStep(prediction, vel, acceleration, minmax, time);
            else
                prediction = PredictStep(prediction, vel, acceleration, minmax, time, g_GlobalVars->interval_per_tick, false, dist);
            positions.push_back({ prediction.x, prediction.y, prediction.z + offset });
        }
        else
        {
            prediction = Predict(pos, vel, acceleration, dist, time);
            prediction.z += offset;
            positions.push_back(prediction);
        }
    }
    return positions;
}

#if ENABLE_VISUALS
void Prediction_PaintTraverse()
{
    if (g_Settings.bInvalid)
        return;
    if (debug_enginepred)
        for (int i = 1; i < predicted_player_count; i++)
        {
            CachedEntity *ent = ENTITY(i);
            if (CE_GOOD(ent))
            {
                Vector previous_screen;
                if (!draw::WorldToScreen(ent->m_vecOrigin(), previous_screen))
                    continue;
                rgba_t color = colors::FromRGBA8(255, 0, 0, 255);
                for (int j = 0; j < predicted_players_engine[i].size(); j++)
                {
                    Vector screen;
                    if (draw::WorldToScreen(predicted_players_engine[i][j], screen))
                    {
                        draw::Line(screen.x, screen.y, previous_screen.x - screen.x, previous_screen.y - screen.y, color, 2);
                        previous_screen = screen;
                    }
                    else
                    {
                        break;
                    }
                    color.r -= 1.0f / 20.0f;
                }
            }
        }
    if (debug_pp_draw)
    {
        static ConVar *sv_gravity = g_ICvar->FindVar("sv_gravity");
        if (!sv_gravity)
        {
            sv_gravity = g_ICvar->FindVar("sv_gravity");
            if (!sv_gravity)
                return;
        }
        for (int i = 1; i <= g_GlobalVars->maxClients; i++)
        {
            auto ent = ENTITY(i);
            if (CE_BAD(ent) || !ent->m_bAlivePlayer())
                continue;
            auto data = Predict(ent->m_vecOrigin(), 0.0f, CE_VECTOR(ent, netvar.vVelocity), Vector(0, 0, -sv_gravity->GetFloat()), std::make_pair(RAW_ENT(ent)->GetCollideable()->OBBMins(), RAW_ENT(ent)->GetCollideable()->OBBMaxs()), 0.0f, 32);
            Vector previous_screen;
            if (!draw::WorldToScreen(ent->m_vecOrigin(), previous_screen))
                continue;
            rgba_t color = colors::FromRGBA8(255, 0, 0, 255);
            for (int j = 0; j < data.size(); j++)
            {
                Vector screen;
                if (draw::WorldToScreen(data[j], screen))
                {
                    draw::Line(screen.x, screen.y, previous_screen.x - screen.x, previous_screen.y - screen.y, color, 2);
                    previous_screen = screen;
                }
                else
                {
                    break;
                }
                color.r -= 1.0f / 20.0f;
            }
        }
    }
}
#endif
Vector EnginePrediction(CachedEntity *entity, float time)
{
    Vector result      = entity->m_vecOrigin();
    IClientEntity *ent = RAW_ENT(entity);

    typedef void (*SetupMoveFn)(IPrediction *, IClientEntity *, CUserCmd *, class IMoveHelper *, CMoveData *);
    typedef void (*FinishMoveFn)(IPrediction *, IClientEntity *, CUserCmd *, CMoveData *);

    void **predictionVtable  = *((void ***) g_IPrediction);
    SetupMoveFn oSetupMove   = (SetupMoveFn)(*(unsigned *) (predictionVtable + 19));
    FinishMoveFn oFinishMove = (FinishMoveFn)(*(unsigned *) (predictionVtable + 20));

    // CMoveData *pMoveData = (CMoveData*)(sharedobj::client->lmap->l_addr +
    // 0x1F69C0C);  CMoveData movedata {};
    auto pMoveData = std::make_unique<CMoveData>();

    float frameTime = g_GlobalVars->frametime;
    float curTime   = g_GlobalVars->curtime;

    CUserCmd fakecmd{};
    memset(&fakecmd, 0, sizeof(CUserCmd));

    Vector vel(0.0f);
    if (entity->m_IDX <= 33 && player_vel[entity->m_IDX].size())
    {
        for (auto entry : player_vel[entity->m_IDX])
            vel += entry;
        vel /= player_vel[entity->m_IDX].size();
    }
    else
        vel = CE_VECTOR(entity, netvar.vVelocity);
    fakecmd.command_number = last_cmd_number;
    fakecmd.forwardmove    = vel.x;
    fakecmd.sidemove       = -vel.y;
    Vector oldangles       = CE_VECTOR(entity, netvar.m_angEyeAngles);
    static Vector zerov{ 0, 0, 0 };
    CE_VECTOR(entity, netvar.m_angEyeAngles) = zerov;

    CUserCmd *original_cmd = NET_VAR(ent, 4188, CUserCmd *);

    NET_VAR(ent, 4188, CUserCmd *) = &fakecmd;

    g_GlobalVars->curtime   = g_GlobalVars->interval_per_tick * NET_INT(ent, netvar.nTickBase);
    g_GlobalVars->frametime = time;

    Vector old_origin      = entity->m_vecOrigin();
    NET_VECTOR(ent, 0x354) = entity->m_vecOrigin();

    *g_PredictionRandomSeed = MD5_PseudoRandom(current_user_cmd->command_number) & 0x7FFFFFFF;
    g_IGameMovement->StartTrackPredictionErrors(reinterpret_cast<CBasePlayer *>(ent));
    oSetupMove(g_IPrediction, ent, &fakecmd, nullptr, pMoveData.get());
    g_IGameMovement->ProcessMovement(reinterpret_cast<CBasePlayer *>(ent), pMoveData.get());
    oFinishMove(g_IPrediction, ent, &fakecmd, pMoveData.get());
    g_IGameMovement->FinishTrackPredictionErrors(reinterpret_cast<CBasePlayer *>(ent));

    NET_VAR(entity, 4188, CUserCmd *) = original_cmd;

    g_GlobalVars->frametime = frameTime;
    g_GlobalVars->curtime   = curTime;

    result                                    = ent->GetAbsOrigin();
    NET_VECTOR(ent, 0x354)                    = old_origin;
    CE_VECTOR(entity, netvar.m_angEyeAngles)  = oldangles;
    const_cast<Vector &>(ent->GetAbsOrigin()) = old_origin;
    const_cast<QAngle &>(ent->GetAbsAngles()) = VectorToQAngle(oldangles);

    return result;
}

Vector ProjectilePrediction_Engine(CachedEntity *ent, int hb, float speed, float gravitymod, float entgmod /* ignored */)
{
    Vector origin = ent->m_vecOrigin();
    Vector hitbox;
    GetHitbox(ent, hb, hitbox);
    Vector hitbox_offset = hitbox - origin;

    if (speed == 0.0f)
        return Vector();
    Vector velocity(0.0f);
    if (ent->m_IDX <= 33 && player_vel[ent->m_IDX].size())
    {
        for (auto entry : player_vel[ent->m_IDX])
            velocity += entry;
        velocity /= player_vel[ent->m_IDX].size();
    }
    else
        velocity = CE_VECTOR(ent, netvar.vVelocity);
    // TODO ProjAim
    float medianTime  = g_pLocalPlayer->v_Eye.DistTo(hitbox) / speed;
    float range       = 1.5f;
    float currenttime = medianTime - range;
    if (currenttime <= 0.0f)
        currenttime = 0.01f;
    float besttime = currenttime;
    float mindelta = 65536.0f;
    Vector bestpos = origin;
    Vector current = origin;
    int maxsteps   = 40;
    bool onground  = false;
    if (ent->m_Type() == ENTITY_PLAYER)
    {
        if (CE_INT(ent, netvar.iFlags) & FL_ONGROUND)
            onground = true;
    }
    float steplength = ((float) (2 * range) / (float) maxsteps);
    Vector ent_mins  = RAW_ENT(ent)->GetCollideable()->OBBMins();
    Vector ent_maxs  = RAW_ENT(ent)->GetCollideable()->OBBMaxs();
    for (int steps = 0; steps < maxsteps; steps++, currenttime += steplength)
    {
        ent->m_vecOrigin() = current;
        current            = EnginePrediction(ent, steplength);

        if (onground)
        {
            float toground = DistanceToGround(current, ent_mins, ent_maxs);
            current.z -= toground;
        }

        float rockettime = g_pLocalPlayer->v_Eye.DistTo(current) / speed;
        if (fabs(rockettime - currenttime) < mindelta)
        {
            besttime = currenttime;
            bestpos  = current;
            mindelta = fabs(rockettime - currenttime);
        }
    }
    const_cast<Vector &>(RAW_ENT(ent)->GetAbsOrigin()) = origin;
    CE_VECTOR(ent, 0x354)                              = origin;
    bestpos.z += (400 * besttime * besttime * gravitymod);
    // S = at^2/2 ; t = sqrt(2S/a)*/
    Vector result = bestpos + hitbox_offset;
    /*logging::Info("[Pred][%d] delta: %.2f   %.2f   %.2f", result.x - origin.x,
                  result.y - origin.y, result.z - origin.z);*/
    return result;
}
Vector BuildingPrediction(CachedEntity *building, Vector vec, float speed, float gravity)
{
    if (!vec.z || CE_BAD(building))
        return Vector();
    Vector result = vec;
    // if (not debug_pp_extrapolate) {
    //} else {
    //        result = SimpleLatencyPrediction(ent, hb);
    //
    //}

    if (speed == 0.0f)
        return Vector();
    trace::filter_no_player.SetSelf(RAW_ENT(building));
    float dtg = DistanceToGround(vec, RAW_ENT(building)->GetCollideable()->OBBMins(), RAW_ENT(building)->GetCollideable()->OBBMaxs());
    // TODO ProjAim
    float medianTime  = g_pLocalPlayer->v_Eye.DistTo(result) / speed;
    float range       = 1.5f;
    float currenttime = medianTime - range;
    if (currenttime <= 0.0f)
        currenttime = 0.01f;
    float besttime = currenttime;
    float mindelta = 65536.0f;
    Vector bestpos = result;
    int maxsteps   = 300;
    for (int steps = 0; steps < maxsteps; steps++, currenttime += ((float) (2 * range) / (float) maxsteps))
    {
        Vector curpos = result;
        if (dtg > 0.0f)
        {
            if (curpos.z < result.z - dtg)
                curpos.z = result.z - dtg;
        }
        float rockettime = g_pLocalPlayer->v_Eye.DistTo(curpos) / speed;
        if (debug_pp_rockettimeping)
            rockettime += g_IEngine->GetNetChannelInfo()->GetLatency(FLOW_OUTGOING);
        if (fabs(rockettime - currenttime) < mindelta)
        {
            besttime = currenttime;
            bestpos  = curpos;
            mindelta = fabs(rockettime - currenttime);
        }
    }
    if (debug_pp_rockettimeping)
        besttime += g_IEngine->GetNetChannelInfo()->GetLatency(FLOW_OUTGOING);
    bestpos.z += (400 * besttime * besttime * gravity);
    // S = at^2/2 ; t = sqrt(2S/a)*/
    return bestpos;
}

Vector ProjectilePrediction(CachedEntity *ent, int hb, float speed, float gravitymod, float entgmod)
{
    Vector origin = ent->m_vecOrigin();
    Vector hitbox;
    GetHitbox(ent, hb, hitbox);
    Vector hitbox_offset = hitbox - origin;

    if (speed == 0.0f)
        return Vector();
    Vector velocity(0.0f);
    if (ent->m_IDX <= 33 && player_vel[ent->m_IDX].size())
    {
        for (auto entry : player_vel[ent->m_IDX])
            velocity += entry;
        velocity /= player_vel[ent->m_IDX].size();
    }
    else
        velocity = CE_VECTOR(ent, netvar.vVelocity);
    // TODO ProjAim
    float medianTime  = g_pLocalPlayer->v_Eye.DistTo(hitbox) / speed;
    float range       = 1.5f;
    float currenttime = medianTime - range;
    if (currenttime <= 0.0f)
        currenttime = 0.01f;
    float besttime = currenttime;
    float mindelta = 65536.0f;
    Vector bestpos = origin;
    Vector current = origin;
    int maxsteps   = 40;
    bool onground  = false;
    if (ent->m_Type() == ENTITY_PLAYER)
    {
        if (CE_INT(ent, netvar.iFlags) & FL_ONGROUND)
            onground = true;
    }

    static ConVar *sv_gravity = g_ICvar->FindVar("sv_gravity");
    float steplength          = ((float) (2 * range) / (float) maxsteps);
    auto minmax               = std::make_pair(RAW_ENT(ent)->GetCollideable()->OBBMins(), RAW_ENT(ent)->GetCollideable()->OBBMaxs());

    float dist_to_ground = DistanceToGround(origin, minmax.first, minmax.second);
    Vector acceleration  = { 0, 0, -(sv_gravity->GetFloat() * entgmod) };

    Vector last = origin;

    for (int steps = 0; steps < maxsteps; steps++, currenttime += steplength)
    {
        if (steps == 0)
            last = Predict(last, velocity, acceleration, dist_to_ground, currenttime);
        else
            last = PredictStep(last, velocity, acceleration, minmax, currenttime, steplength);

        current = last;
        if (onground)
        {
            float toground = DistanceToGround(current, minmax.first, minmax.second);
            current.z -= toground;
        }

        float rockettime = g_pLocalPlayer->v_Eye.DistTo(current) / speed;
        if (fabs(rockettime - currenttime) < mindelta)
        {
            besttime = currenttime;
            bestpos  = current;
            mindelta = fabs(rockettime - currenttime);
        }
    }
    bestpos.z += (sv_gravity->GetFloat() / 2.0f * besttime * besttime * gravitymod);
    // S = at^2/2 ; t = sqrt(2S/a)*/
    Vector result = bestpos + hitbox_offset;
    /*logging::Info("[Pred][%d] delta: %.2f   %.2f   %.2f", result.x - origin.x,
                  result.y - origin.y, result.z - origin.z);*/
    return result;
}

float DistanceToGround(CachedEntity *ent)
{
    Vector origin = ent->m_vecOrigin();
    Vector mins   = RAW_ENT(ent)->GetCollideable()->OBBMins();
    Vector maxs   = RAW_ENT(ent)->GetCollideable()->OBBMins();
    return DistanceToGround(origin, mins, maxs);
}

float DistanceToGround(Vector origin, Vector mins, Vector maxs)
{
    trace_t ground_trace;
    Ray_t ray;
    Vector endpos = origin;
    endpos.z -= 8192;
    ray.Init(origin, endpos, mins, maxs);
    g_ITrace->TraceRay(ray, MASK_PLAYERSOLID, &trace::filter_no_player, &ground_trace);
    return std::fabs(origin.z - ground_trace.endpos.z);
}

float DistanceToGround(Vector origin)
{
    trace_t ground_trace;
    Ray_t ray;
    Vector endpos = origin;
    endpos.z -= 8192;
    ray.Init(origin, endpos);
    g_ITrace->TraceRay(ray, MASK_PLAYERSOLID, &trace::filter_no_player, &ground_trace);
    return std::fabs(origin.z - ground_trace.endpos.z);
}
