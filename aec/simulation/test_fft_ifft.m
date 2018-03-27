% 演示数据经过fft 和 ifft 处理的误差；
% 测试结果: 整形信号，误差为0；浮点型信号，误差非常小（e-16）
% 生成数据：如果对输入数据和输出数据做四舍五入，误差为0；
% 测试wav：有误差

N=16*100000; % 生成测试数据个数
A=0.6; % 生成数据的最大值
dot=4; % 四舍五入小数

% x=A*rand(N,1); % 生成浮点数
% x=floor(A*rand(N,1)); % 生成整数
% x=[8  6  0  9  0  5  1  3  8  2  8  7  3  2  2  7]'; % 固定数
[snd, sampFreq, nBits] = wavread('channels2.wav'); x=snd(:,1); % 从 wav 文件读取

p=fft(x);
x2=ifft(p);
x2r=round(x2,dot); %  小数四舍五入
xr=round(x,dot); % 小数四舍五入
diff=x2-x;
diffr=x2r-xr;
% diff=diffr;
diffabs=abs(diff);
equ=isequal(x2r, xr); % 是否相等
[maxerr, maxerri]=max(diffabs); % 最大误差
[minerr, minerri]=min(diffabs); % 最大误差

disp(['length=' num2str(length(x))]);
% disp(['x=' num2str(x')]);
% disp(['x2=' num2str(x2')]);
% disp(['x2r=' num2str(x2r')]);
% disp(['diff=' num2str(diff')]);
disp(['maxerr=' num2str(maxerr)]);
disp(['minerr=' num2str(minerr)]);
fprintf('  x[%d] =%f\n',maxerri, x(maxerri));
fprintf(' x2[%d] =%f\n',maxerri,x2(maxerri));
fprintf(' xr[%d] =%f\n',maxerri, xr(maxerri));
fprintf('x2r[%d] =%f\n',maxerri,x2r(maxerri));
disp(['equ=' num2str(equ)]);

fprintf('round(a,%d) =%f\n',dot, round(a, dot));
fprintf('round( x(%d),%d) =%f\n',maxerri, dot, round(x(maxerri), dot));
fprintf('round(x2(%d),%d) =%f\n',maxerri, dot, round(x2(maxerri), dot));
fprintf('  diff[%d] =%f\n',maxerri, x2(maxerri)-x(maxerri));
