#ifndef SABINE_AEC_H
#define SABINE_AEC_H

#include "Platform.h"

#ifndef MEDIAEXPORT
#ifdef WIN32
#define MEDIAEXPORT
#else
#define MEDIAEXPORT __attribute__((visibility("default")))
#endif // WIN32
#endif

typedef struct SpeexAec SpeexAec;

#ifdef  __cplusplus
extern "C" {
#endif

    MEDIAEXPORT SpeexAec* SabineAecOpen(OsInt32 inFs, int frame_size);
MEDIAEXPORT void SabineAecClose(SpeexAec *inAec);
MEDIAEXPORT void SabineAecExecute(SpeexAec *inAec,OsInt16 *ioNear,OsInt16 *inFar);

#ifdef  __cplusplus
}
#endif

#endif
