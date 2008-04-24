/**
 * ${project-name} - a GNU/Linux console-based file manager
 *
 * Smart including of headers
 *
 * Copyright 2008 Sergey I. Sharybin <nazgul@school9.perm.ru>
 * Copyright 2008 Alex A. Smirnov <sceptic13@gmail.com>
 *
 * This program can be distributed under the terms of the GNU GPL.
 * See the file COPYING.
 */

#ifndef _smart_include_h_
#define _smart_include_h_

#include <config.h>

#include "build-stamp.h"
#include "package.h"
#include "version.h"

#include "types.h"
#include "macrodef.h"

// Some definitions to tell compiler use
// specified stuff
#define __USE_GNU
#define __USE_ISOC99
#define USE_WIDEC_SUPPORT
#define _XOPEN_SOURCE_EXTENDED

#ifdef HAVE__ATTRIBUTE__
#  define ATTR_UNUSED __attribute__((unused))
#else
#  define ATTR_UNUSED
#endif

#endif
