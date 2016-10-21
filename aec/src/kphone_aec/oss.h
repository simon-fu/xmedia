/* oss.h
 *
 * Copyright (C) DFS Deutsche Flugsicherung (2004, 2005). 
 * All Rights Reserved.
 *
 * Open Sound System
 *
 * Version 0.3
 */
#ifndef _OSS_H

/* Design Constants */
#define AUDIOBUFS   3           // Soundcard buffers (minimum 2)
/* End of Design Constants */

#define FORMAT_OSS  AFMT_S16_LE

/* Using fragment sizes shorter than 256 bytes is not recommended as the
 * default mode of application
 */
/* for Packettime = 40ms FRAGSIZELD = 7 can be used
 * for Packettime = 80ms FRAGSIZELD = 8 can be used
 */
#define FRAGSIZELD  (5+WIDEB)   // WIDEB is 1 or 2
#define FRAGSIZE    (1<<FRAGSIZELD)
#define FRAGTIME    (FRAGSIZE/(WIDEB*16))

const int RATE=WIDEB*8000;

int audio_init(char *pathname, int channels_, int mode);

#define _OSS_H
#endif
