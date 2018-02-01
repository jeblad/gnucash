/********************************************************************\
 * gnc-filepath-utils.c -- file path resolutin utilitie             *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
\********************************************************************/

/*
 * @file gnc-filepath-utils.c
 * @brief file path resolution utilities
 * @author Copyright (c) 1998-2004 Linas Vepstas <linas@linas.org>
 * @author Copyright (c) 2000 Dave Peticolas
 */

extern "C" {
#include <config.h>

#include <platform.h>
#if PLATFORM(WINDOWS)
#include <windows.h>
#include <Shlobj.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <errno.h>

#include "gnc-path.h"
#include "gnc-filepath-utils.h"

#if defined (_MSC_VER) || defined (G_OS_WIN32)
#include <glib/gwin32.h>
#ifndef PATH_MAX
#define PATH_MAX MAXPATHLEN
#endif
#endif
#ifdef MAC_INTEGRATION
#include <Foundation/Foundation.h>
#endif
}

#include <boost/filesystem.hpp>

#if PLATFORM(WINDOWS)
#include <codecvt>
#include <locale>
#endif

namespace bfs = boost::filesystem;
namespace bst = boost::system;

/**
 * Scrubs a filename by changing "strange" chars (e.g. those that are not
 * valid in a win32 file name) to "_".
 *
 * @param filename File name - updated in place
 */
static void
scrub_filename(char* filename)
{
    char* p;

#define STRANGE_CHARS "/:"
    p = strpbrk(filename, STRANGE_CHARS);
    while (p)
    {
        *p = '_';
        p = strpbrk(filename, STRANGE_CHARS);
    }
}

/** Check if the path exists and is a regular file.
 *
 * \param path -- freed if the path doesn't exist or isn't a regular file
 *
 *  \return NULL or the path
 */
static gchar *
check_path_return_if_valid(gchar *path)
{
    if (g_file_test(path, G_FILE_TEST_IS_REGULAR))
    {
        return path;
    }
    g_free (path);
    return NULL;
}

/** @brief Create an absolute path when given a relative path;
 *  otherwise return the argument.
 *
 *  @warning filefrag should be a simple path fragment. It shouldn't
 *  contain xml:// or http:// or <whatever>:// other protocol specifiers.
 *
 *  If passed a string which g_path_is_absolute declares an absolute
 *  path, return the argument.
 *
 *  Otherwise, assume that filefrag is a well-formed relative path and
 *  try to find a file with its path relative to
 *  \li the current working directory,
 *  \li the installed system-wide data directory (e.g., /usr/local/share/gnucash),
 *  \li the installed system configuration directory (e.g., /usr/local/etc/gnucash),
 *  \li or in the user's configuration directory (e.g., $HOME/.gnucash/data)
 *
 *  The paths are searched for in that order. If a matching file is
 *  found, return the absolute path to it.

 *  If one isn't found, return a absolute path relative to the
 *  user's configuration directory and note in the trace file that it
 *  needs to be created.
 *
 *  @param filefrag The file path to resolve
 *
 *  @return An absolute file path.
 */
gchar *
gnc_resolve_file_path (const gchar * filefrag)
{
    gchar *fullpath = NULL, *tmp_path = NULL;

    /* seriously invalid */
    if (!filefrag)
    {
        g_critical("filefrag is NULL");
        return NULL;
    }

    /* ---------------------------------------------------- */
    /* OK, now we try to find or build an absolute file path */

    /* check for an absolute file path */
    if (g_path_is_absolute(filefrag))
        return g_strdup (filefrag);

    /* Look in the current working directory */
    tmp_path = g_get_current_dir();
    fullpath = g_build_filename(tmp_path, filefrag, (gchar *)NULL);
    g_free(tmp_path);
    fullpath = check_path_return_if_valid(fullpath);
    if (fullpath != NULL)
        return fullpath;

    /* Look in the data dir (e.g. $PREFIX/share/gnucash) */
    tmp_path = gnc_path_get_pkgdatadir();
    fullpath = g_build_filename(tmp_path, filefrag, (gchar *)NULL);
    g_free(tmp_path);
    fullpath = check_path_return_if_valid(fullpath);
    if (fullpath != NULL)
        return fullpath;

    /* Look in the config dir (e.g. $PREFIX/share/gnucash/accounts) */
    tmp_path = gnc_path_get_accountsdir();
    fullpath = g_build_filename(tmp_path, filefrag, (gchar *)NULL);
    g_free(tmp_path);
    fullpath = check_path_return_if_valid(fullpath);
    if (fullpath != NULL)
        return fullpath;

    /* Look in the users config dir (e.g. $HOME/.gnucash/data) */
    fullpath = g_strdup(gnc_build_data_path(filefrag));
    if (g_file_test(fullpath, G_FILE_TEST_IS_REGULAR))
        return fullpath;

    /* OK, it's not there. Note that it needs to be created and pass it
     * back anyway */
    g_warning("create new file %s", fullpath);
    return fullpath;

}

gchar *gnc_file_path_relative_part (const gchar *prefix, const gchar *path)
{
    std::string p{path};
    if (p.find(prefix) == 0)
        return g_strdup(p.substr(strlen(prefix)).c_str());
    return g_strdup(path);
}

/* Searches for a file fragment paths set via GNC_DOC_PATH environment
 * variable. If this variable is not set, fall back to search in
 * - a html directory in the local user's gnucash settings directory
 *   (typically $HOME/.gnucash/html)
 * - the gnucash documentation directory
 *   (typically /usr/share/doc/gnucash)
 * - the gnucash data directory
 *   (typically /usr/share/gnucash)
 * It searches in this order.
 *
 * This is used by gnc_path_find_localized_file to search for
 * localized versions of files if they exist.
 */
static gchar *
gnc_path_find_localized_html_file_internal (const gchar * file_name)
{
    gchar *full_path = NULL;
    int i;
    const gchar *env_doc_path = g_getenv("GNC_DOC_PATH");
    const gchar *default_dirs[] =
        {
            gnc_build_userdata_path ("html"),
            gnc_path_get_pkgdocdir (),
            gnc_path_get_pkgdatadir (),
            NULL
        };
    gchar **dirs;

    if (!file_name || *file_name == '\0')
        return NULL;

    /* Allow search path override via GNC_DOC_PATH environment variable */
    if (env_doc_path)
        dirs = g_strsplit (env_doc_path, G_SEARCHPATH_SEPARATOR_S, -1);
    else
        dirs = (gchar **)default_dirs;

    for (i = 0; dirs[i]; i++)
    {
        full_path = g_build_filename (dirs[i], file_name, (gchar *)NULL);
        g_debug ("Checking for existence of %s", full_path);
        full_path = check_path_return_if_valid (full_path);
        if (full_path != NULL)
            return full_path;
    }

    return NULL;
}

/** @brief Find an absolute path to a localized version of a given
 *  relative path to a html or html related file.
 *  If no localized version exists, an absolute path to the file
 *  is searched for. If that file doesn't exist either, returns NULL.
 *
 *  @warning file_name should be a simple path fragment. It shouldn't
 *  contain xml:// or http:// or <whatever>:// other protocol specifiers.
 *
 *  If passed a string which g_path_is_absolute declares an absolute
 *  path, return the argument.
 *
 *  Otherwise, assume that file_name is a well-formed relative path and
 *  try to find a file with its path relative to
 *  \li a localized subdirectory in the html directory
 *      of the user's configuration directory
 *      (e.g. $HOME/.gnucash/html/de_DE, $HOME/.gnucash/html/en,...)
 *  \li a localized subdirectory in the gnucash documentation directory
 *      (e.g. /usr/share/doc/gnucash/C,...)
 *  \li the html directory of the user's configuration directory
 *      (e.g. $HOME/.gnucash/html)
 *  \li the gnucash documentation directory
 *      (e.g. /usr/share/doc/gnucash/)
 *  \li last resort option: the gnucash data directory
 *      (e.g. /usr/share/gnucash/)
 *
 *  The paths are searched for in that order. If a matching file is
 *  found, return the absolute path to it.

 *  If one isn't found, return NULL.
 *
 *  @param file_name The file path to resolve
 *
 *  @return An absolute file path or NULL if no file is found.
 */
gchar *
gnc_path_find_localized_html_file (const gchar *file_name)
{
    gchar *loc_file_name = NULL;
    gchar *full_path = NULL;
    const gchar * const *lang;

    if (!file_name || *file_name == '\0')
        return NULL;

    /* An absolute path is returned unmodified. */
    if (g_path_is_absolute (file_name))
        return g_strdup (file_name);

    /* First try to find the file in any of the localized directories
     * the user has set up on his system
     */
    for (lang = g_get_language_names (); *lang; lang++)
    {
        loc_file_name = g_build_filename (*lang, file_name, (gchar *)NULL);
        full_path = gnc_path_find_localized_html_file_internal (loc_file_name);
        g_free (loc_file_name);
        if (full_path != NULL)
            return full_path;
    }

    /* If not found in a localized directory, try to find the file
     * in any of the base directories
     */
    return gnc_path_find_localized_html_file_internal (file_name);

}

/* ====================================================================== */
static auto gnc_userdata_home = bfs::path();
static auto build_dir = bfs::path();

static bool dir_is_descendant (const bfs::path& path, const bfs::path& base)
{
    auto test_path = path;
    if (bfs::exists (path))
        test_path = bfs::canonical (path);
    auto test_base = base;
    if (bfs::exists (base))
        test_base = bfs::canonical (base);

    auto is_descendant = (test_path.string() == test_base.string());
    while (!test_path.empty() && !is_descendant)
    {
        test_path = test_path.parent_path();
        is_descendant = (test_path.string() == test_base.string());
    }
    return is_descendant;
}

/** @brief Check that the supplied directory path exists, is a directory, and
 * that the user has adequate permissions to use it.
 *
 * @param dirname The path to check
 */
static bool
gnc_validate_directory (const bfs::path &dirname)
{
    if (dirname.empty())
        return false;

    auto create_dirs = true;
    if (build_dir.empty() || !dir_is_descendant (dirname, build_dir))
    {
        /* Gnucash won't create a home directory
         * if it doesn't exist yet. So if the directory to create
         * is a descendant of the homedir, we can't create it either.
         * This check is conditioned on do_homedir_check because
         * we need to overrule it during build (when guile interferes)
         * and testing.
         */
        auto home_dir = bfs::path (g_get_home_dir ());
        auto homedir_exists = bfs::exists(home_dir);
        auto is_descendant = dir_is_descendant (dirname, home_dir);
        if (!homedir_exists && is_descendant)
            create_dirs = false;
    }

    /* Create directories if they don't exist yet and we can
     *
     * Note this will do nothing if the directory and its
     * parents already exist, but will fail if the path
     * points to a file or a softlink. So it serves as a test
     * for that as well.
     */
    if (create_dirs)
        bfs::create_directories(dirname);
    else
        throw (bfs::filesystem_error (
            std::string (dirname.string() +
                         " is a descendant of a non-existing home directory. As " +
                         PACKAGE_NAME +
                         " will never create a home directory this path can't be used"),
            dirname, bst::error_code(bst::errc::permission_denied, bst::generic_category())));

    auto d = bfs::directory_entry (dirname);
    auto perms = d.status().permissions();

    /* On Windows only write permission will be checked.
     * So strictly speaking we'd need two error messages here depending
     * on the platform. For simplicity this detail is glossed over though. */
#if PLATFORM(WINDOWS)
    auto check_perms = bfs::owner_read | bfs::owner_write;
#else
    auto check_perms = bfs::owner_all;
#endif
    if ((perms & check_perms) != check_perms)
        throw (bfs::filesystem_error(
            std::string("Insufficient permissions, at least write and access permissions required: ")
            + dirname.string(), dirname,
            bst::error_code(bst::errc::permission_denied, bst::generic_category())));

    return true;
}

/* Will attempt to copy all files and directories from src to dest
 * Returns true if successful or false if not */
static bool
copy_recursive(const bfs::path& src, const bfs::path& dest)
{
    if (!bfs::exists(src))
        return false;

    // Don't copy on self
    if (src.compare(dest) == 0)
        return false;

    auto old_str = src.string();
    auto old_len = old_str.size();
    try
    {
        /* Note: the for(auto elem : iterator) construct fails
         * on travis (g++ 4.8.x, boost 1.54) so I'm using
         * a traditional for loop here */
        for(auto direntry = bfs::recursive_directory_iterator(src);
            direntry != bfs::recursive_directory_iterator(); ++direntry)
        {
            auto cur_str = direntry->path().string();
            auto cur_len = cur_str.size();
            auto rel_str = std::string(cur_str, old_len, cur_len - old_len);
            auto relpath = bfs::path(rel_str).relative_path();
            auto newpath = bfs::absolute (relpath, dest);
            bfs::copy(direntry->path(), newpath);
        }
    }
    catch(const bfs::filesystem_error& ex)
    {
        g_warning("An error occured while trying to migrate the user configation from\n%s to\n%s"
                  "(Error: %s)",
                  src.string().c_str(), gnc_userdata_home.string().c_str(),
                  ex.what());
        return false;
    }

    return true;
}

#ifdef G_OS_WIN32
/* g_get_user_data_dir defaults to CSIDL_LOCAL_APPDATA, but
 * the roaming profile makes more sense.
 * So this function is a copy of glib's internal get_special_folder
 * and minimally adjusted to fetch CSIDL_APPDATA
 */
static char *
win32_get_userdata_home (void)
{
    wchar_t path[MAX_PATH+1];
    HRESULT hr;
    LPITEMIDLIST pidl = NULL;
    BOOL b;
    char *retval = NULL;

    hr = SHGetSpecialFolderLocation (NULL, CSIDL_APPDATA, &pidl);
    if (hr == S_OK)
    {
        b = SHGetPathFromIDListW (pidl, path);
        if (b)
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
            retval = g_strdup(utf8_conv.to_bytes(path).c_str());
        }
        CoTaskMemFree (pidl);
    }
    return retval;
}
#elif defined MAC_INTEGRATION
static char*
quarz_get_userdata_home(void)
{
    char *retval = NULL;
    NSFileManager*fm = [NSFileManager defaultManager];
    NSArray* appSupportDir = [fm URLsForDirectory:NSApplicationSupportDirectory
    inDomains:NSUserDomainMask];
    if ([appSupportDir count] > 0)
    {
        NSURL* dirUrl = [appSupportDir objectAtIndex:0];
        NSString* dirPath = [dirUrl path];
        retval = g_strdup([dirPath UTF8String]);
    }
    return retval;
}
#endif

static bfs::path
get_userdata_home(void)
{
    auto try_tmp_dir = true;
    gchar *data_dir = NULL;
    auto userdata_home = bfs::path();

#ifdef G_OS_WIN32
    data_dir = win32_get_userdata_home ();
#elif defined MAC_INTEGRATION
    data_dir = quarz_get_userdata_home ();
#endif

    if (data_dir)
    {
        userdata_home = data_dir;
        g_free(data_dir);
    }
    else
        userdata_home = g_get_user_data_dir();

    /* g_get_user_data_dir doesn't check whether the path exists nor attempts to
     * create it. So while it may return an actual path we may not be able to use it.
     * Let's check that now */
    if (!userdata_home.empty())
    {
        try
        {
            gnc_validate_directory(userdata_home);  // May throw
            try_tmp_dir = false;
        }
        catch (const bfs::filesystem_error& ex)
        {
            auto path_string = userdata_home.string();
            g_warning("%s is not a suitable base directory for the user data. "
            "Trying temporary directory instead.\n(Error: %s)",
            path_string.c_str(), ex.what());
        }
    }

    /* The path we got is not usable, so fall back to a path in TMP_DIR.
       Hopefully we can always write there. */
    if (try_tmp_dir)
    {
        userdata_home = bfs::path (g_get_tmp_dir ()) / g_get_user_name ();
    }
    g_assert(!userdata_home.empty());

    return userdata_home;
}

static bfs::path
get_userconfig_home(void)
{
    gchar *config_dir = NULL;
    auto userconfig_home = bfs::path();

#ifdef G_OS_WIN32
    config_dir = win32_get_userdata_home ();
#elif defined MAC_INTEGRATION
    config_dir = quarz_get_userdata_home ();
#endif

    /* On Windows and Macs the data directory is used, for Linux
       $HOME/.config is used */
    if (config_dir)
    {
        userconfig_home = config_dir;
        g_free(config_dir);
    }
    else
        userconfig_home = g_get_user_config_dir();

    userconfig_home = userconfig_home / PACKAGE;

    return userconfig_home;
}

gboolean
gnc_filepath_init (void)
{
    auto gnc_userdata_home_exists = false;
    auto have_valid_userdata_home = false;

    /* If this code is run while building/testing, use a fake GNC_DATA_HOME
     * in the base of the build directory. This is to deal with all kinds of
     * issues when the build environment is not a complete environment (like
     * it could be missing a valid home directory). */
    auto env_build_dir = g_getenv ("GNC_BUILDDIR");
    build_dir = bfs::path(env_build_dir ? env_build_dir : "");
    auto running_uninstalled = (g_getenv ("GNC_UNINSTALLED") != NULL);
    if (running_uninstalled && !build_dir.empty())
    {
        gnc_userdata_home = build_dir / "gnc_data_home";
        try
        {
            gnc_validate_directory (gnc_userdata_home); // May throw
            have_valid_userdata_home = true;
            gnc_userdata_home_exists = true; // To prevent possible migration further down
        }
        catch (const bfs::filesystem_error& ex)
        {
            auto path_string = gnc_userdata_home.string();
            g_warning("%s (due to run during at build time) is not a suitable directory for user data. "
            "Trying another directory instead.\n(Error: %s)",
                      path_string.c_str(), ex.what());
        }
    }

    if (!have_valid_userdata_home)
    {
        /* If environment variable GNC_DATA_HOME is set, try whether
         * it points at a valid directory. */
        auto gnc_userdata_home_env = g_getenv ("GNC_DATA_HOME");
        if (gnc_userdata_home_env)
        {
            gnc_userdata_home = bfs::path (gnc_userdata_home_env);
            try
            {
                gnc_userdata_home_exists = bfs::exists (gnc_userdata_home);
                gnc_validate_directory (gnc_userdata_home); // May throw
                have_valid_userdata_home = true;
            }
            catch (const bfs::filesystem_error& ex)
            {
                auto path_string = gnc_userdata_home.string();
                g_warning("%s (from environment variable 'GNC_DATA_HOME') is not a suitable directory for user data. "
                "Trying the default instead.\n(Error: %s)",
                path_string.c_str(), ex.what());
            }
        }
    }

    if (!have_valid_userdata_home)
    {
        /* Determine platform dependent default userdata_home_path
         * and check whether it's valid */
        auto userdata_home = get_userdata_home();
        gnc_userdata_home = userdata_home / PACKAGE;
        try
        {
            gnc_userdata_home_exists = bfs::exists (gnc_userdata_home);
            gnc_validate_directory (gnc_userdata_home);
        }
        catch (const bfs::filesystem_error& ex)
        {
            g_warning ("User data directory doesn't exist, yet could not be created. Proceed with caution.\n"
            "(Error: %s)", ex.what());
        }
    }

    auto migrated = FALSE;
    if (!gnc_userdata_home_exists)
        migrated = copy_recursive (bfs::path (g_get_home_dir()) / ".gnucash",
                                   gnc_userdata_home);

    /* Try to create the standard subdirectories for gnucash' user data */
    try
    {
        gnc_validate_directory (gnc_userdata_home / "books");
        gnc_validate_directory (gnc_userdata_home / "checks");
        gnc_validate_directory (gnc_userdata_home / "translog");
    }
    catch (const bfs::filesystem_error& ex)
    {
        g_warning ("Default user data subdirectories don't exist, yet could not be created. Proceed with caution.\n"
        "(Error: %s)", ex.what());
    }

    return migrated;
}

/** @fn const gchar * gnc_userdata_dir ()
 *  @brief Ensure that the user's configuration directory exists and is minimally populated.
 *
 *  Note that the default path depends on the platform.
 *  - Windows: CSIDL_APPDATA/Gnucash
 *  - OS X: $HOME/Application Support/Gnucash
 *  - Linux: $XDG_DATA_HOME/Gnucash (or the default $HOME/.local/share/Gnucash)
 *
 *  If any of these paths doesn't exist and can't be created the function will
 *  fall back to
 *  - $HOME/.gnucash
 *  - <TMP_DIR>/Gnucash
 *  (in that order)
 *
 *  @return An absolute path to the configuration directory. This string is
 *  owned by the gnc_filepath_utils code and should not be freed by the user.
 */


/* Note Don't create missing directories automatically
 * here and in the next function except if the
 * target directory is the temporary directory. This
 * should be done properly at a higher level (in the gui
 * code most likely) very early in application startup.
 * This call is just a fallback to prevent the code from
 * crashing because no directories were configured. This
 * weird construct is set up because compiling our guile
 * scripts also triggers this code and that's not the
 * right moment to start creating the necessary directories.
 * FIXME A better approach would be to have the gnc_userdata_home
 * verification/creation be part of the application code instead
 * of libgnucash. If libgnucash needs access to this directory
 * libgnucash will need some kind of initialization routine
 * that the application can call to set (among others) the proper
 * gnc_uderdata_home for libgnucash. The only other aspect to
 * consider here is how to handle this in the bindings (if they
 * need it).
 */
const gchar *
gnc_userdata_dir (void)
{
    if (gnc_userdata_home.empty())
        gnc_filepath_init();

    return gnc_userdata_home.string().c_str();
}

/** @fn const gchar * gnc_userconfig_dir ()
 *  @brief Return the config directory
 *
 *  Note that the default path depends on the platform.
 *  - Windows: CSIDL_APPDATA/Gnucash
 *  - OS X: $HOME/Application Support/Gnucash
 *  - Linux: $XDG_CONFIG_HOME/Gnucash (or the default $HOME/.config/Gnucash)
 *
 *  @return An absolute path to the configuration directory. This string is
 *  owned by the gnc_filepath_utils code and should not be freed by the user.
 */
const gchar *
gnc_userconfig_dir (void)
{
    auto path_string = get_userconfig_home();
    return g_strdup(path_string.string().c_str());
}

static const bfs::path&
gnc_userdata_dir_as_path (void)
{
    if (gnc_userdata_home.empty())
        /* Don't create missing directories automatically except
         * if the target directory is the temporary directory. This
         * should be done properly at a higher level (in the gui
         * code most likely) very early in application startup.
         * This call is just a fallback to prevent the code from
         * crashing because no directories were configured. */
        gnc_filepath_init();

    return gnc_userdata_home;
}

/** @fn gchar * gnc_build_userdata_path (const gchar *filename)
 *  @brief Make a path to filename in the user's configuration directory.
 *
 * @param filename The name of the file
 *
 *  @return An absolute path. The returned string should be freed by the user
 *  using g_free().
 */

gchar *
gnc_build_userdata_path (const gchar *filename)
{
    return g_strdup((gnc_userdata_dir_as_path() / filename).string().c_str());
}

static bfs::path
gnc_build_userdata_subdir_path (const gchar *subdir, const gchar *filename)
{
    gchar* filename_dup = g_strdup(filename);

    scrub_filename(filename_dup);
    auto result = (gnc_userdata_dir_as_path() / subdir) / filename_dup;
    g_free(filename_dup);
    return result;
}

/** @fn gchar * gnc_build_book_path (const gchar *filename)
 *  @brief Make a path to filename in the book subdirectory of the user's configuration directory.
 *
 * @param filename The name of the file
 *
 *  @return An absolute path. The returned string should be freed by the user
 *  using g_free().
 */

gchar *
gnc_build_book_path (const gchar *filename)
{
    auto path = gnc_build_userdata_subdir_path("books", filename).string();
    return g_strdup(path.c_str());
}

/** @fn gchar * gnc_build_translog_path (const gchar *filename)
 *  @brief Make a path to filename in the translog subdirectory of the user's configuration directory.
 *
 * @param filename The name of the file
 *
 *  @return An absolute path. The returned string should be freed by the user
 *  using g_free().
 */

gchar *
gnc_build_translog_path (const gchar *filename)
{
    auto path = gnc_build_userdata_subdir_path("translog", filename).string();
    return g_strdup(path.c_str());
}

/** @fn gchar * gnc_build_data_path (const gchar *filename)
 *  @brief Make a path to filename in the data subdirectory of the user's configuration directory.
 *
 * @param filename The name of the file
 *
 *  @return An absolute path. The returned string should be freed by the user
 *  using g_free().
 */

gchar *
gnc_build_data_path (const gchar *filename)
{
    auto path = gnc_build_userdata_subdir_path("data", filename).string();
    return g_strdup(path.c_str());
}

/** @fn gchar * gnc_build_report_path (const gchar *filename)
 *  @brief Make a path to filename in the report directory.
 *
 * @param filename The name of the file
 *
 *  @return An absolute path. The returned string should be freed by the user
 *  using g_free().
 */

gchar *
gnc_build_report_path (const gchar *filename)
{
    gchar *result = g_build_filename(gnc_path_get_reportdir(), filename, (gchar *)NULL);
    return result;
}

/** @fn gchar * gnc_build_stdreports_path (const gchar *filename)
 *  @brief Make a path to filename in the standard reports directory.
 *
 * @param filename The name of the file
 *
 *  @return An absolute path. The returned string should be freed by the user
 *  using g_free().
 */

gchar *
gnc_build_stdreports_path (const gchar *filename)
{
    gchar *result = g_build_filename(gnc_path_get_stdreportsdir(), filename, (gchar *)NULL);
    return result;
}

static gchar *
gnc_filepath_locate_file (const gchar *default_path, const gchar *name)
{
    gchar *fullname;

    g_return_val_if_fail (name != NULL, NULL);

    if (g_path_is_absolute (name))
        fullname = g_strdup (name);
    else if (default_path)
        fullname = g_build_filename (default_path, name, NULL);
    else
        fullname = gnc_resolve_file_path (name);

    if (!g_file_test (fullname, G_FILE_TEST_IS_REGULAR))
    {
        g_warning ("Could not locate file %s", name);
        g_free (fullname);
        return NULL;
    }

    return fullname;
}

gchar *
gnc_filepath_locate_data_file (const gchar *name)
{
    return gnc_filepath_locate_file (gnc_path_get_pkgdatadir(), name);
}

gchar *
gnc_filepath_locate_pixmap (const gchar *name)
{
    gchar *default_path;
    gchar *fullname;
    gchar* pkgdatadir = gnc_path_get_pkgdatadir ();

    default_path = g_build_filename (pkgdatadir, "pixmaps", NULL);
    g_free(pkgdatadir);
    fullname = gnc_filepath_locate_file (default_path, name);
    g_free(default_path);

    return fullname;
}

gchar *
gnc_filepath_locate_ui_file (const gchar *name)
{
    gchar *default_path;
    gchar *fullname;
    gchar* pkgdatadir = gnc_path_get_pkgdatadir ();

    default_path = g_build_filename (pkgdatadir, "ui", NULL);
    g_free(pkgdatadir);
    fullname = gnc_filepath_locate_file (default_path, name);
    g_free(default_path);

    return fullname;
}

gchar *
gnc_filepath_locate_doc_file (const gchar *name)
{
    return gnc_filepath_locate_file (gnc_path_get_pkgdocdir(), name);
}


/* =============================== END OF FILE ========================== */
