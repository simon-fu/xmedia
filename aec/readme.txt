
./mac 目录下存放的编译webrtc mac版的命令行程序

# ./mac/unpack_aecdump ./tmp/audio.aecdump   
在当前目录把 audio.aecdump 解出以下几个文件：
input.wav     	麦克风输入声音
reverse.wav 	播放的远端声音
ref_out.wav     当时APM处理后的声音（录制audio.aecdump时做的APM处理，比如aec，ns，agc等）
settings.txt	当时的APM配置

# ./mac/audioproc -aec -pb ./tmp/audio.aecdump -o ./tmp/out
# ./mac/audioproc -aecm -pb ./tmp/audio.aecdump -o ./tmp/out
# ./mac/audioproc -aecm -ns -vad -agc -hpf --delay 300  -pb ./tmp/audio.aecdump -o ./tmp/out
用APM 重新回放处理并输出out.wav 文件，对应当时的 ref_out.wav 文件



# ./bin/aec-replay tmp-dspalgorithms/left.wav tmp-dspalgorithms/right.wav tmp-dspalgorithms/out.wav 0 0
# ./bin/aec-replay tmp-icoolmeida/mic.wav tmp-icoolmeida/ref.wav tmp-icoolmeida/out.wav 0 0
自写的回放处理aec

