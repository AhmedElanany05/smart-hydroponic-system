function t = topfeed(p, tilt_deg, hole_D)
%TOPFEED  Top-feed (gravity tray) design: the pump lifts water up the riser
%   to the top of the tower; it falls on the drip tray ("edit 1") of every
%   module and leaves through 8 holes, one above each pocket. Pocket drains
%   (Part2) carry it down to the next tray. Water that the holes cannot pass
%   overflows the central collar and continues to the tray below.
%   tilt_deg : tower tilt (deg) -> uneven water depth over the tray
%   hole_D   : tray hole diameter (m), CAD = 3 mm
if nargin < 2, tilt_deg = 0; end
if nargin < 3, hole_D = p.tray.hole_D; end
g = p.g;  D = p.riser_ID;  A_r = pi*D^2/4;
Qmax = p.pump.Qmax/1000/3600;

%% 1) Pump operating point: lift to the riser top
z_top  = p.tank_h + p.lid_h + p.N*p.module_h;          % riser outlet height
static = z_top - p.water_level;
L      = p.feed_pipe_L + p.N*p.module_h;
K      = p.K_fixed + (p.N-1)*p.K_connector + 1.0;       % + exit loss
f_loss = @(Q) loss(Q/A_r, D, L, p.nu, K, g);
Hp     = @(Q) p.pump.Hmax * (1 - (Q/Qmax)^2);
if Hp(0) <= static
    Q = 0;
else
    Q = fzero(@(x) Hp(x) - static - f_loss(x), [0, Qmax]);
end

%% 2) Tray: water depth needed so the 8 holes pass the inflow
n   = p.tray.n_holes;
phi = (0:n-1)'*2*pi/n;
dz  = p.tray.hole_r * tand(tilt_deg) * cos(phi);        % hole height offsets
A_h = pi*hole_D^2/4;
q_of_h = @(h) p.tray.Cd * A_h * sqrt(2*g*max(h - dz, 0));   % per hole, h at centre
Qholes = @(h) sum(q_of_h(h));
h_max  = p.tray.depth_max;
if Qholes(h_max) >= Q
    h = fzero(@(x) Qholes(x) - Q, [0, h_max]);
    overflow = 0;
else
    h = h_max;  overflow = Q - Qholes(h_max);
end
q = q_of_h(h);

t.Q_Lph          = Q*3.6e6;
t.static_m       = static;
t.pump_head_m    = Hp(Q);
t.tray_depth_mm  = h*1000;
t.hole_capacity_Lph = Qholes(h_max)*3.6e6;
t.pocket_Lph     = q*3.6e6;
t.overflow_Lph   = overflow*3.6e6;
t.uniformity     = min(q) / max(max(q), eps);
t.ok             = min(t.pocket_Lph) >= p.min_pocket_Lph;
end

function H = loss(v, D, L, nu, K, g)
Re = max(v*D/nu, 1);
if Re < 2300, f = 64/Re; else, f = 0.316*Re^-0.25; end
H = (f*L/D + K) * v^2/(2*g);
end
