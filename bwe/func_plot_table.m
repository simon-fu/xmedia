function [ TT ] = func_plot_table(fname, TT, plotIdMap, defaultPlotId )
%UNTITLED Summary of this function goes here
%   Detailed explanation goes here

% file4plot = fullfile(fpath, fname);
% TT = readtable(file4plot, 'Delimiter',',', 'FileEncoding', 'UTF-8');

% 转换成时间相对值
% starttime = TT.(1)(1);
% TT.(1) = TT.(1)-starttime; % 单位相对毫秒
% TT.(1) = TT.(1)/1000.0; % 单位为秒

% numVar=曲线个数+1
numVar = size(TT.Properties.VariableNames,2);



% 若没有指定plotId，赋予默认Id
% 计算最大图数
maxPlotId = 0;
for i=2:numVar
    name = TT.Properties.VariableNames{i};
    if ~plotIdMap.isKey(name)
        plotIdMap(name) = defaultPlotId;      
    end
    if plotIdMap(name) > maxPlotId
        maxPlotId = plotIdMap(name);
    end
end
% plotId等于0的重新赋予新的plotId
for i=2:numVar
  name = TT.Properties.VariableNames{i};
  if plotIdMap(name) == 0
      maxPlotId = maxPlotId + 1;
      plotIdMap(name) = maxPlotId;
  end
end

ynameMap = containers.Map('KeyType', 'int32', 'ValueType', 'any');
for i=2:numVar
  name = TT.Properties.VariableNames{i};
  plotId = plotIdMap(name);
  if plotId < 0
      continue;
  end
  
  if ~ynameMap.isKey(plotId)
      ynameMap(plotId) = cell(0);
  end
  tmp = ynameMap(plotId);
  tmp{end+1} = name;
  ynameMap(plotId) = tmp;
end


% numCounters保存每一个图里的已画曲线计数
numCounters = zeros(1, maxPlotId);
figure('NumberTitle', 'off', 'Name', fname);
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
    
%     x = rows;
    x = pairTable.(1);
    yname = pairTable.Properties.VariableNames{2};
    plotId = plotIdMap(yname);
    
    if plotId < 0
        continue;
    end
    
    subplot(maxPlotId,1,plotId);
    if(~isempty(y))
        H1 = plot(x, y);
    end
    
%     names = ynames{plotId};
    names = ynameMap(plotId);

    % 当前图里的所有曲线都已经画完，显示曲线名称
    numCounters(plotId) = numCounters(plotId)+1;
    if(numCounters(plotId) >= size(names,2))
        lgd=legend(names, 'Location','northeast'); % 'Orientation','horizontal'
        if(~isempty(lgd))
            lgd.FontSize = 12;
            lgd.TextColor = 'blue';
        end
    end
    hold on;
    grid on;
end

% 修改图表光标显示精度
dcmObj = datacursormode; 
set(dcmObj,'UpdateFcn',@updateFcn);

end

