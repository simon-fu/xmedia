% 从txt文件中读取数据，用kalman处理myrtc生成的rtt，loss等数据
% 

% clear all
% clc;
% close all;

[fname, fpath] = uigetfile(...
    {'*.txt', '*.*'}, ...
    'Pick a file');

x = load(fullfile(fpath, fname));

m = 1;
n = 1;
cQ = 1e-8;
cR = 1e-6;

kfilter.A = eye(n);
kfilter.H = eye(n);
kfilter.B = 0;
kfilter.u = 0;
kfilter.P = eye(n); % nxn
kfilter.K = zeros(n,m);% nxm
kfilter.Q = eye(n) * cQ; % nxn
kfilter.R = eye(m) * cR; % mxm
kfilter.x = zeros(n,1); % 初始状态x0


figure;

name = 'RTT';
cQ = 1e-8;
cR = 1e-6;
kfilter.Q = eye(n) * cQ; 
kfilter.R = eye(m) * cR; 
z = x(:,2)';
out_kalman = kalman_process(kfilter, z)';
out_aver=filter(ones(1,10)/10,1,z)';
subplot(2,1,1);
H1 = plot(z, 'k-');
hold on;
K1 = plot(out_kalman(:,1), 'r-');
hold on;
AVER1 = plot(out_aver(:,1), 'b-');
hold on;
legend([H1,K1, AVER1], name, 'EST-KALMAN', 'EST-AVER');
str = sprintf('cQ = %e, cR = %e', cQ, cR);
title(str)
grid on

name = 'LOST';
cQ = 1e-4;
cR = cQ*8;
kfilter.Q = eye(n) * cQ; 
kfilter.R = eye(m) * cR; 
z = x(:,3)';
out_kalman = kalman_process(kfilter, z)';
subplot(2,1,2);
H1 = plot(z, 'k-');
hold on;
R1 = plot(out_kalman(:,1), 'r-');
hold on;
legend([H1,R1], name, 'EST-KALMAN');
str = sprintf('cQ = %e, cR = %e', cQ, cR);
title(str)
grid on



