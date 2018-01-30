function filter = kalman(filter)
  %predict
  predict_x = filter.A * filter.x + filter.B * filter.u;
  filter.P = filter.A * filter.P * filter.A' + filter.Q;
  %correct
  filter.K = filter.P * filter.H' / (filter.H * filter.P * filter.H' + filter.R);
  filter.x = predict_x + filter.K * (filter.z - filter.H * predict_x);
  filter.P = filter.P - filter.K * filter.H * filter.P;
end

