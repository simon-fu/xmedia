% refer http://bbs.loveuav.com/forum.php?mod=viewthread&tid=274&extra=page%3D1&page=1
% 一个网上的kalman例子，生成数据，然后滤波
clear
close all
clc

% 生成测试数据
interval = pi/18;
t = 1:interval:100*pi;
len = size(t, 2);
a = t + 4 * (rand(1,len)-0.5);
b = t .* sin(t/10) +  10 * (rand(1,len)-0.5);
z = [a; b];
%save('z.mat','z');
%plot(z(1,:),z(2,:),'o');

% 1、静止
dim_observe = 2;      %观测值维数
n = dim_observe;  %状态维数
A = eye(n);
H = eye(n);
cQ = 1e-8;
cR = 1e-2; % 1e-2 1e-4 1e-8
app1_run( dim_observe, n, A, H, cQ, cR, z);

cR = 1e-4; 
app1_run( dim_observe, n, A, H, cQ, cR, z);

% 2、匀速运动
n = 2 * dim_observe;  %状态维数，观测状态每个维度都有1个速度，故需乘2
A = [1,0,1,0;0,1,0,1;0,0,1,0;0,0,0,1];
H = [1,0,0,0;0,1,0,0];
cQ = 1e-8;
cR = 1e-2;
app1_run( dim_observe, n, A, H, cQ, cR, z);

cR = 1e-4;
app1_run( dim_observe, n, A, H, cQ, cR, z);




