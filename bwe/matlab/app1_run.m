function [  ] = app1_run( dim_observe, n, A, H, cQ, cR, z)
% dim_observe - 观测值维数
% n - 状态维数
% A - 状态转移矩阵
% H - 观测矩阵
% cQ - 预测噪声协方差
% cR - 测量噪声协方差
% z - 测量数据  

filter.A = A;
filter.H = H;
filter.B = 0;
filter.u = 0;
filter.P = eye(n); % nxn
filter.K = zeros(n,dim_observe);% nxm

filter.Q = eye(n) * cQ;        %这里简单设置Q和R对角线元素都相等，设为不等亦可
filter.R = eye(dim_observe) * cR;
filter.x = zeros(n,1); %初始状态x0
%load('z.mat');
figure,subplot(1,2,1),
t = 1;
out = [];
for i=1:size(z,2)
  filter.z = z(:,i);
  filter = kalman(filter);
  plot(filter.x(1),filter.x(2),  'r*');hold on        
  plot(filter.z(1),filter.z(2),  'bo');        hold on
  out=[out filter.x];
%         pause(.5)
end
% figure(1),
str = sprintf('cQ = %e, cR = %e', cQ, cR);
title(str)
%画局部放大
subplot(1,2,2), 
plot(out(1,:),out(2,:),  'r*');hold on        
plot(z(1,:),z(2,:),  'bo');        hold on
axis([120 170 80 200]);

end

