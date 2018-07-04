#! /bin/sh

OUTFILE="./data/test_s16le_44100Hz_2ch.pcm"
ffmpeg -i ../resource/test.mp4 -acodec pcm_s16le -f s16le -ar 44100 -ac 2 $OUTFILE \
&& echo $OUTFILE
