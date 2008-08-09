/**
 * ${project-name} - a GNU/Linux console-based file manager
 *
 * Different stuff providing file-related operations
 *
 * Copyright 2008 Sergey I. Sharybin <nazgul@school9.perm.ru>
 * Copyright 2008 Alex A. Smirnov <sceptic13@gmail.com>
 *
 * This program can be distributed under the terms of the GNU GPL.
 * See the file COPYING.
 */

#ifndef _file_h_
#define _file_h_

#include "smartinclude.h"

BEGIN_HEADER

#include <sys/stat.h>
#include <time.h>

////////
// Constants

#define MAX_FILENAME_LEN  256

////////
// Type definitions

typedef struct {
  ////
  // Inherited from dirent
  wchar_t         name[MAX_FILENAME_LEN]; // Name of file
  unsigned char   type;                   // Type of file
  struct stat     stat;                   // Stat information of file
  struct stat     lstat;                  // L-Stat information of file
} file_t;

////////
//

void           // Fit filename to specified length
fit_filename                      (const wchar_t  *__file_name,
                                   long            __len,
                                   wchar_t        *__res);

wchar_t*       // Strip directory prefix from file name
wcfilename                        (const wchar_t *__name);

void           // Convert file mode creation mask to string
umasktowcs                        (mode_t __mask, wchar_t *__res);

#ifdef __USE_FILE_OFFSET64
__u64_t        // Convert file size to human-readable format
fsizetohuman                      (__u64_t __size, char *__suffix);
#else
__u32_t       // Convert file size to human-readable format
fsizetohuman                      (__u32_t __size, char *__suffix);
#endif

void
format_file_time                  (wchar_t *__buf,
                                   size_t   __buf_size,
                                   time_t   __time);

wchar_t*
filename_trim                     (const wchar_t *__fn);

int
filename_compare                  (const wchar_t *__a, const wchar_t *__b);

END_HEADER

#endif
