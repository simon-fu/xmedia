/* intercomd.h
 *
 * Copyright (C) DFS Deutsche Flugsicherung (2004, 2005). 
 * All Rights Reserved.
 *
 * Voice over IP Intercom with Telephone conference and Acoustic Echo
 * Cancellation using unicast RTP messages (RFC3550)
 *
 * Version 0.2
 */
#ifndef _INTERCOMD_H

#include <syslog.h>

#define ERROR (-1)
#define OKAY 0

#define NO 0
#define YES 1

/* Emit program info and abort the program if expr is false with errno */
#define assert_errno(expr) \
if(!(expr)) { \
  syslog(LOG_WARNING, "%s:%d: %s: Assertion '%s' failed. errno=%s\n", \
  __FILE__, __LINE__, __PRETTY_FUNCTION__, __STRING(expr), strerror(errno)); \
  exit(1); \
}

/* Emit program info and return function if expr is true with retvalue */
#define return_if(expr, retvalue) \
if(expr) { \
  syslog(LOG_WARNING, "%s:%d: %s: Check '%s' failed.\n", \
  __FILE__, __LINE__, __PRETTY_FUNCTION__, __STRING(expr)); \
  return(retvalue); \
}

int print_gui(const char *fmt, ...);

#define _INTERCOMD_H
#endif
