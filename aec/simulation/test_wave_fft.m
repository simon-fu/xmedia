% 使用 MATLAB 处理声音的基本操作
% http://www.lab-z.com/wp-content/uploads/2013/02/mlb.pdf

[snd, sampFreq, nBits] = wavread('channels2.wav');
ds=size(snd)
datalength=ds(1)
% sound(snd, sampFreq, nBits) 
s1 = snd(:,1); 
timeArray = (0:datalength-1) / sampFreq; % 建立一个包含时间点的数组
timeArray = timeArray * 1000; % 时间点放大到毫秒级
plot(timeArray, s1, 'k'); 
% n=size(s1);
n= length(s1);
p=fft(s1);
disp('fft done');

is1=ifft(p);
% is1=roundn(is1,-2); % 四舍五入
% s1=roundn(s1,-2);
disp('ifft done');
figure;
plot(timeArray, is1, 'k') 
% sound(is1, sampFreq, nBits) % 播放ifft后的音频
% figure;
% plot(timeArray, is1-s1, 'k') % 绘制 原始信号与ifft之后的信号差值

equ=isequal(s1, is1);
disp(['equ=' num2str(equ)]);

nUniquePts = ceil((n+1)/2);
p = p(1:nUniquePts); % 选择前半部，因为后半部是前半部的一个镜像
p = abs(p); % 取绝对值，或者称之为幅度 
  % FFT 函数处理音频返回值包括幅度和相位信息，是以复数的形式给出的（返回复
  % 数）。对傅立叶变换后的结果取绝对值后，我们就可以取得频率分量的幅度信息。

 p = p/n; % 使用点数按比例缩放，这样幅度和信号长度或者它自身的频率无关
 p = p.^2; % 平方得到功率 乘以 2（原因请参考上面的文档）
 if rem(n, 2) % 奇数，nfft 需要排除奈奎斯特点
 p(2:end) = p(2:end)*2;
 else
 p(2:end -1) = p(2:end -1)*2;
 end
freqArray = (0:nUniquePts-1) * (sampFreq / n); % 创建频率数组
plot(freqArray/1000, 10*log10(p), 'k')
xlabel('Frequency (kHz)')
ylabel('Power (dB)') 

disp('done');
