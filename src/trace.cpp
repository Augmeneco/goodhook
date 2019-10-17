/*
 * trace.cpp
 *
 *  Created on: Oct 10, 2016
 *      Author: nullifiedcat
 */

// This file is a mess. I need to fix it. TODO

/* Default Filter */

#include <client_class.h>
#include <engine/IEngineTrace.h>
#include <icliententity.h>

#include "classinfo.hpp"
#include "logging.hpp"
#include "trace.hpp"

class IHandleEntity;

trace::FilterDefault::FilterDefault()
{
    m_pSelf = nullptr;
}

trace::FilterDefault::~FilterDefault()
{
}

void trace::FilterDefault::SetSelf(IClientEntity *self)
{
    if (self == nullptr)
    {
        logging::Info("nullptr in FilterDefault::SetSelf");
    }
    m_pSelf = self;
}

bool trace::FilterDefault::ShouldHitEntity(IHandleEntity *handle, int mask)
{
    IClientEntity *entity;
    ClientClass *clazz;

    if (!handle)
        return false;
    entity = (IClientEntity *) handle;
    clazz  = entity->GetClientClass();
    /* Ignore invisible entities that we don't wanna hit */
    switch (clazz->m_ClassID)
    {
    // TODO magic numbers: invisible entity ids
    case CL_CLASS(CFuncRespawnRoomVisualizer):
    case CL_CLASS(CTFMedigunShield):
    case CL_CLASS(CFuncAreaPortalWindow):
        return false;
    }
    /* Do not hit yourself. Idiot. */
    if (entity == m_pSelf)
        return false;
    return true;
}

TraceType_t trace::FilterDefault::GetTraceType() const
{
    return TRACE_EVERYTHING;
}

/* No-Player filter */

trace::FilterNoPlayer::FilterNoPlayer()
{
    m_pSelf = nullptr;
}

trace::FilterNoPlayer::~FilterNoPlayer(){};

void trace::FilterNoPlayer::SetSelf(IClientEntity *self)
{
    if (self == nullptr)
    {
        logging::Info("nullptr in FilterNoPlayer::SetSelf");
        return;
    }
    m_pSelf = self;
}

bool trace::FilterNoPlayer::ShouldHitEntity(IHandleEntity *handle, int mask)
{
    IClientEntity *entity;
    ClientClass *clazz;

    if (!handle)
        return false;
    entity = (IClientEntity *) handle;
    clazz  = entity->GetClientClass();

    /* Ignore invisible entities that we don't wanna hit */
    switch (clazz->m_ClassID)
    {
    // TODO magic numbers: invisible entity ids
    case CL_CLASS(CTFPlayerResource):
    case CL_CLASS(CTFPlayer):
    case CL_CLASS(CFuncRespawnRoomVisualizer):
    case CL_CLASS(CTFMedigunShield):
    case CL_CLASS(CFuncAreaPortalWindow):
        return false;
    }
    /* Do not hit yourself. Idiot. */
    if (entity == m_pSelf)
        return false;
    return true;
}

TraceType_t trace::FilterNoPlayer::GetTraceType() const
{
    return TRACE_EVERYTHING;
}
/* No-Entity filter */

trace::FilterNoEntity::FilterNoEntity()
{
    m_pSelf = nullptr;
}

trace::FilterNoEntity::~FilterNoEntity(){};

void trace::FilterNoEntity::SetSelf(IClientEntity *self)
{
    if (self == nullptr)
    {
        logging::Info("nullptr in FilterNoPlayer::SetSelf");
        return;
    }
    m_pSelf = self;
}

bool trace::FilterNoEntity::ShouldHitEntity(IHandleEntity *handle, int mask)
{
    IClientEntity *entity;
    ClientClass *clazz;

    if (!handle)
        return false;
    entity = (IClientEntity *) handle;
    clazz  = entity->GetClientClass();

    // Hit doors, carts, etc
    switch (clazz->m_ClassID)
    {
    case CL_CLASS(CWorld):
    case CL_CLASS(CPhysicsProp):
    case CL_CLASS(CDynamicProp):
    case CL_CLASS(CBaseDoor):
    case CL_CLASS(CBaseEntity):
        return true;
    }
    return false;
}

TraceType_t trace::FilterNoEntity::GetTraceType() const
{
    return TRACE_EVERYTHING;
}
/* Penetration Filter */

trace::FilterPenetration::FilterPenetration()
{
    m_pSelf = nullptr;
}

trace::FilterPenetration::~FilterPenetration(){};

void trace::FilterPenetration::SetSelf(IClientEntity *self)
{
    if (self == nullptr)
    {
        logging::Info("nullptr in FilterPenetration::SetSelf");
    }
    m_pSelf = self;
}

bool trace::FilterPenetration::ShouldHitEntity(IHandleEntity *handle, int mask)
{
    IClientEntity *entity;
    ClientClass *clazz;

    if (!handle)
        return false;
    entity = (IClientEntity *) handle;
    clazz  = entity->GetClientClass();
    /* Ignore invisible entities that we don't wanna hit */
    switch (clazz->m_ClassID)
    {
    // TODO magic numbers: invisible entity ids
    case CL_CLASS(CFuncRespawnRoomVisualizer):
    case CL_CLASS(CTFMedigunShield):
    case CL_CLASS(CFuncAreaPortalWindow):
        return false;
    case CL_CLASS(CTFPlayer):
        if (!m_pIgnoreFirst && (entity != m_pSelf))
        {
            m_pIgnoreFirst = entity;
        }
    }
    /* Do not hit yourself. Idiot. */
    if (entity == m_pSelf)
        return false;
    if (entity == m_pIgnoreFirst)
        return false;
    return true;
}

TraceType_t trace::FilterPenetration::GetTraceType() const
{
    return TRACE_EVERYTHING;
}

void trace::FilterPenetration::Reset()
{
    m_pIgnoreFirst = 0;
}

trace::FilterDefault trace::filter_default{};
trace::FilterNoPlayer trace::filter_no_player{};
trace::FilterNoEntity trace::filter_no_entity{};
trace::FilterPenetration trace::filter_penetration{};
