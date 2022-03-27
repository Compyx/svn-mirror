/** \file   archdep_user_cache_path.c
 * \brief   Retrieve path to the user's cache directory
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 *
 * Get path to user's cache directory, this is where the vice files like
 * autostart-$emu.d64 are stored.
 *
 * OS support:
 *  - Linux
 *  - Windows
 *  - MacOS
 *  - BeOS/Haiku (untested)
 *  - AmigaOS (untested)
 *  - OS/2 (untested)
 *
 */

/*
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "vice.h"
#include "archdep_defs.h"

#include <stddef.h>
#ifdef ARCHDEP_OS_WINDOWS
# include <windows.h>
# include <shlobj.h>
#endif

#include "archdep_xdg.h"

/* TODO: Haiku is a lot more POSIX-like than classic BeOS, so perhaps we should
 *       test for classic BeOS or Haiku and act accordingly.
 */
#if !defined(UNIX_COMPILE) && !defined(ARCHDEP_OS_WINDOWS) \
    && !(defined(BEOS_COMPILE))
# include "archdep_boot_path.h"
#endif
#ifdef BEOS_COMPILE
# include "archdep_home_path.h"
#endif
#include "lib.h"
#include "util.h"

#include "archdep_user_cache_path.h"


/** \brief  User's XDG cache dir
 *
 * Allocated once in the first call to archdep_user_config_path(), should be
 * freed on emulator exit with archdep_user_config_path_free()
 */
static char *user_cache_dir = NULL;


/** \brief  Get path to the VICE cache directory
 *
 * On systems supporting home directories this will return a directory inside
 * the home directory, depending on OS:
 *
 * - Windows: $HOME\\AppData\\Roaming\\vice (I think)
 * - Unix: $HOME/.cache/vice (ie XDG_CACHE_HOME)
 *
 * On other systems the path to the executable is returned.
 *
 * Free memory used on emulator exit with archdep_user_cache_path_free()
 *
 * \return  path to VICE cache directory
 */
const char *archdep_user_cache_path(void)
{
#ifdef ARCHDEP_OS_WINDOWS
    TCHAR szPath[MAX_PATH];
#endif
    /* don't recalculate path if it's already known */
    if (user_cache_dir != NULL) {
        return user_cache_dir;
    }

    /* FIXME: Probably Haiku-specific, not classic BeOS */
#if defined(UNIX_COMPILE) || defined(BEOS_COMPILE)
    char *xdg_cache = archdep_xdg_cache_home();
    user_cache_dir = util_join_paths(xdg_cache, "vice", NULL);
    lib_free(xdg_cache);

#elif defined(ARCHDEP_OS_WINDOWS)
    /*
     * Use WinAPI to get %APPDATA% directory, hopefully more reliable than
     * hardcoding 'AppData/Roaming'. We can't use SHGetKnownFolderPath() here
     * since SDL should be able to run on Windows XP and perhaps even lower.
     */
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, szPath))) {
        user_cache_dir = util_join_paths(szPath, "vice", NULL);
    } else {
        user_cache_dir = NULL;
    }
#else
    user_cache_dir = lib_strdup(archdep_boot_path());
#endif
    return user_cache_dir;
}


/** \brief  Free memory used by the user's config path
 */
void archdep_user_cache_path_free(void)
{
    if (user_cache_dir != NULL) {
        lib_free(user_cache_dir);
        user_cache_dir = NULL;
    }
}
