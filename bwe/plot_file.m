% 从txt中读取数据并画图

clear all
clc;
close all;

[fname, fpath] = uigetfile(...
    {'*.txt', '*.*'}, ...
    'Pick a file');

x = load(fullfile(fpath, fname));
m = size(x,2);
color = char('-', 'k-', 'r-', 'b-', 'g-');
for i=2:m
  % disp(color(i,:));
  out = x(:,i);
  H1 = plot(out, color(i,:));
  hold on;
end
