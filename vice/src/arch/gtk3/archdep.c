/*
 * archdep.c - Miscellaneous system-specific stuff.
 *
 * Written by
 *  Marco van den Heuvel <blackystardust68@yahoo.com>
 *  Bas Wassink <b.wassink@ziggo.nl>
 *
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


/** \file   src/arch/gtk3/archdep.c
 * \brief   Wrappers for architecture/OS-specific code
 *
 * I've decided to use GLib's use of the XDG specification and the standard
 * way of using paths on Windows. So some files may not be where the older
 * ports expect them to be. For example, vicerc will be in $HOME/.config/vice
 * now, not $HOME/.vice.
 */

#include "vice.h"

#include <stdio.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "findpath.h"
#include "ioutil.h"
#include "lib.h"
#include "log.h"
#include "machine.h"
#include "util.h"


/** \brief  Prefix used for autostart disk images
 */
#define AUTOSTART_FILENAME_PREFIX   "autostart-"


/** \brief  Suffix used for autostart disk images
 */
#define AUTOSTART_FILENAME_SUFFIX   ".d64"


/** \brief  Reference to argv[0]
 *
 * FIXME: this is only used twice I think, better pass this as an argument to
 *        the functions using it
 */
static char *argv0 = NULL;

#ifdef UNIX_COMPILE
#include "archdep_unix.c"
#endif

#ifdef WIN32_COMPILE
#include "archdep_win32.c"
#endif


/** \brief  Get the program name
 *
 * This returns the final part of argv[0], as if basename were used.
 *
 * \return  program name, heap-allocated, free with lib_free()
 */
char *archdep_program_name(void)
{
    return lib_stralloc(g_path_get_basename(argv0));

}


/** \brief  Get the absolute path to the VICE dir
 *
 * \return  Path to VICE's directory
 */
const char *archdep_boot_path(void)
{
    /* FIXME: this code is copied from unix/archdep.c because the code below
     * crashes with "g_path_get_dirname: assertion 'file_name != NULL' failed"
     * when the program file is not in the PATH */
    static char *boot;

    if (boot == NULL) {
        boot = findpath(argv0, getenv("PATH"), IOUTIL_ACCESS_X_OK);

        /* Remove the program name.  */
        *strrchr(boot, '/') = '\0';
    }

    return boot;
#if 0
    const char *boot;
    char *prg_name = archdep_program_name();

    boot = g_path_get_dirname(g_find_program_in_path(prg_name));
    lib_free(prg_name);
    return boot;
#endif
}


/** \brief  Get the user's home directory
 *
 * \return  current user's home directory
 */
const char *archdep_home_path(void)
{
    return g_get_home_dir();
}



char *archdep_user_config_path(void)
{
    char *path;
    gchar *tmp = g_build_path(path_separator, g_get_user_config_dir(),
            VICEUSERDIR, NULL);
    path = lib_stralloc(tmp);
    g_free(tmp);
    return path;
}

/** \brief  Determine if \a path is an absolute path
 *
 * \param[in]   path    some path
 *
 * \return  bool
 */
int archdep_path_is_relative(const char *path)
{
    return !g_path_is_absolute(path);
}


/** \brief  Quote \a name for use as a parameter in exec() etc calls
 *
 * Surounds \a name with double-quotes and replaces brackets with escaped
 * versions on Windows, on Unix it simply returns a heap-allocated copy.
 * Still leaves the OSX unzip bug. (See bug #920)
 *
 * \param[in]   name    string to quote
 *
 * \return  quoted string
 */
char *archdep_quote_parameter(const char *name)
{
#ifdef WIN32_COMPILE
    char *a,*b,*c;

    a = util_subst(name, "[", "\\[");
    b = util_subst(a, "]", "\\]");
    c = util_concat("\"", b, "\"", NULL);
    lib_free(a);
    lib_free(b);
    return c;
#else
    return lib_stralloc(name);
#endif
}


/** \brief  Quote \a name with double quotes
 *
 * Taken from win32/archdep.c, seems Windows needs this, but it makes unzip etc
 * fail on proper systems.
 *
 * \param[in]   name    string to quote
 *
 * \return  quoted (win32 only) and heap-allocated copy of \a name
 */
char *archdep_filename_parameter(const char *name)
{
#ifdef WIN32_COMPILE
    char *path;
    char *result;

    archdep_expand_path(&path, name);
    result = util_concat("\"", path, "\"", NULL);
    lib_free(path);
    return result;
#else
    return lib_stralloc(name);
#endif
}


/** \brief  Generate path to the default fliplist file
 *
 * On Unix, this will return "$HOME/.config/vice/fliplist-$machine.vfl", on
 * Windows this should return "%APPDATA%\\vice\\fliplist-$machine.vfl".
 *
 * \return  path to defaul fliplist file, must be freed with lib_free()
 */
char *archdep_default_fliplist_file_name(void)
{
    gchar *path;
    char *name;
    char *tmp;

    name = util_concat("fliplist-", machine_get_name(), ".vfl", NULL);
    path = g_build_path(path_separator, g_get_user_config_dir(), VICEUSERDIR,
            name, NULL);
    lib_free(name);
    /* transfer ownership of path to VICE */
    tmp = lib_stralloc(path);
    g_free(path);
    return tmp;
}


/** \brief  Create path(s) used by VICE for user-data
 *
 * \return  0 on success, -1 on failure
 */
static void archdep_create_user_config_dir(void)
{
    char *path = archdep_user_config_path();

    /* create config dir, fail silently if it exists
     * XXX: perhaps I should stat on failure to see if the directory already
     * existed, or there was another failure */
    (void)g_mkdir(path, 0755);
    lib_free(path);
}


/** \brief  Generate default autostart disk image path
 *
 * The path will be "$cfgdir/autostart-$emu.d64". this needs to be freed with
 * lib_free().
 *
 * \return  path to autostart disk image
 */
char *archdep_default_autostart_disk_image_file_name(void)
{
    char *cfg;
    gchar *path;
    char *name;
    char *tmp;

    cfg = archdep_user_config_path();
    name = util_concat(AUTOSTART_FILENAME_PREFIX, machine_get_name(),
            AUTOSTART_FILENAME_SUFFIX, NULL);
    path = g_build_path(path_separator, cfg, name, NULL);
    lib_free(name);
    lib_free(cfg);
    /* transfer ownership from non/glib to VICE */
    tmp = lib_stralloc(path);
    g_free(path);
    return tmp;
}


/** \brief  Generate path to vice.ini
 *
 * The value returned needs to be freed using lib_free()
 *
 * \return  absolute path to vice.ini
 */
char *archdep_default_resource_file_name(void)
{
    char *cfg;
    gchar *tmp;
    char *path;

    cfg = archdep_user_config_path();
    tmp = g_build_path(path_separator, cfg, "vice.ini", NULL);
    /* transfer ownership to VICE */
    path = lib_stralloc(tmp);
    g_free(tmp);
    lib_free(cfg);
    return path;
}



/** \brief  Open the default log file
 *
 * XXX: For now, this returns stdout, until I figure out why MacOSX duplicates
 *      fd 0 (stdin)
 */
FILE *archdep_open_default_log_file(void)
{
    INCOMPLETE_IMPLEMENTATION();
    return stdout;
}

void archdep_signals_init(int do_core_dumps)
{
    /* NOP: Gtk3 should handle any signals, I think */
}


/** \brief  Sanitize \a name by removing invalid characters for the current OS
 *
 * \param[in,out]   name    0-terminated string
 */
void archdep_sanitize_filename(char *name)
{
    while (*name != '\0') {
        int i = 0;
        while (illegal_name_tokens[i] != '\0') {
            if (illegal_name_tokens[i] == *name) {
                *name = '_';
                break;
            }
            i++;
        }
        name++;
    }
}


/** \brief  Create and open temp file
 *
 * \param[in]   filename    pointer to object to store name of temp file
 * \param[in]   mode        mode to open file with (see fopen(3))
 *
 * \return  pointer to new file or `NULL` on error
 */
FILE *archdep_mkstemp_fd(char **filename, const char *mode)
{
    GError *err;
    /* this function already uses the OS's tmp dir as a prefix, so no need to
     * do stuff like getenv("TMP")
     */
    int fd = g_file_open_tmp("vice.XXXXXX", filename, &err);
    if (fd < 0) {
        return NULL;
    }
    return fdopen(fd, mode);
}


/** \brief  Create directory \a pathname
 *
 * \param[in]   pathname    path/name of new directory
 * \param[in]   mode        ignored
 *
 * \return  0 on success, -1 on failure
 */
int archdep_mkdir(const char *pathname, int mode)
{
    return g_mkdir(pathname, mode); /* mode is ignored on Windows */
}


/** \brief  Rename \a oldpath to \a newpath
 *
 * \param[in]   oldpath old path
 * \param[in]   newpath new path
 *
 * \return  0 on success, -1 on failure
 */
int archdep_rename(const char *oldpath, const char *newpath)
{
    return g_rename(oldpath, newpath);
}



/** \brief  Arch-dependent init
 *
 * \param[in]   argc    pointer to argument count
 * \param[in]   argv    argument vector
 *
 * \return  0
 */
int archdep_init(int *argc, char **argv)
{
    char *prg_name;
    char *cfg_path;
    char *searchpath;
    char *vice_ini;

    argv0 = lib_stralloc(argv[0]);

    archdep_create_user_config_dir();

    /* sanity checks, to remove later: */
    prg_name = archdep_program_name();
    searchpath = archdep_default_sysfile_pathlist(machine_name);
    cfg_path = archdep_user_config_path();
    vice_ini = archdep_default_resource_file_name();

    printf("program name    = \"%s\"\n", prg_name);
    printf("user home dir   = \"%s\"\n", archdep_home_path());
    printf("user config dir = \"%s\"\n", cfg_path);
    printf("prg boot path   = \"%s\"\n", archdep_boot_path());
    printf("VICE searchpath = \"%s\"\n", searchpath);
    printf("vice.ini path   = \"%s\"\n", vice_ini);

    lib_free(prg_name);
    lib_free(searchpath);
    lib_free(vice_ini);
    lib_free(cfg_path);

    /* needed for early log control (parses for -silent/-verbose) */
    log_verbose_init(*argc, argv);

    return 0;
}

char *archdep_extra_title_text(void)
{
    return NULL;
}
