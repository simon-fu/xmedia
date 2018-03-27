clear all;
close all;
clc;
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%产生作为输出缓噪声的伪噪声序列%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% temp=(randn(1,8192)>0);
% PN=zeros(1,16384);
% PN(1:8192)=temp;
% PN(16384:-1:8193)=-temp;
% w_k=zeros(1,16384);
% w_k(1:3715)=1;
% w_k(end-3715:end)=1;
% FFT_value=w_k.*exp(j*PN*pi);
% FFT_temp(1)=0;
% FFT_temp(2:16385)=FFT_value;
% IFFT_temp=ifft(FFT_temp,16385);
% IFFT_value=IFFT_temp(2:16385);
% FINAL_value=fftshift(real(IFFT_value));
% FINAL_value=resample(FINAL_value,2,11);
% PN_value=FINAL_value;
% PN_num=1;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%产生一个回声路径的冲激响应%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
N = 240; %240个点 
t = [0 : N-1]'; %0：1：239
tau = N./4; %tau=60
envelope = exp(-(t./tau));%随即信号的幅度权值
w_k = randn(N,1).*envelope; 
w_k = [w_k(end-19 : end);w_k(1 : end-20)]; %把后20个点放到前面
w_k = w_k./sqrt(N);%再次乘上一个固定的增益
figure; 
plot(w_k);title('回声路径的冲击响应')

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%输入两段语音信号作为远端和近端信号%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% array1=wavread('quiet',100000);%近端语音
% array2=wavread('wang2',100000);%远端语音

%array1=wavread('wang1',300000);%1 2 ...
% array1=wavread('最初的梦想CD音质.wav',500000);%1 2 ...  采样频率 44100Hz 近端信号
% array2=wavread('wang2',300000);
% array1(1:100000)=wavread('quiet',100000);
% array2(end-69999:end)=wavread('quiet',70000);
% array1=wavread('1-near.wav',500000);
[array1, sampFreq1, nBits1] = wavread('1-near.wav',500000);

% array1=wavread('quiet',300000);
%array2=wavread('wang2',300000); %10 11 ....
% array2=wavread('现在就想见的人CD音质.wav',500000); %10 11 .... 远端 信号
% Rin2=array2(:,2)';
% array2=wavread('1-far.wav',500000);
[array2, sampFreq2, nBits2] = wavread('1-far.wav',500000);
Rin2=array2(:,1)';
array1=array1(:,1)';

array2=array2(:,1)';
%wavplay(array1,22050);
%wavplay(array2,22050);
M=length(array2); %序列总长度

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%通过远端语音信号与回声路径冲激响应卷积生成回声信号%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
N=length(w_k);%滤波器阶数  240个点
%pecho=zeros(1,N);
pecho=conv(array2,w_k); %%卷积处理后 长度变为M+N-1
pecho=pecho(1:M);%模拟的回声信号  取出与原始语音信号相同的点数
%array1= 0.05*mean(abs(pecho))*randn(1,M);

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%读入N个数据，设置初值%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
Rin1=array2; %远端信号  作为滤波器的输入信号

% Sinn=pecho;
Sinn=2*pecho+array1;%近端信号——近端说话人的语音信号与回声叠加构成  作为期望信号
Sinn=Sinn/max(Sinn);
% wavplay(pecho,8000);
% wavplay(Sinn,8000);

x1=Rin1(1:N);%？？？

w1=zeros(1,N);%%%滤波器系数的初始化
w2=zeros(1,N);
a=0.0001;
u=0.01;%%步长

energ_window=1/32;         %短窗功率估计
near_energ=0;
far_energ(1)=0;
for i=2:N-1
    far_energ(i)=(1-energ_window)*abs(far_energ(i-1))+energ_window*abs(x1(i-1));%初始化远端信号的能量
end;

Dtemp=x1(1:N-1)*x1(1:N-1)';%滤波器输入信号的能量
sout=zeros(1,M);
ff=0;
dd=0;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%NLMS算法的自适应滤波%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
for i=N:M; 
    x1(N)=Rin1(i);
    x2(N)=Rin2(i);
    D=Dtemp+x1(N).^2; %%D 跟Dtemp 用来干嘛？？
    far_energ(N)=(1-energ_window)*abs(far_energ(N-1))+energ_window*abs(x1(N-1));%远端信号能量
    near_energ=(1-energ_window)*abs(near_energ)+energ_window*abs(Sinn(i-1));%近端信号能量
    y1=dot(w1,x1);
    y2=dot(w2,x2);
    en=Sinn(i)-y1-y2;  
    if near_energ>1/2*max(far_energ); %%%%具有对讲保护的功能 
        sout(i)=en;
        dd=dd+1;
%       sout(i)=nlp_new(en); ;%远端模式，启动NLP 
    else
        
         w1=w1+u*en*x1/(a+D);   % D Dtemp 是滤波器输入信号的能量！！！ 
         w2=w2+u*en*x2/(a+D); 
         sout(i)=en;
%         sout(i)=nlp_new(en); ;%远端模式，启动NLP 
        ff=ff+1;
    end;
%     mis(i)=norm(w1'-w_k)/norm(w_k);  %%????
    
    %%%%%%%%%更新滤波器输入信号的能量值%%%%%%%%%%
    Dtemp=D-x1(1).^2;
    x1(1:N-1)=x1(2:N);
    x2(1:N-1)=x2(2:N);
    far_energ(1:N-1)=far_energ(2:N);%%%更新远端信号的能量
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
end;
% figure;
% plot(array1);
% hold on;
% plot(pecho,'r');
% figure;
% plot(Sinn);
figure;
plot(Rin1);title('自适应滤波器的输入信号A路')
figure;
plot(Rin2);title('自适应滤波器的输入信号B路')
figure;
plot(array1);title('近端信号')
figure; plot(Sinn); legend('near end signal');hold on; %画出期望信号时域波形
figure;plot(sout/max(sout),'r');legend('error signal');%画出输出信号的时域波形
wavwrite(Sinn,sampFreq1,16,'Sinn');%%自适应滤波器的期望信号
% wavwrite(Rin,44100,16,'Rin');%%自适应滤波器输入信号
wavwrite(sout,sampFreq1,16,'sout');%%自适应滤波器的输出信号
figure;subplot(4,1,1);plot(sout);title('输出的误差信号');
subplot(4,1,2);plot(array1);title('近端信号')
subplot(4,1,3);plot(Sinn);title('近端信号与回波的混合')
subplot(4,1,4);plot(array1);title('回波衰减比率')
% wavwrite(Sinn,8000,16,'Sinn');
% wavwrite(Rin,8000,16,'Rin');
% wavwrite(sout,8000,16,'sout');
%wavplay(sout,22050);

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%性能测试%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
Hd2 = dfilt.dffir(ones(1,1000));%%%%设计的滤波器？？？

erle = filter(Hd2,(sout-array1(1:length(sout))).^2)./ ...
    (filter(Hd2,Sinn(1:length(sout)).^2));
erledB = -10*log10(erle); 
plot(erledB);title('回波的衰减比率')



% P_Sinn=abs(Sinn).^2;
% P_en=abs(sout).^2;
% Ave_num=100;%把原始语音信号分帧 每5600个点为一帧
% P_num=fix(M/Ave_num);
% ERLE_temp=P_Sinn./P_en; %期望信号的能量比输出信号的能量
% for j=1:P_num;
%     Mark=(j-1)*Ave_num;
%     ERLE_win=ERLE_temp(Mark+1:Mark+Ave_num);
%     ERLE(j)=sum(ERLE_win)/Ave_num;
% end;
% figure;
% plot(ERLE);
% ERLE=10*log10(ERLE);%取自然对数 
% figure;
% plot(ERLE);


% figure;
% plot(10*log10(mis));                     % plot misalignment curve
% ylabel('mis');xlabel('samples');
% title('Misalignment of NLMS algorithm');
