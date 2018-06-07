% close all;
% N=64;
% std_time_data=[1:2*N];
% time_data_for_fft=std_time_data;
% freq_data_for_fft=fft(time_data_for_fft);
% inv_time_data=ifft(freq_data_for_fft);
% diff=inv_time_data-std_time_data;
% [max,index]=max(abs(diff));
% max
% index


close all;
clear;  
% N=1024;  %长度  
N=64;  %长度
d=0;     %延迟点数
Fs=500;  %采样频率  
n=0:N-1;  
t=n/Fs;   %时间序列  
a1=1;     %信号幅度  
a2=1;    
x1=a1*cos(2*pi*10*n/Fs);     %信号1  
% x1=x1+randn(size(x1))/max(randn(size(x1)));      %加噪声  
x2=a2*cos(2*pi*10*(n+d)/Fs); %信号2  
% x2=x2+randn(size(x2));   % 噪声

% N = 64;
% x1 = [1:N];
% x2 = [d:N,1:d-1];
offset=abs(d);
x2=[x1(offset+1:N), x1(1:offset)];
x2=[x1(N-offset:N), x1(1:N-offset-1)];

ex1=[x1, zeros(1,N)];
ex2=[x2, zeros(1,N)];
Y1=fft(ex1);  
Y2=fft(ex2); 
S12=Y1.*conj(Y2);  
time12=ifft(S12);
time12_real=real(time12);
Cxy=fftshift(time12_real);  
[maxval,location]=max(Cxy);%求出最大值max,及最大值所在的位置（第几行）location;  
d2=location-N ; %求得延迟点数
Delay2=d2/Fs ;   %求得时间延迟  

Cxy2 = xcorr(x1,x2);
DiffCxy=abs(Cxy2-Cxy(2:2*N));
[MaxDiffCxy]=max(DiffCxy);

d2
Delay2
maxval
MaxDiffCxy

% figure; plot(x1);
% figure; plot(x2);

% figure; plot(time12);
% figure; plot(time12_real);
figure; plot(Cxy);
figure; plot(Cxy2);
