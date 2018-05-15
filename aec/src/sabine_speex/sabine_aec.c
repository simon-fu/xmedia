#include "sabine_aec.h"
#include "speex_echo.h"

struct SpeexAec
{
    OsInt32         fs;
    OsInt32         frame_size;
    SpeexEchoState  *aec;
};

SpeexAec* SabineAecOpen(OsInt32 inFs, int frame_size)
{
    SpeexAec *pAec = (SpeexAec*)icm_alloc(1,sizeof(SpeexAec));
    assert(0 != pAec);
    pAec->fs            = inFs;
//    pAec->frame_size    = inFs/100*2;
    pAec->frame_size    = frame_size;
    pAec->aec           = speex_echo_state_init(pAec->frame_size,4096);
    return pAec;
}

void SabineAecClose(SpeexAec *inAec)
{
    speex_echo_state_destroy(inAec->aec);
    icm_free(inAec);
}

void SabineAecExecute(SpeexAec *inAec,OsInt16 *ioNear,OsInt16 *inFar)
{
    assert(0 != inAec);

    speex_echo_cancellation(inAec->aec,ioNear,inFar,ioNear);
}
