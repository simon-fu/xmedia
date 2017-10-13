function [ out ] = kalman_process( kfilter, z )
filter = kfilter;
len = size(z,2);
out = zeros(size(z));
for i=1:len
  filter.z = z(i);
  filter = kalman(filter);
  out(i)=[filter.x];
end

end

