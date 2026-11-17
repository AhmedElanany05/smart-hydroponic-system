function T = handcalc_structure(Ns, V_wind, E)
%HANDCALC_STRUCTURE  Hand calculations to check the ANSYS results.
%   T = handcalc_structure([3 5 7], [15 30], 0.8e9)
%   The stacked tubes are treated as one cantilever fixed at the tank lid.
%   Prints wind force, top deflection, bending stress, first natural
%   frequency and the tipping safety factor of the whole tower on its tank.
if nargin < 1, Ns = [3 5 7]; end
if nargin < 2, V_wind = [15 30]; end
if nargin < 3, E = 0.8e9; end                 % PP at ~50 °C (summer sun)

p   = tower_params(1);
Do  = 0.167;  Di = 0.160;                      % Part1 tube (CAD)
I   = pi/64*(Do^4 - Di^4);
A_t = pi/4*(Do^2 - Di^2);
rhoPP   = 905;                                 % kg/m3
m_pocket_load = 0.25;                          % kg, wet medium + plant per pocket
m_tray_water  = 0.27;                          % kg, water standing on each tray
m_module_plastic = 1.6;                        % kg, tube + 8 pockets + tray + coupler (estimate)
A_proj  = Do*p.module_h + 0.01;                % m2 per module, tube + pockets
Cd      = 1.2;  rho_air = 1.20;
m_tank  = 2.0 + p.tank_L*p.tank_W*p.water_level*1000;   % tank + water (kg)
lever   = min(p.tank_L, p.tank_W)/2;          % tipping about the narrow edge

rows = {};
for N = Ns
    L    = N*p.module_h;
    m_mod = m_module_plastic + 8*m_pocket_load + m_tray_water;
    mL   = m_mod/p.module_h;                   % kg/m along the tower
    f1   = 1.875^2/(2*pi) * sqrt(E*I/(mL*L^4));
    W_tower = N*m_mod*9.81;
    sig_dead = W_tower / A_t / 1e6;
    for V = V_wind
        q    = 0.5*rho_air*V^2;
        F    = q*Cd*A_proj;                    % N per module
        w    = F/p.module_h;
        delta = w*L^4/(8*E*I);
        M    = w*L^2/2;
        sig  = M*(Do/2)/I/1e6;
        h_c  = p.tank_h + p.lid_h + L/2;        % height of the wind resultant
        M_over = N*F*h_c;
        M_rest = (W_tower + m_tank*9.81)*lever;
        rows(end+1,:) = {N, V, N*F, 1000*delta, sig, sig_dead, f1, 0.2*V/Do, M_rest/M_over}; %#ok<AGROW>
    end
end
T = cell2table(rows, 'VariableNames', {'N','V_ms','F_total_N','top_defl_mm', ...
    'bend_stress_MPa','dead_stress_MPa','f1_Hz','vortex_Hz','tip_safety'});
disp(T);
fprintf(['Notes: bending stress is for the plain tube; ANSYS will show 2-3x higher\n' ...
         'peaks around the pocket openings. tip_safety < 1.5 means the tower needs anchoring.\n']);
end
