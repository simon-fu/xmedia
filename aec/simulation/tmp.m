
close all; clear all;
N=8000;
k=1:N;
a=N/4;
b=N/2;
c=3*N/4;
y(1:a)=sin(1*(1:a));
y((a+1):b)=0.5*sin(1*(a+1:b));
y((b+1):c)=5*sin(1*((b+1):c));
y((c+1):N)=2*sin(1*((c+1):N));

figure(1)
subplot(3,1,1);
plot(k,y);
a=zeros(1,N+1);
	a(1)=1;
d=2;
u=0.01;
for m=1:N
	z(m)=a(m)*y(m);
	a(m+1)=a(m)+u*(d-abs(z(m)));
end
% figure(2)
subplot(3,1,2);
plot(k,z);
grid on;
% figure(3)
subplot(3,1,3);
plot(k,a(1:N));
grid on;