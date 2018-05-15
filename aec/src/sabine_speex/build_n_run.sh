
gcc main.cpp sabine_aec.c fftwrap.c mdf.c scal.c smallft.c WavReader.cpp WavWriter.cpp -lstdc++ -o speex-aec \
&& ./speex-aec ../../testdata/hua_mic.wav ../../testdata/hua_ref.wav ./aecOut_speex.wav
