rm -rf tmp/* &&\
mkdir -p tmp &&\
adb pull /sdcard/audio.aecdump ./tmp/ &&\
cd tmp/ &&\
../bin/unpack_aecdump audio.aecdump &&\
echo split done