#include "WavWriter.h"

WavWriter::WavWriter()
    : m_pFile(0)
    , m_nDataLen(0)
    , m_nSampleRate(0)
    , m_nBitsPerSamples(0)
    , m_nChannels(0)
{
}

WavWriter::~WavWriter()
{
    Close();
}

bool WavWriter::Open(const OsInt8 *inFileName,
                     OsInt32 inSampleRate,
                     OsInt16 inBitsPerSamples,
                     OsInt16 inChannels)
{
    m_pFile = fopen(inFileName,"wb");
    if (m_pFile == NULL) return false;
    m_nDataLen = 0;

    m_nSampleRate       = inSampleRate;
    m_nBitsPerSamples   = inBitsPerSamples;
    m_nChannels         = inChannels;

    writeHeader(m_nDataLen);
    return true;
}

void WavWriter::Close()
{
    if (0 == m_pFile) return;
    fseek(m_pFile,0,SEEK_SET);
    writeHeader(m_nDataLen);
    if(m_pFile)
    {
        fclose(m_pFile);
        m_pFile = 0;
    }
}

void WavWriter::Write(OsInt16 *inData,OsInt32 inLen)
{
    if (m_pFile == NULL) return;
    fwrite(inData,inLen,sizeof(OsInt16),m_pFile);
    m_nDataLen += inLen*sizeof(OsInt16);
}

void WavWriter::writeString(const OsInt8 *inStr)
{
    fputc(inStr[0],m_pFile);
    fputc(inStr[1],m_pFile);
    fputc(inStr[2],m_pFile);
    fputc(inStr[3],m_pFile);
}

void WavWriter::writeInt32(OsInt32 inValue)
{
    fputc((inValue >>  0) & 0xff,m_pFile);
    fputc((inValue >>  8) & 0xff,m_pFile);
    fputc((inValue >> 16) & 0xff,m_pFile);
    fputc((inValue >> 24) & 0xff,m_pFile);
}

void WavWriter::writeInt16(OsInt32 inValue)
{
    fputc((inValue >> 0) & 0xff,m_pFile);
    fputc((inValue >> 8) & 0xff,m_pFile);
}

void WavWriter::writeHeader(OsInt32 inLength)
{
    writeString("RIFF");
    writeInt32(4 + 8 + 16 + 8 + inLength);
    writeString("WAVE");

    writeString("fmt ");
    writeInt32(16);

    int nBytesPerFrame  = m_nBitsPerSamples/8*m_nChannels;
    int nBytesPerSec    = nBytesPerFrame*m_nSampleRate;

    writeInt16(1);                  // Format
    writeInt16(m_nChannels);        // Channels
    writeInt32(m_nSampleRate);      // Samplerate
    writeInt32(nBytesPerSec);       // Bytes per sec
    writeInt16(nBytesPerFrame);     // Bytes per frame
    writeInt16(m_nBitsPerSamples);  // Bits per sample

    writeString("data");
    writeInt32(inLength);
}