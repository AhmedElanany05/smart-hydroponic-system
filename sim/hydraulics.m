function h = hydraulics(p, emitter_D)
%HYDRAULICS  Operating point of the main pump feeding N modules.
%   The riser (Part6 segments joined by Part7 connectors) is a vertical
%   manifold. Each module has 8 side emitters (orifices) at the same height.
%   Pump curve:   H = Hmax * (1 - (Q/Qmax)^2)
%   Emitter flow: q = Cd * A * sqrt(2 g P)  (P = local pressure head)
%   Solved for the total flow Q where all water leaves through the emitters.
if nargin < 2, emitter_D = p.emitter_D; end
N   = p.N;  n_e = p.pockets_per_module;
g   = p.g;  D = p.riser_ID;  A_r = pi*D^2/4;
A_e = pi*emitter_D^2/4;
Qmax = p.pump.Qmax/1000/3600;                       % m3/s

z_base = p.tank_h + p.lid_h;
z_e    = z_base + (0:N-1)'*p.module_h + p.emitter_z_in_module;   % emitter heights
z_pump = p.pump_inlet_z + 0.10;                     % pump outlet (Part11 is 100 mm tall)
L_seg  = [max(p.feed_pipe_L, z_base - z_pump) + (z_e(1) - z_base); diff(z_e)];
K_seg  = [p.K_fixed; p.K_connector*ones(N-1,1)];

    function [res, P, q] = march(Q)
        Hp  = p.pump.Hmax * max(1 - (Q/Qmax)^2, 0);
        Hd  = p.water_level + Hp;                   % total head at pump outlet
        P = zeros(N,1); q = zeros(N,1); Qs = Q; E = Hd;
        for i = 1:N
            v  = Qs / A_r;
            E  = E - friction(v, D, L_seg(i), p.nu)*v^2/(2*g) - K_seg(i)*v^2/(2*g);
            P(i) = E - z_e(i) - v^2/(2*g);
            q(i) = n_e * p.emitter_Cd * A_e * sqrt(2*g*max(P(i),0));
            Qs = max(Qs - q(i), 0);
        end
        res = Q - sum(q);
    end

Q = fzero(@(x) march(x), [1e-9, Qmax*0.999]);
[~, P, q] = march(Q);

h.Q_Lph          = Q*1000*3600;
h.module_Lph     = q*1000*3600;
h.pocket_Lph     = h.module_Lph / n_e;
h.P_head_m       = P;
h.z_emitter      = z_e;
h.pump_head_m    = p.pump.Hmax * (1 - (Q/Qmax)^2);
h.static_head_m  = z_e(end) - p.water_level;
h.uniformity     = min(h.module_Lph) / max(h.module_Lph);
h.ok             = min(h.pocket_Lph) >= p.min_pocket_Lph;
h.emitter_D_mm   = emitter_D*1000;
end

function k = friction(v, D, L, nu)
% Darcy-Weisbach loss coefficient f*L/D (laminar or Blasius smooth pipe).
Re = max(v*D/nu, 1);
if Re < 2300, f = 64/Re; else, f = 0.316*Re^-0.25; end
k = f*L/D;
end
