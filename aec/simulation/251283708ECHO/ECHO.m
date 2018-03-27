%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%% ECHO CANCELLATION SIMULLATION  %%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
clear all;
mu = .01;    %Larger values for fast conv
max_run = 200;
for run=1:max_run;
taps = 20;   %Adaptive Filter order 
freq = 2000; %Signal Freq
w = zeros(1,taps); %initial state of adaptive filter
time = .2; %lenght of simulation (sec)
samplerate = 8000;%samples/sec
samples = time*samplerate;
max_iterations = samples-taps+1;
iterations = 1:max_iterations;%Vector of iterations
t=1/samplerate:1/samplerate:time;
noise=.02*rand(1,samples);%noise added to signal
s=.4*sin(2*pi*freq*t);%Pure Signal
x=noise+s;%input to adaptive filter
echo_amp_per = .4; %Echo percent of signal

echo_time_delay = .05;
echo_delay=echo_time_delay*samplerate;
echo = echo_amp_per*[zeros(1,echo_delay) x(echo_delay+1:samples)];
%Adaptive Algorithm using LMS
for i=1:max_iterations;
   y(i)=w*x(i:i+taps-1)';    
   e(run,i)=echo(i)-y(i);
   w = w + 2*mu*e(run,i)*x(i:i+taps-1);
end
end
%%Mean Square Error
mse=sum(e.^2,1)/max_run;
b=x+echo;
%Ouput of System 
out=b(1:length(y))-y;
subplot(3,1,1),plot(b);
title('Signal and Echo');
ylabel('Amp');
xlabel('Time sec');
subplot(3,1,2),plot(out);
title('Output of System');
ylabel('Amp');
xlabel('Time sec');
subplot(3,1,3),semilogy(mse);
grid
title('LEARNING CURVE mu=.01 echo delay=64ms runs=200');
ylabel('Estimated MSE, dB');
xlabel('Number of Iterations');

