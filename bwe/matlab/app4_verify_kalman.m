
clear;

% 测试卡尔曼滤波器

% 生成测试数据
count = 100;
tt = 1:1:count; tt=tt';% 时间轴

ideal = 7 + 4*tt'; ideal = ideal'; % 理论值



% zz = ideal + 15*randn(size(ideal)); % 测量值
zz = ideal + 5*sin(ideal); % 测量值


k1.cQ=1e-8; k1.cR=1e-4; k1=kalman(k1, 1, 1);
k1.P = 0.0001; % 为了画KPP时不要偏差太多导致看不清楚小值
k2.cQ=1e-15; k2.cR=1e-1;  k2=kalman(k2, 2, 1);
k2.x = [1, 1]'; 
% k2.Q = [ 1e-8, 0; 0, 1e-9];

zzEst1 = zeros(count, 1);  % 估计值
zzEst2 = zeros(count, 1);  % 估计值
zzOffset2 = zeros(count, 1);  % 偏移估计值
zzSlop2 = zeros(count, 1);  % 斜率估计值
zzDiff2 = zeros(count, 1);

% 用卡尔曼滤波器处理数据
lastT = tt(1);
for i=1:count
    deltaT = tt(i); %-tt(1);
    lastT = tt(i);
    
    k1.z = zz(i);
    k1 = kalman(k1);
    zzEst1(i)=k1.x;
    
    k2.z = zz(i);
    k2.H = [1 deltaT];
    k2 = kalman(k2);
    
    zzEst2(i)=k2.x(1,1)+k2.x(2,1)*deltaT;
    zzOffset2(i) = k2.x(1,1);
    zzSlop2(i) = k2.x(2,1);
    zzDiff2(i) = zzEst2(i) - ideal(i);
end
% zzSlop2(1:3) = 0;

% 画图
table4Draw = table;
table4Draw.tt = tt;
table4Draw.ideal = ideal;
table4Draw.zz = zz;
table4Draw.zzEst1 = zzEst1;
table4Draw.zzEst2 = zzEst2;
table4Draw.zzOffset2 = zzOffset2;
table4Draw.zzSlop2 = zzSlop2;
table4Draw.zzDiff2 = zzDiff2;
plotIdMap = containers.Map;
plotIdMap('ideal')    = 1;
plotIdMap('zz')    = 1;
plotIdMap('zzEst1')    = 1;
plotIdMap('zzEst2')    = 1;

func_plot_table('kalman', table4Draw, plotIdMap, 0);
return;

