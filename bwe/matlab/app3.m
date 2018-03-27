% 读取文件并画图
% 或 估计接收端的阈值

clear


% 重构 webrtc 接收端带宽估计器OveruseEstimator 的Matlab程序

% 读取文件
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
% TT.(1) = TT.(1)/1000.0; % 单位为秒




% % 单独估计timestamp周期
% TTOut = table;
% rows2 = find(~ismember(TT.RtpTimestamp, '-'));
% numRows2 = size(rows2,1);
% TTOut.Timestamp = TT.Timestamp(rows2);
% TTOut.RtpTimestamp = TT.RtpTimestamp(rows2);
% TTOut.RtpTsCircle = zeros(numRows2, 1);
% TTOut.RtpTsCircleEst = zeros(numRows2, 1);
% if(iscell(TTOut.RtpTimestamp) == 1)
%     TTOut.RtpTimestamp = cellfun(@(x)str2double(x), TTOut.RtpTimestamp);
% end
% basePos = 1;
% interval = 2500;
% for i=2:numRows2
%     if(i > 1 && (TTOut.RtpTimestamp(i) - TTOut.RtpTimestamp(i-1)) > 20000 ) 
%         basePos = i; 
%         TTOut.RtpTsCircle(i) = 0;
%         TTOut.RtpTsCircleEst(i) = 0;
%         continue;
%     end
%     nowms = TTOut.(1)(i);
%     elapse = nowms - TTOut.(1)(basePos);
%     if elapse > interval
%         deltaTS = (TTOut.RtpTimestamp(i) - TTOut.RtpTimestamp(basePos));
%         TTOut.RtpTsCircle(i) = deltaTS * 1000 / elapse;
%         for pos = basePos:i
%             pre = TTOut.(1)(pos);
%             elapse = nowms - pre;
% %             fprintf('pos=%d, pre=%f, nowms=%f, elapse=%f\n', pos, pre, nowms, elapse);, 
%             if(elapse < interval) 
%                 break; 
%             end;
%         end
%         basePos = pos;
%     else
%         TTOut.RtpTsCircle(i) = TTOut.RtpTsCircle(i-1);
%     end
%     if(TTOut.RtpTsCircle(i) ~= 0)
%         if(TTOut.RtpTsCircleEst(i-1) == 0)
%             TTOut.RtpTsCircleEst(i) = TTOut.RtpTsCircle(i);
%         else
%             TTOut.RtpTsCircleEst(i) = 0.95*TTOut.RtpTsCircleEst(i-1) + 0.05*TTOut.RtpTsCircle(i);
%         end
%     else
%         TTOut.RtpTsCircleEst(i) = 0;
%     end
% end
% 
% plotIdMap = containers.Map;
% plotIdMap('RtpTimestamp')    = 1;
% plotIdMap('RtpTsCircle') = 2;
% plotIdMap('RtpTsCircleEst') = 2;
% func_plot_table(fname, TTOut, plotIdMap, 0);
% 
% clear TTOut;
% return;


% % 单独估计timestamp周期
% TTOut = table;
% rows2 = find(~ismember(TT.RtpTimestamp, '-'));
% numRows2 = size(rows2,1);
% 
% TTOut.Timestamp = TT.Timestamp(rows2);
% TTOut.RtpTimestamp = TT.RtpTimestamp(rows2);
% TTOut.RtpTsCircle = zeros(numRows2, 1);
% TTOut.RtpTsCircleEst = zeros(numRows2, 1);
% 
% if(iscell(TTOut.RtpTimestamp) == 1)
%     TTOut.RtpTimestamp = cellfun(@(x)str2double(x), TTOut.RtpTimestamp);
% end
% basePos = 1;
% interval = 2500;
% for i=2:numRows2
%     if(i > 1 && (TTOut.RtpTimestamp(i) - TTOut.RtpTimestamp(i-1)) > 20000 ) 
%         basePos = i; 
%         TTOut.RtpTsCircle(i) = 0;
%         TTOut.RtpTsCircleEst(i) = 0;
%         continue;
%     end
%     nowms = TTOut.(1)(i);
%     elapse = nowms - TTOut.(1)(basePos);
%     if elapse > interval
%         deltaTS = (TTOut.RtpTimestamp(i) - TTOut.RtpTimestamp(basePos));
%         TTOut.RtpTsCircle(i) = deltaTS * 1000 / elapse;
%         for pos = basePos:i
%             pre = TTOut.(1)(pos);
%             elapse = nowms - pre;
% %             fprintf('pos=%d, pre=%f, nowms=%f, elapse=%f\n', pos, pre, nowms, elapse);, 
%             if(elapse < interval) 
%                 break; 
%             end;
%         end
%         basePos = pos;
%     else
%         TTOut.RtpTsCircle(i) = TTOut.RtpTsCircle(i-1);
%     end
%     if(TTOut.RtpTsCircle(i) ~= 0)
%         if(TTOut.RtpTsCircleEst(i-1) == 0)
%             TTOut.RtpTsCircleEst(i) = TTOut.RtpTsCircle(i);
%         else
%             TTOut.RtpTsCircleEst(i) = 0.95*TTOut.RtpTsCircleEst(i-1) + 0.05*TTOut.RtpTsCircle(i);
%         end
%     else
%         TTOut.RtpTsCircleEst(i) = 0;
%     end
% end
% plotIdMap = containers.Map;
% plotIdMap('RtpTimestamp')    = 1;
% plotIdMap('RtpTsCircle') = 2;
% plotIdMap('RtpTsCircleEst') = 2;
% func_plot_table(fname, TTOut, plotIdMap, 0);
% clear TTOut;
% return;



rows = find(~ismember(TT.LastT, '-'));
numRows = size(rows,1);

% 把 TT -> TTOut，cell转成double
% TTOut = table;
% TTOut.Timestamp = TT.Timestamp(rows);
% TTOut.TimeDelta = TT.TimeDelta(rows);
% TTOut.RtpTimeDelta = TT.RtpTimeDelta(rows);
% TTOut.SizeDelta = TT.SizeDelta(rows);
% TTOut.LastT = TT.LastT(rows);
% TTOut.NumDeltas = TT.NumDeltas(rows);
% 
% if(iscell(TTOut.TimeDelta) == 1)
%     TTOut.TimeDelta = cellfun(@(x)str2double(x), TTOut.TimeDelta);
% end
% if(iscell(TTOut.RtpTimeDelta) == 1)
%     TTOut.RtpTimeDelta = cellfun(@(x)str2double(x), TTOut.RtpTimeDelta);
% end
% if(iscell(TTOut.SizeDelta) == 1)
%     TTOut.SizeDelta = cellfun(@(x)str2double(x), TTOut.SizeDelta);
% end
% if(iscell(TTOut.LastT) == 1)
%     TTOut.LastT = cellfun(@(x)str2double(x), TTOut.LastT);
% end
% if(iscell(TTOut.NumDeltas) == 1)
%     TTOut.NumDeltas = cellfun(@(x)str2double(x), TTOut.NumDeltas);
% end
% 
% TTOut.TTSDelta = zeros(numRows, 1);
% TTOut.yyT = zeros(numRows, 1);
% TTOut.yySlope = zeros(numRows, 1);
% TTOut.yyTDiff = zeros(numRows, 1);
% TTOut.yyOffsetK = zeros(numRows, 1);
% TTOut.yySlopeK = zeros(numRows, 1);
% TTOut.TTSDelta = TTOut.TimeDelta - TTOut.RtpTimeDelta;



numDatas = size(TT.(1),1);
TTOut = table;
TTOut.TTSDelta = cell(numDatas, 1); 
TTOut.yyT = cell(numDatas, 1);
TTOut.yySlope = cell(numDatas, 1);
TTOut.yyTDiff = cell(numDatas, 1);
TTOut.yyOffsetK = cell(numDatas, 1);
TTOut.yySlopeK = cell(numDatas, 1);
TTOut.TTSDelta = cell(numDatas, 1);


numOutVars = size(TTOut.Properties.VariableNames,2);
for i = 1:numOutVars
    TTOut.(i)(:) = {'-'};
    TTOut.(i)(rows(1)) = {'0'};
end
nextRowIndex = 2;


if ismember('RtpTimestamp', TT.Properties.VariableNames)
    existRtpTimestamp = 1;
    tsRows = find(~ismember(TT.RtpTimestamp, '-'));
    numTsRows = size(tsRows,1);
    RtpTimestamps = TT.RtpTimestamp(tsRows);
    if(iscell(RtpTimestamps) == 1)
        RtpTimestamps = cellfun(@(x)str2double(x), RtpTimestamps);
    end

    TTOutTs = table;
    TTOutTs.RtpTsCircle = cell(numDatas, 1);
    TTOutTs.RtpTsCircleEst = cell(numDatas, 1);

    numTsVars = size(TTOutTs.Properties.VariableNames,2);
    for i = 1:numTsVars
        TTOutTs.(i)(:) = {'-'};
        TTOutTs.(i)(tsRows(1)) = {'0'};
    end
    basePos = 1;
    interval = 2500;
else
    existRtpTimestamp = 0;
end



% OveruseEstimator 构造函数中的初始值
initial_slope = 8.0/512.0; % 0.015625;
initial_offset = 0;
initial_e = [100, 0; 0, 1e-1];
initial_avg_noise = 0.0;
initial_var_noise  =50;
initial_process_noise = [1e-13,  1e-3];

% 调整初始值看效果
% initial_var_noise = 500;
% initial_process_noise = [1e-14,  1e-4];

k1=kalman(struct, 2, 1);
k1.P = initial_e;  % webrtc 里 P 是 E_
k1.Q(1,1) = initial_process_noise(1);
k1.Q(2,2) = initial_process_noise(2);
k1.R = initial_var_noise;
k1.x = [initial_slope, initial_offset]';



% % 分布图
% figure;
% pos=1; len = size(TTOut.SizeDelta,1);
% rows=pos:1:pos+len-1;rows = rows';
% plot(TTOut.SizeDelta(rows), TTOut.TTSDelta(rows), '.');
% figure;
% H = hist(TTOut.SizeDelta(rows), 50);
% plot(H);
% hold on;grid on;
% return;


if existRtpTimestamp
    TTOutTs.RtpTsCircle(tsRows(1)) = {'0'};
    TTOutTs.RtpTsCircleEst(tsRows(1)) = {'0'};
    numtsout = 1;
    nextTsIndex = 2;
    lastSampleRate = 90000;
end

for i=2:numDatas
    nowms = TT.(1)(i);
    if existRtpTimestamp && nextTsIndex <= numTsRows && i == tsRows(nextTsIndex)
        
        delta = (RtpTimestamps(nextTsIndex) - RtpTimestamps(nextTsIndex-1));
        if(i > 1 && (delta > 20000) ) 
            basePos = nextTsIndex; 
            TTOutTs.RtpTsCircle(i) = {'0'};
            TTOutTs.RtpTsCircleEst(i) = {'0'};
            numtsout = numtsout + 1;
            nextTsIndex = nextTsIndex + 1;
            continue;
        end
        
        elapse = nowms - TT.(1)(tsRows(basePos));
        if elapse > interval
            deltaTS = (RtpTimestamps(nextTsIndex) - RtpTimestamps(basePos));
            samplerate = deltaTS * 1000 / elapse;
            TTOutTs.RtpTsCircle(i) = {num2str(samplerate)};
            numtsout = numtsout + 1;
            for pos = basePos:nextTsIndex
                pre = TT.(1)(tsRows(pos));
                elapse = nowms - pre;
    %             fprintf('pos=%d, pre=%f, nowms=%f, elapse=%f\n', pos, pre, nowms, elapse);, 
                if(elapse < interval) 
                    break; 
                end;
            end
            basePos = pos;
        else
%             TTOutTs.RtpTsCircle(i) = TTOutTs.RtpTsCircle(i-1);
            TTOutTs.RtpTsCircle(tsRows(nextTsIndex)) = TTOutTs.RtpTsCircle(tsRows(nextTsIndex-1));
            samplerate = str2double(TTOutTs.RtpTsCircle(tsRows(nextTsIndex)));
            numtsout = numtsout + 1;
        end
        
        if(samplerate ~= 0)
            if(ismember(TTOutTs.RtpTsCircleEst(tsRows(nextTsIndex-1)), '0'))
%                 TTOutTs.RtpTsCircleEst(i) = TTOutTs.RtpTsCircle(i);
                TTOutTs.RtpTsCircleEst(tsRows(nextTsIndex)) = TTOutTs.RtpTsCircle(tsRows(nextTsIndex));
                
            else
                oldval = str2double(TTOutTs.RtpTsCircleEst(tsRows(nextTsIndex-1)));
                newval = 0.95*oldval + 0.05*samplerate;
                TTOutTs.RtpTsCircleEst(tsRows(nextTsIndex)) = {num2str(newval)};
            end
        else
            TTOutTs.RtpTsCircleEst(tsRows(nextTsIndex)) = {'0'};
        end
        nextTsIndex = nextTsIndex + 1;
    end
    




    if nextRowIndex > numRows || i ~= rows(nextRowIndex)
        continue;
    end
    nextRowIndex = nextRowIndex + 1;
    
%     t_delta = TTOut.TimeDelta(i);
%     ts_delta = TTOut.RtpTimeDelta(i);
%     size_delta = TTOut.SizeDelta(i);
%     num_of_deltas = TTOut.NumDeltas(i);
    
    t_delta = str2double(TT.TimeDelta(i));
    ts_delta = str2double(TT.RtpTimeDelta(i));
    size_delta = str2double(TT.SizeDelta(i));
    num_of_deltas = str2double(TT.NumDeltas(i));
    
    % 处理 ts_delta 突变情况
%     if(i > 1 && (ts_delta - TTOut.RtpTimeDelta(i-1)) > 200 ) 
%         ts_delta = TTOut.RtpTimeDelta(i-1);
%     end
    
    % 有时候timestamp周期不一定是90000
    % ts_delta = ts_delta + ts_delta*35/1000; 
    if existRtpTimestamp
        if nextTsIndex <= numTsRows
            oldval = str2double(TTOutTs.RtpTsCircleEst(tsRows(nextTsIndex-1)));
            if(oldval > 1)
                lastSampleRate = oldval;
            end
        end
        ts_delta = ts_delta * 90000/lastSampleRate;
    end
    
    t_ts_delta = t_delta - ts_delta;
    fs_delta = size_delta;
    
    k1.H = [fs_delta, 1.0];
    k1.z  = t_ts_delta;
    k1 = kalman(k1);
    slop_ = k1.x(1);
    offset_ = k1.x(2);
    yyt = min(num_of_deltas, 60) * offset_;
    
    TTOut.yySlopeK(i) = {num2str((k1.z - k1.H * k1.PX))};
    TTOut.yyOffsetK(i) = {num2str(k1.K(2))};

    TTOut.TTSDelta(i) = {num2str(t_ts_delta)};
    TTOut.yySlope(i) = {num2str(slop_)};
    TTOut.yyT(i) = {num2str(yyt)}; 
    
    lastT = str2double(TT.LastT(i));
    TTOut.yyTDiff(i) = {num2str(yyt - lastT)};
    
%     TTOut.TimeDelta(i) = 0.99*TTOut.TimeDelta(i-1) + 0.01 * t_delta;
%     TTOut.RtpTimeDelta(i) = 0.99*TTOut.RtpTimeDelta(i-1) + 0.01 * ts_delta;
%     TTOut.TTSDelta(i) = 0.99 * TTOut.TTSDelta(i-1) + 0.01*t_ts_delta;
    
end

for i = 1:numOutVars
    name = TTOut.Properties.VariableNames{i};
    TT.(name) = TTOut.(name);
end

if existRtpTimestamp
    for i = 1:numTsVars
        name = TTOutTs.Properties.VariableNames{i};
        TT.(name) = TTOutTs.(name);
    end
end


plotIdMap = containers.Map;
plotIdMap('TimeDelta')    = 1;
plotIdMap('RtpTimeDelta') = 2;
% plotIdMap('SizeDelta')    = 3;
plotIdMap('TTSDelta')    = 3;
plotIdMap('LastT')    = 4;
plotIdMap('yyT')    = 4;
plotIdMap('RtpTsCircle')    = 5;
plotIdMap('RtpTsCircleEst')    = 5;

% plotIdMap('yySlope')    = 0;
% plotIdMap('yySlopeK')    = 0;
% plotIdMap('yyOffsetK')    = 0;
% plotIdMap('yyTDiff')    = 0;

func_plot_table(fname, TT, plotIdMap, -1);

return;








% 估计接收端的阈值
% 思路：当前之前的时间段内，斜率变化在指定范围内，则可以更新阈值（threshold）

rows = find(~ismember(TT.LastT, '-'));
xx = TT.Timestamp(rows);
zz = TT.LastT(rows);
if(iscell(zz) == 1)
    zz = cellfun(@(x)str2double(x), zz);
end
numDatas = size(zz,1);

restartThreshold = -10.0;
yy = zeros(numDatas, 1);
yySlope = zeros(numDatas, 1);

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
    
    % 大的下降沿
    if(drop < restartThreshold)
        % 往前找指定时间范围的点
        pos = i-1;
        while (x-xx(pos)) < 3.5 && pos > 1
            pos = pos - 1;
        end
        
        % 如果有的话
        if pos > 1 && (x-xx(pos)) >= 3.5 && pos > lastDrop 
            % 找出最大最小的Slope（斜率）
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
            
            % Slope在指定范围内，更新阈值（threshold）
            if(maxy-miny) < 10 && miny > -0.2
                candidate = zz(i-1) + 2;
                if(candidate > minThreshold)
                    threshold = candidate;
                else
                    threshold = minThreshold;
                end
            end
        end
        
        yySlope(i) = 0;
        lastIndex = i;
        xx0 = xx(lastIndex);
        zz0 = zz(lastIndex);
        yySlopEst(i) = 0;
        lastDrop = i;
        
    elseif(deltaX >= 1.5)
        % 只有时间足够长才开始计算Slope
        yySlope(i) = deltaZ/deltaX;
        lastIndex = lastIndex + 1;
        xx0 = xx(lastIndex);
        zz0 = zz(lastIndex);
        yySlopEst(i) = 0.6*yySlopEst(i-1) + 0.4*yySlope(i);
    else
        % 时间不够长，Slope用之前的值
        yySlope(i) = yySlope(i-1);
        yySlopEst(i) = 0.6*yySlopEst(i-1) + 0.4*yySlope(i);
        if lastDrop == i-1
            lastDrop = i;
        end
    end
    yyThreshold(i) = threshold;
    yyEst(i) = 0.6*yyEst(i-1) + 0.4*zz(i);
    
end

% 画结果图
table4Draw = table;
table4Draw.xx = xx;
table4Draw.zz = zz;
table4Draw.yySlope = yySlope;
table4Draw.yyEst = yyEst;
table4Draw.yySlopEst = yySlopEst;
table4Draw.yyThreshold = yyThreshold;
table4Draw.yySlopeRange = yySlopMax - yySlopMin;
plotIdMap = containers.Map;
plotIdMap('zz')    = 1;
plotIdMap('yyEst')    = 1;
plotIdMap('yyThreshold')    = 1;
plotIdMap('yySlope')    = 2;
plotIdMap('yySlopEst')    = 2;
plotIdMap('yySlopeRange')    = 3;
func_plot_table(fname, table4Draw, plotIdMap, -1);



