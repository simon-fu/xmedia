#ifndef WAVWRITER_H
#define WAVWRITER_H

#include "Platform.h"

/**
 * @brief   Wav文件生成器
 */
class WavWriter
{
public:
    WavWriter();
    ~WavWriter();

    bool Open(const OsInt8 *inFileName,
              OsInt32 inSampleRate,
              OsInt16 inBitsPerSamples,
              OsInt16 inChannels);
    void Close();

    void Write(OsInt16 *inData,OsInt32 inLen);
private:
    void writeString(const OsInt8 *inStr);
    void writeInt32(OsInt32 inValue);
    void writeInt16(OsInt32 inValue);
    void writeHeader(OsInt32 inLength);

    FILE    *m_pFile;
    OsInt32 m_nDataLen;
    OsInt32 m_nSampleRate;
    OsInt32 m_nBitsPerSamples;
    OsInt32 m_nChannels;
};

#endif

