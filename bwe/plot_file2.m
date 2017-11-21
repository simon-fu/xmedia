% 从txt中读取数据并画图
% 数据格式：第一列是横坐标，其他列为曲线数据，曲线数据为‘-’代表这行没有数据
% 一个文件例子：
% timestamp rtt loss
% 100 200 100
% 200 - 10
% 300 300 150

clear all
% clc;
% close all;

% plotIds 的维数必须和曲线个数一致
plotIds = [1, 1, 1, 2, 1, 1, 1, 1];

[fname, fpath] = uigetfile(...
    {'*.txt', '*.*'}, ...
    'Pick a file');
if(~ischar(fname))
    disp 'wrong filename';
    return;
end
file4read = fullfile(fpath, fname);
TT = readtable(fullfile(fpath, fname), 'Delimiter',' ', 'FileEncoding', 'UTF-8');

% numVar=曲线个数+1
numVar = size(TT.Properties.VariableNames,2);

% 如果没有 plotIds，生成默认的
if(~exist('plotIds','var'))
    plotIds = 1:1:numVar-1;
end

% 检查 plotIds 个数是否正确
numIds = size(plotIds, 2);
if(numIds == 0)
    plotIds = 1:1:numVar-1;
    numIds = size(plotIds, 2);
elseif(numIds ~= (numVar-1))
    disp 'wrong size of plotIds. should be:';
    disp (numVar-1);
    return;
end

% 计算最大图数
maxPlotId = 1;
for i=1:numIds
  if plotIds(i) > maxPlotId 
      maxPlotId = plotIds(i);
  end
end

% ynames保存每一个图里的所有曲线名字
ynames = cell(1, maxPlotId);
for i=1:numIds
  nId = plotIds(i);
  tmp = ynames{nId};
  tmp{end+1}=TT.Properties.VariableNames{i+1};
  ynames{nId} = tmp;
end

% numCounters保存每一个图里的已画曲线计数
numCounters = zeros(1, maxPlotId);
for i=2:numVar
    % 取出第i个变量中所有不是'-'的行
    rows = find(~ismember(TT.(i), '-'));
    pairTable = TT(rows,[1,i]);
    if(iscell(pairTable.(2)) == 1)
        % 如果是字符串则转成double
        y = cellfun(@(x)str2double(x), pairTable.(2));
    else
        y = pairTable.(2);
    end
    
    x = pairTable.(1);
%     yname = pairTable.Properties.VariableNames{2};
    
    plotId = plotIds(i-1);
    subplot(maxPlotId,1,plotId);
    H1 = plot(x, y);
    
    % 当前图里的所有曲线都已经画完，显示曲线名称
    numCounters(plotId) = numCounters(plotId)+1;
    if(numCounters(plotId) >= size(ynames{plotId},2))
        lgd=legend(ynames{plotId}, 'Location','northeast'); % 'Orientation','horizontal'
        lgd.FontSize = 12;
        lgd.TextColor = 'blue';
    end
    hold on;
end

