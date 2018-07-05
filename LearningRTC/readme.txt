
概要
====
在编译和运行demo前，请先安装好依赖；
brew install sdl2
brew install ffmpeg


文件和目录说明
============
make-yuv-file.sh    生成yuv文件到data目录中
make-pcm-file.sh    生成pcm文件到data目录中
data/               存放用于测试的视频和音频文件
010-yuvplayer/      一个简单的视频yuv播放器demo
020-pcmplayer/      一个简单的音频pcm播放器demo
030-camera/         摄像头录制yuv文件demo


010-yuvplayer 说明
==================
+ 一个简单的视频yuv播放器例子，依赖SDL2；
+ demo打开并读取本地yuv视频文件，并播放到一个窗口中；
+ 使用步骤：
  1）生成yuv文件： ./make-yuv-file.sh （这步不是每次必须的，如果已经生成过可以不用再执行）
  2）运行demo： cd 010-yuvplayer && ./build_and_run.sh;


020-pcmplayer 说明
==================
+ 一个简单的音频pcm播放器例子，依赖SDL2；
+ demo打开并读取本地pcm文件，并播放出声音；
+ 使用步骤：
  1）生成yuv文件： ./make-pcm-file.sh （这步不是每次必须的，如果已经生成过可以不用再执行）
  2）运行demo： cd 020-pcmplayer && ./build_and_run.sh;


030-camera 说明
==============
+ 摄像头录制yuv文件demo，依赖SDL2, FFmpeg；
+ demo打开摄像头，读取视频数据，写入到yuv文件中；
+ 可以用 yuvplayer 播放生成的yuv文件；
+ 使用步骤：
  1）运行demo： cd 030-camera && ./build_and_run.sh;

