function filter = kalman(filter, n, m)
  % init
  if(~exist('filter', 'var') ||  ~isfield(filter, 'A'))
    if(~exist('filter', 'var'))
        filter = struct;
    end
    if(exist('n', 'var'))
        filter.n = n;
    end
    if(exist('m', 'var'))
        filter.m = m;
    end
    if(~isfield(filter, 'n'))
        filter.n = 1;
    end
    if(~isfield(filter, 'm'))
        filter.m = 1;
    end

    filter.A = eye(filter.n);
    filter.H = ones(filter.m, filter.n); % mxn
    filter.B = 0;
    filter.u = 0;
    filter.P = eye(filter.n); % nxn
    filter.K = zeros(filter.n, filter.m);% nxm
    filter.Q = eye(filter.n); % nxn
    filter.R = eye(filter.m); % mxm
    filter.x = zeros(filter.n, 1); % 初始状态值
    filter.z = zeros(filter.m, 1); % 初始测量值
    if isfield(filter, 'cQ')
        filter.Q = filter.Q * filter.cQ;
    end
    if isfield(filter, 'cR')
        filter.R = filter.R * filter.cR;
    end
    return ;
  end
  %predict
  filter.PX = filter.A * filter.x + filter.B * filter.u;
  filter.PP = filter.A * filter.P * filter.A' + filter.Q;
  %correct
  filter.K = filter.PP * filter.H' / (filter.H * filter.PP * filter.H' + filter.R);
  filter.x = filter.PX + filter.K * (filter.z - filter.H * filter.PX);
  filter.P = filter.PP - filter.K * filter.H * filter.PP;
end

