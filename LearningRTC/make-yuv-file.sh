#! /bin/sh

OUTFILE="./data/test_yuv420p_640x360.yuv"
ffmpeg -i ../resource/test.mp4 -an -pix_fmt yuv420p $OUTFILE \
&& echo $OUTFILE