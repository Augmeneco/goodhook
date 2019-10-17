/*
 * CatBot.hpp
 *
 *  Created on: Dec 30, 2017
 *      Author: nullifiedcat
 */

#pragma once

#include "common.hpp"
#include "Registered.hpp"
#include "config.h"
#include "timer.hpp"

class CachedEntity;
namespace ipc {
struct user_data_s;
}  // namespace ipc

namespace hacks::shared::catbot
{

bool is_a_catbot(unsigned steamID);
extern Timer timer_votekicks;
bool should_ignore_player(CachedEntity *player);
void update();
void init();
void level_init();
extern settings::Boolean catbotmode;
extern settings::Boolean anti_motd;

#if ENABLE_IPC
void update_ipc_data(ipc::user_data_s &data);
#endif
} // namespace hacks::shared::catbot
