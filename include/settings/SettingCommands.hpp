#include <core/sdk.hpp>
#include <core/cvwrapper.hpp>
#include <settings/Manager.hpp>
#include <init.hpp>
#include <settings/SettingsIO.hpp>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <MiscTemporary.hpp>
#include <thread>
#include <string>
#include <vector>

#include "config.h"
#if ENABLE_VISUALS
#include "Menu.hpp"
#include "special/SettingsManagerList.hpp"
#endif

namespace settings::commands
{
extern std::vector<std::string> sortedConfigs;
}
