
概要
====
在编译和运行demo前，请先安装好依赖；
brew install sdl2
brew install ffmpeg


文件和目录说明
============
make-yuv-file.sh    生成yuv文件到data目录中
data/               存放用于测试的视频和音频文件
yuvplayer/          一个简单的yuv播放器例子


yuvplayer 说明
==============
一个简单的yuv播放器例子，依赖SDL2；
编译运行：
  1）生成yuv文件： ./make-yuv-file.sh  （这步不是每次必须的，如果已经生成过可以不用再执行）
  2）编译运行： cd yuvplayer && ./build.sh && ./yuvplayer
