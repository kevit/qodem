/*
 * common.c
 *
 * This module is licensed under the GNU General Public License Version 2.
 * Please see the file "COPYING" in this directory for more information about
 * the GNU General Public License Version 2.
 *
 *     Copyright (C) 2015  Kevin Lamonte
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "qcurses.h"
#include "common.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef Q_PDCURSES_WIN32
#include <tchar.h>
#include <windows.h>
#include <shlobj.h>
#else
#include <sys/poll.h>
#include <unistd.h>
#endif /* Q_PDCURSES_WIN32 */

/**
 * wcsdup() equivalent that plugs into the Hans-Boehm GC if it is enabled, if
 * not it just passes through to the default wcsdup().
 *
 * @param ptr the string to copy
 * @param file the filename calling this function
 * @param line the line number at the call point.
 */
wchar_t * Xwcsdup(const wchar_t * ptr, const char * file, const int line) {

    wchar_t * local_ptr = NULL;
#if defined(QODEM_USE_GC) && defined(HAVE_LIBGC)
    int length;
    int i;
#endif

    assert(ptr != NULL);

#if defined(QODEM_USE_GC) && defined(HAVE_LIBGC)
    length = wcslen(ptr) + 1;
#ifdef DEBUG
    local_ptr =
        (wchar_t *) GC_debug_malloc(length * sizeof(wchar_t), file, line);
#else
    local_ptr = (wchar_t *) GC_malloc(length * sizeof(wchar_t));
#endif

    if (local_ptr != NULL) {
        for (i = 0; i < length; i++) {
            local_ptr[i] = ptr[i];
        }
    }
    local_ptr[length - 1] = 0;

#else

#if __linux

    local_ptr = wcsdup(ptr);

#else

    if (wcslen(ptr) > 0) {
        local_ptr =
            (wchar_t *) Xcalloc(1, (sizeof(wchar_t) + 1) * (wcslen(ptr) + 1),
                                __FILE__, __LINE__);
        wcsncpy(local_ptr, ptr, wcslen(ptr));
    } else {
        local_ptr =
            (wchar_t *) Xcalloc(1, (sizeof(wchar_t) + 1) * 1, __FILE__,
                                __LINE__);
    }

#endif

#endif

#if 0
    /* For debugging: */
    fprintf(stderr, "Xwcsdup: ptr=%p local_ptr=%p file=%s line=%d\n", ptr,
            local_ptr, file, line);
#endif

    return local_ptr;
}

/**
 * A narrow-char to wide-char string duplicate function that plugs into the
 * Hans-Boehm GC if it is enabled, if not it just passes through to the
 * default mbstowcs().
 *
 * @param ptr the string to copy
 * @param file the filename calling this function
 * @param line the line number at the call point.
 */
wchar_t * Xstring_to_wcsdup(const char * ptr, const char * file,
                            const int line) {

    wchar_t * local_ptr = NULL;
    int length;

#if defined(QODEM_USE_GC) && defined(HAVE_LIBGC)
    int i;
#endif

    assert(ptr != NULL);

    length = mbstowcs(NULL, ptr, strlen(ptr)) + 1;

#if defined(QODEM_USE_GC) && defined(HAVE_LIBGC)
#ifdef DEBUG
    local_ptr =
        (wchar_t *) GC_debug_malloc((sizeof(wchar_t)) * length, file, line);
#else
    local_ptr = (wchar_t *) GC_malloc((sizeof(wchar_t)) * length);
#endif

#else

    local_ptr = (wchar_t *) malloc((sizeof(wchar_t)) * length);

#endif

    if (length > 1) {
        mbstowcs(local_ptr, ptr, length - 1);
        local_ptr[length - 1] = 0;
    } else {
        if (length == 1) {
            local_ptr[0] = 0;
        } else {
            local_ptr = Xwcsdup(L"", file, line);
        }
    }

#if 0
    /* For debugging: */
    fprintf(stderr, "Xstring_to_wcsdup: ptr=%p local_ptr=%p file=%s line=%d\n",
            ptr, local_ptr, file, line);
#endif

    return local_ptr;
}

/**
 * strdup() equivalent that plugs into the Hans-Boehm GC if it is enabled, if
 * not it just passes through to the default strdup().
 *
 * @param ptr the string to copy
 * @param file the filename calling this function
 * @param line the line number at the call point.
 */
char * Xstrdup(const char * ptr, const char * file, const int line) {

    char * local_ptr = NULL;
#if defined(QODEM_USE_GC) && defined(HAVE_LIBGC)
    int length;
    int i;
#endif

    assert(ptr != NULL);

#if defined(QODEM_USE_GC) && defined(HAVE_LIBGC)
    length = strlen(ptr) + 1;
#ifdef DEBUG
    local_ptr = (char *) GC_debug_malloc(length, file, line);
#else
    local_ptr = (char *) GC_malloc(length);
#endif /* DEBUG */

    if (local_ptr != NULL) {
        for (i = 0; i < length; i++) {
            local_ptr[i] = ptr[i];
        }
    }

#else

    local_ptr = strdup(ptr);

#endif

#if 0
    /* For debugging: */
    fprintf(stderr, "Xstrdup: ptr=%p local_ptr=%p file=%s line=%d\n", ptr,
            local_ptr, file, line);
#endif

    return local_ptr;
}

#ifdef Q_PDCURSES_WIN32

/* Saved copy of path pointing to My Documents  */
static char * win32_docs_path = NULL;

#endif

/**
 * Returns the home directory where ~/.qodem (My Documents\\qodem\\prefs) and
 * ~/qodem (My Documents\\qodem) are stored.
 *
 * @return the path string pointing to qodem's home directory, usually
 * ~/.qodem on POSIX or My Documents\\qodem on Windows.
 */
char * get_home_directory() {
    char * env_string;

#ifdef Q_PDCURSES_WIN32

    TCHAR myDocsPath[MAX_PATH];

    if (win32_docs_path != NULL) {
        return win32_docs_path;
    }

#ifdef __BORLANDC__

    /*
     * We go through some work to make %USERPROFILE%\\My Documents because
     * swprintf() is weird when mixing char * and wchar_t * with Borland.
     */
    env_string = getenv("USERPROFILE");
    int i, j;

    memset(myDocsPath, 0, sizeof(myDocsPath));
    for (i = 0; i < strlen(env_string); i++) {
        myDocsPath[i] = env_string[i];
    }
    myDocsPath[i] = '\\';
    i++;
    wchar_t * myDocumentsString = _(L"My Documents");
    for (j = 0; j < wcslen(myDocumentsString); j++) {
        myDocsPath[i] = myDocumentsString[j];
        i++;
    }

    if (1) {

#else

    /*
     * Windows: try the CSIDL function, if that fails just return
     * %USERPROFILE% .
     */
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, 0, myDocsPath))) {

#endif /* __BORLANDC__ */

        if (sizeof(TCHAR) == sizeof(char)) {
            /*
             * Direct copy
             */
            win32_docs_path = Xstrdup((char *) myDocsPath, __FILE__, __LINE__);
        } else {
            /*
             * TCHAR is wchar_t, copy each byte.  This would be like a
             * Xwcs_to_strdup but no one else needs that function.
             */
            int i;
            win32_docs_path =
                (char *) Xmalloc(wcslen((wchar_t *) myDocsPath) + 1, __FILE__,
                                 __LINE__);
            memset(win32_docs_path, 0, wcslen((wchar_t *) myDocsPath) + 1);
            for (i = 0; i < wcslen((wchar_t *) myDocsPath); i++) {
                win32_docs_path[i] = myDocsPath[i];
            }
        }
        return win32_docs_path;
    } else {
        env_string = getenv("USERPROFILE");
    }

#else

    /*
     * Everyone else in the world: $HOME
     */
    env_string = getenv("HOME");

#endif /* Q_PDCURSES_WIN32 */

    return env_string;
}

#ifndef Q_PDCURSES_WIN32

/**
 * Function to clean out any characters waiting in stdin.
 */
void purge_stdin() {
    struct pollfd pfd;
    int rc = 0;

    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;

    rc = poll(&pfd, 1, 10);
    if (rc < 0) {
        /*
         * Error
         */
    } else if (rc > 0) {

        /*
         * Flush stdin
         */
        do {
            pfd.fd = STDIN_FILENO;
            pfd.events = POLLIN;
            getchar();
        } while ((rc = poll(&pfd, 1, 0)) > 0);
    }
}

#endif

/**
 * Returns Q_TRUE if the named file already exists.
 *
 * @param filename the filename to check
 * @return true if the file exists
 */
Q_BOOL file_exists(const char * filename) {
    int rc;
    struct stat fstats;
    rc = stat(filename, &fstats);
    if (rc < 0) {
        if (errno == ENOENT) {
            /*
             * File definitely does not exist.
             */
            return Q_FALSE;
        }
    }

    /*
     * For all I/O errors, or a successful stat(), assume that the file
     * exists.
     */
    return Q_TRUE;
}

/**
 * Returns Q_TRUE if path exists and is a directory.
 *
 * @param path the directory name to check
 * @return true if the directory exists
 */
Q_BOOL directory_exists(const char * path) {

#ifdef Q_PDCURSES_WIN32
    DWORD dwAttrib = GetFileAttributesA(path);

    if ((dwAttrib != 0xFFFFFFFF) && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
        return Q_TRUE;
    }
#else

    if (access(path, F_OK) == 0) {
        return Q_TRUE;
    }

#endif

    /*
     * Directory does not exist
     */
    return Q_FALSE;
}

#ifdef __BORLANDC__

/**
 * wmemmove() implementation to make up for Borland C++ not having it.
 * Wide-char equivalent of memmove().
 *
 * @param dest the destination location
 * @param src the source wide-chars to copy
 * @param n the number of wide-chars to copy
 * @return dest
 */
extern wchar_t * wmemmove(wchar_t * dest, const wchar_t * src, size_t n) {
    return (wchar_t *) memmove(dest, src, n * sizeof(wchar_t));
}

#endif
