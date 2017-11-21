% 从txt中读取数据并画图

% clear all
% clc;
% close all;

[fname, fpath] = uigetfile(...
    {'*.txt', '*.*'}, ...
    'Pick a file');

file4read = fullfile(fpath, fname);
TT = readtable(fullfile(fpath, fname), 'Delimiter',' ');
% TT = textread(file4read, '%s', 'delimiter', '\n');
size(TT.Properties.VariableNames,2)


x = load(fullfile(fpath, fname));
x0 = x(:,1);
x0base = x0(1);
for i=1:size(x0, 1)
    x0(i) = x0(i)-x0base;
end

m = size(x,2);
color = char('-', 'k-', 'r-', 'b-', 'g-', 'c-', 'm-', 'y-', 'r-', 'b-', 'g-', 'c-', 'm-', 'y-');
figure;
sub_flags = zeros(m-1);
for i=1:m-1
  sub_flags(i)=i;
end
% labels = {'RTT', 'Loss', 'K', 'Bitrate', 'Stable', 'Remote'};

% labels = {'RTT', 'Loss', 'Jit', 'Remote', 'Local', 'RTT-Diff', 'Loss-Diff', 'Jit-Diff', 'K', 'Rtt-Var', 'Loss-Var', 'Jit-Var', 'Action'};
% sub_flags = [1, 2, 3, 4, 4, 1, 2, 3, 5, 6, 6, 6, 6];

labels = {'RTT', 'Loss', 'Jit', 'Remote', 'Local', 'LowTh', 'HighTh', 'K', 'Action'};
sub_flags = [1, 2, 3, 4, 4, 3, 3, 5, 5];

maxm = 1;
for i=1:m-1
  if sub_flags(i) > maxm 
      maxm = sub_flags(i);
  end
end

m = size(sub_flags,2)+1;

for i=2:m
  % disp(color(i,:));
  % plotId = i-1;
  plotId = sub_flags(i-1);
  subplot(maxm,1,plotId);
  out = x(:,i);
%   H1 = plot(x0, out, color(i,:));
  H1 = plot(x0, out);
  legend(H1, labels(i-1));
  hold on;
end



