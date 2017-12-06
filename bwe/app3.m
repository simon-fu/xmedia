% 估计接收端带宽

clear all

[fname, fpath] = uigetfile(...
    {'*.txt', '*.*'}, ...
    'Pick a file');
if(~ischar(fname))
    disp 'wrong filename';
    return;
end
file4plot = fullfile(fpath, fname);
TT = readtable(file4plot, 'Delimiter',',', 'FileEncoding', 'UTF-8');
% 转换成时间相对值
starttime = TT.(1)(1);
TT.(1) = TT.(1)-starttime; % 单位相对毫秒
TT.(1) = TT.(1)/1000.0; % 单位为秒

plotIdMap = containers.Map;
plotIdMap('LastT')    = 1;
plotIdMap('Value')    = 1;
plotIdMap('ValueEst')    = -1;
plotIdMap('Threshold')    = 1;
% plotIdMap('Threshold')    = 1;
% plotIdMap('LastT2')    = 2;
% plotIdMap('Threshold2')    = 2;
% plotIdMap('RecvRate')    = 3;
% plotIdMap('RecvBWE')    = 3;
% plotIdMap('RCAvgMax')    = 3;
func_plot_table(fname, TT, plotIdMap, 0);
return;



rows = find(~ismember(TT.LastT, '-'));
xx = TT.Timestamp(rows);
zz = TT.LastT(rows);
if(iscell(zz) == 1)
    zz = cellfun(@(x)str2double(x), zz);
end
numDatas = size(zz,1);


yy = zeros(numDatas, 1);
yyVar = zeros(numDatas, 1);
yyDropSlope = zeros(numDatas, 1);
yySlope = zeros(numDatas, 1);
yyKalmanZ = zeros(numDatas, 1);
yyKalmanSlope = zeros(numDatas, 1);
yyKalmanSlopeDiff = zeros(numDatas, 1);
yyKalmanZ1 = zeros(numDatas, 1);

% m = 2;
% n = 2;
cQ = 1e-8;
cR = 1e-5;
QQ=[1e-8, 0; 0, 1e-8];
RR=[1e-5, 0; 0, 1e-1];

kfilter.n = 2;
kfilter.m = 2;
kfilter.A = eye(kfilter.n);
kfilter.H = eye(kfilter.n);
kfilter.B = 0;
kfilter.u = 0;
kfilter.P = eye(kfilter.n)*3e-7; % nxn
kfilter.K = zeros(kfilter.n,kfilter.m);% nxm
kfilter.Q = QQ; % nxn
kfilter.R = RR; % mxm
kfilter.x = zeros(kfilter.n,1); % 初始状态
kfilter.x(kfilter.n,1) = 2.0;
std_kfilter = kfilter;

kfilter1.n = 1;
kfilter1.m = 1;
kfilter1.A = eye(kfilter1.n);
kfilter1.H = eye(kfilter1.n);
kfilter1.B = 0;
kfilter1.u = 0;
kfilter1.P = eye(kfilter1.n); % nxn
kfilter1.K = zeros(kfilter1.n,kfilter1.m);% nxm
kfilter1.Q = eye(kfilter1.n) * 1e-8; % nxn
kfilter1.R = eye(kfilter1.m) * 1e-6; % mxm
kfilter1.x = zeros(kfilter1.n,1); % 初始状态

restartThreshold = -10;

xx0 = xx(1);
zz0 = zz(1);
for i=2:numDatas
    x = xx(i);
    z = zz(i);
    
    deltaX = x - xx0;
    deltaZ = z - zz0;
    
    if(deltaZ < -12.5)
        ps = i - 20;
        pe = i - 2;
        pattenX = xx(ps : pe, 1);
        pattenZ = zz(ps : pe, 1);
        A=polyfit(pattenX,pattenZ,1);
        yy(ps : pe, 1)=polyval(A, pattenX); 
        err = yy(ps : pe, 1)-pattenZ;        
%         yyVar(i) = err'*err;
        yyVar(i) = max(abs(err));
        yyDropSlope(i) = A(1);
        
        yyKalmanZ(i) = z;
        yyKalmanSlope(i) = yyKalmanSlope(i-1);
        yyKalmanSlopeDiff(i) = yyKalmanSlopeDiff(i-1);
        kfilter = std_kfilter;
        kfilter.x(1,1) = z;
        
        yyKalmanZ1(i) = z;
        kfilter1.x(1) = z;

    else
        yy(i) = z;
        yyDropSlope(i) = 0;
        
       if deltaX ~= 0
        yySlope(i) = deltaZ/deltaX;
        kfilter.A(1,kfilter.n)=deltaX;
        kfilter.z = [z, deltaZ/deltaX]';
        kfilter = kalman(kfilter);
        yyKalmanZ(i) = kfilter.x(1);
        yyKalmanSlope(i) = kfilter.x(2);
        yyKalmanSlopeDiff(i) = kfilter.P(kfilter.n,kfilter.n);
        
%         yyKalmanSlope(i) = (yyKalmanZ(i) - yyKalmanZ(i-1))/deltaX;
        yyKalmanSlope(i) = 0.85 * yyKalmanSlope(i-1) + 0.15*deltaZ/deltaX;
        yyKalmanSlopeDiff(i) = yyKalmanSlope(i) - yyKalmanSlope(i-1);
        else
        yySlope(i) = yySlope(i-1) ;
        yyKalmanZ(i) = yyKalmanZ(i-1);
        yyKalmanSlope(i) = yyKalmanSlope(i-1);
        yyKalmanSlopeDiff(i) = yyKalmanSlopeDiff(i-1);
       end
        kfilter1.z = z';
        kfilter1 = kalman(kfilter1);
        yyKalmanZ1(i) = kfilter1.x(1);
        
%        yyKalmanZ(i) = 0.85 * yyKalmanZ(i-1) + 0.15*z;
    end
end

minThreshold = 12.5;
threshold = minThreshold;
yyEst = zeros(numDatas, 1);
yySlopEst = zeros(numDatas, 1);
yyThreshold = zeros(numDatas, 1);
yySlopMax = zeros(numDatas, 1);
yySlopMin = zeros(numDatas, 1);

startIndex = 1;
endIndex = 1;

xx0 = xx(1);
zz0 = zz(1);
yyThreshold(1) = threshold;
lastIndex = 1;
lastDrop = 1;
for i=2:numDatas
    x = xx(i);
    z = zz(i);
    
    deltaX = x - xx0;
    deltaZ = z - zz0;
    drop = z - yyEst(i-1);
    
    if(drop < restartThreshold)
        pos = i-1;
        while (x-xx(pos)) < 3.5 && pos > 1
            pos = pos - 1;
        end
        if pos > 1 && (x-xx(pos)) >= 3.5 && pos > lastDrop 
            maxy = yySlopEst(pos);
            miny = yySlopEst(pos);
            for pos=pos+1:i-1
                if(yySlopEst(pos) > maxy)
                    maxy = yySlopEst(pos);
                end
                if(yySlopEst(pos) < miny)
                    miny = yySlopEst(pos);
                end
            end
            yySlopMax(i) = maxy;
            yySlopMin(i) = miny;
            if(maxy-miny) < 10 && miny > -0.2
                candidate = zz(i-1) + 2;
                if(candidate > minThreshold)
                    threshold = candidate;
                else
                    threshold = minThreshold;
                end
            end
        end
        
        yyKalmanSlope(i) = 0;
        lastIndex = i;
        xx0 = xx(lastIndex);
        zz0 = zz(lastIndex);
        yySlopEst(i) = 0;
        lastDrop = i;
        
    elseif(deltaX >= 1.5)
        yyKalmanSlope(i) = deltaZ/deltaX;
        lastIndex = lastIndex + 1;
        xx0 = xx(lastIndex);
        zz0 = zz(lastIndex);
        yySlopEst(i) = 0.6*yySlopEst(i-1) + 0.4*yyKalmanSlope(i);
    else
        yyKalmanSlope(i) = yyKalmanSlope(i-1);
        yySlopEst(i) = 0.6*yySlopEst(i-1) + 0.4*yyKalmanSlope(i);
        if lastDrop == i-1
            lastDrop = i;
        end
    end
    yyKalmanSlopeDiff(i) = yyKalmanSlope(i) - yyKalmanSlope(i-1);
    yyThreshold(i) = threshold;
    yyEst(i) = 0.6*yyEst(i-1) + 0.4*zz(i);
    
end

table4Draw = table;
table4Draw.xx = xx;
table4Draw.zz = zz;
% table4Draw.yy = yy;
% table4Draw.yyVar = yyVar;
% table4Draw.yyDropSlope = yyDropSlope;
% table4Draw.yySlope = yySlope;
% table4Draw.yyKalmanZ = yyKalmanZ;
% table4Draw.yyKalmanZ2 = yyKalmanZ;
table4Draw.yyKalmanSlope = yyKalmanSlope;
table4Draw.yyKalmanSlopeDiff = yyKalmanSlopeDiff;
% table4Draw.yyKalmanZ1 = yyKalmanZ1;
table4Draw.yyEst = yyEst;
table4Draw.yySlopEst = yySlopEst;
table4Draw.yyThreshold = yyThreshold;
table4Draw.yySlopMax = yySlopMax;
table4Draw.yySlopMin = yySlopMin;
plotIdMap = containers.Map;
plotIdMap('zz')    = 1;
% plotIdMap('yyKalmanZ')    = 1;
% plotIdMap('yyKalmanZ1')    = 2;
plotIdMap('yyKalmanSlope')    = 2;
plotIdMap('yySlopEst')    = 2;
plotIdMap('yyThreshold')    = 1;
plotIdMap('yySlopMax')    = 3;
plotIdMap('yySlopMin')    = 3;
plotIdMap('yyEst')    = 1;
func_plot_table(fname, table4Draw, plotIdMap, -1);



