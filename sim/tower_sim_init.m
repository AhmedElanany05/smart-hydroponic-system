%TOWER_SIM_INIT  Prepares the base workspace for tower_system.slx.
%   Runs automatically before every simulation (model InitFcn).
%   Change these variables in the Command Window before pressing Run:
%     N_modules, sim_start, sim_days, P_panel_W, P_bat_Ah, P_autorefill
if ~exist('N_modules', 'var'),    N_modules = 5; end
if ~exist('sim_start', 'var'),    sim_start = datetime(2023, 1, 8); end   % winter week = worst case
if ~exist('sim_days', 'var'),     sim_days = 14; end
if ~exist('P_panel_W', 'var'),    P_panel_W = 50; end
if ~exist('P_bat_Ah', 'var'),     P_bat_Ah = 18; end                      % Ultracell UL18-12, 18 Ah lead-acid
if ~exist('P_autorefill', 'var'), P_autorefill = 0; end                   % 0 = tank refilled by hand (no valve in the build)

if ~exist('cairo_w', 'var')
    cairo_w = load_pvgis(fullfile(fileparts(which('tower_sim_init')), 'data', 'cairo_pvgis_2023_tilt30.csv'));
end
k_ = cairo_w.t >= sim_start - hours(1) & cairo_w.t <= sim_start + days(sim_days) + hours(1);
t_ = seconds(cairo_w.t(k_) - sim_start);
ws_G   = timeseries(cairo_w.G(k_),   t_);
ws_T   = timeseries(cairo_w.T(k_),   t_);
ws_sun = timeseries(cairo_w.sun(k_), t_);

p_  = tower_params(N_modules);
tf_ = topfeed(p_);
P_plants  = p_.plants;
P_Qtop    = tf_.Q_Lph;
P_qpocket = min(tf_.pocket_Lph);
P_Gmean   = mean(cairo_w.G);
P_Vmax    = p_.tank_usable_L;
clear k_ t_ p_ tf_
