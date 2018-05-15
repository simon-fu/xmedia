#include "WavReader.h"
#include "WavWriter.h"
#include "sabine_aec.h"
// #include <vld.h>
// #pragma comment(lib,"vld.lib")

int main(int argc,char** argv)
{

    const char * micFile = "hua_mic.wav";
    const char * refFile = "hua_ref.wav";
    const char * outFile = "aecOut_speex.wav";
    OsInt32 nFs = 16000;
    OsInt32 nFrameSize  = nFs/100*2;

    if(argc > 1){
        micFile = argv[1];
    }

    if(argc > 2){
        refFile = argv[2];
    }

    if(argc > 3){
        outFile = argv[3];
    }


    printf("micFile=[%s]\n", micFile);
    printf("refFile=[%s]\n", refFile);
    printf("outFile=[%s]\n", outFile);
    printf("nFs=[%d]\n", nFs);
    printf("nFrameSize=[%d]\n", nFrameSize);

    WavReader *pReader1 = new WavReader();
    assert(0 != pReader1);
    bool bRet = pReader1->Open(micFile);
//    bool bRet = pReader1->Open("..\\near.wav");
    assert(true == bRet);
    WavReader *pReader2 = new WavReader();
    assert(0 != pReader2);
    bRet = pReader2->Open(refFile);
//    bRet = pReader2->Open("..\\far.wav");
    assert(true == bRet);

    nFs = pReader1->GetSampleRate();
    nFrameSize  = nFs/100*2;
    WavWriter *pWriter = new WavWriter();
    assert(0 != pWriter);
    bRet = pWriter->Open(outFile,nFs,16,1);
    assert(true == bRet);

    OsInt16 *pMic = (OsInt16*)malloc(sizeof(OsInt16)*nFrameSize);
    assert(0 != pMic);
    OsInt16 *pRef = (OsInt16*)malloc(sizeof(OsInt16)*nFrameSize);
    assert(0 != pRef);

    OsInt32 nDelayMs = 0;

    SpeexAec *aecInst = SabineAecOpen(nFs, nFrameSize);
    assert(0 != aecInst);
    OsInt32 nReadLen = nFrameSize;

    while (nReadLen > 0)
    {
        nReadLen = pReader1->Read(pMic,nFrameSize);
        if(nReadLen <= 0) break;
        nReadLen = pReader2->Read(pRef,nFrameSize);
        if(nReadLen <= 0) break;

        SabineAecExecute(aecInst,pMic,pRef);

        pWriter->Write(pMic,nFrameSize);
    }
    printf("Done\n");

    SabineAecClose(aecInst);

    free(pMic);
    free(pRef);

    delete pReader1; 
    delete pReader2;
    delete pWriter;
    return 0;
}