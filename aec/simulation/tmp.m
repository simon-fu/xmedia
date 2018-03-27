
rng default
Fs = 1000;
t = 0:1/Fs:1-1/Fs;
% x = cos(2*pi*100*t) + randn(size(t));
x = 0.3+cos(2*pi*100*t);
% x = x - mean(x);

figure;
plot(t,x);

[psd,power]=periodogram(x,rectwin(length(x)),length(x),Fs);
freq = 0:Fs/length(x):Fs/2;

figure;
plot(freq, psd);

figure;
plot(freq,10*log10(psd));
figure;
periodogram(x,rectwin(length(x)),length(x),Fs);
% гнгнгнгнгнгнгнгнгн




