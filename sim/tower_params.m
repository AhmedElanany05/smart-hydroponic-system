function p = tower_params(N)
%TOWER_PARAMS  All parameters of the solar hydroponic tower for N stacked modules.
%   Values marked (CAD) come from the SolidWorks STEP files, (shop) from the
%   Egyptian shopping list (Oct 2026), (assumed) are engineering estimates
%   that should be replaced with datasheet / measured values when available.
if nargin < 1, N = 5; end

%% Modules (CAD)
p.N                 = N;
p.pockets_per_module = 8;
p.plants            = p.pockets_per_module * N;
p.module_h          = 0.300;   % m, Part1 tube height
p.tank_h            = 0.300;   % m, Part3 tank height
p.lid_h             = 0.034;   % m, part4 lid
p.tank_L            = 0.400;   % m
p.tank_W            = 0.300;   % m

%% Hydraulics
p.water_level       = 0.20;    % m above tank bottom (normal level)       (assumed)
p.water_min_level   = 0.05;    % m, pump must stay submerged              (assumed)
p.pump_inlet_z      = 0.03;    % m above tank bottom                     (assumed)
p.riser_ID          = 0.016;   % m, Part6 / part12 inner diameter         (CAD)
p.feed_pipe_L       = 0.202;   % m, part12 pump-to-tower pipe             (CAD)
p.emitter_D         = 0.0035;  % m, Part6 side outlet bore (Ø3.5 mm)      (CAD)
p.emitter_Cd        = 0.62;    % orifice discharge coefficient            (assumed)
p.emitter_z_in_module = 0.25;  % m, outlet height inside a module         (assumed)
p.K_connector       = 0.5;     % minor loss per Part7 connector           (assumed)
p.K_fixed           = 2.0;     % pump outlet + elbow + entry losses       (assumed)
p.min_pocket_Lph    = 1.0;     % L/h per pocket to keep medium wet        (assumed)
p.rho = 998; p.g = 9.81; p.nu = 1.0e-6;

% Top-feed drip tray "edit 1" (CAD, redesign of 12 Oct 2026)
p.tray.n_holes   = 8;        % one above each pocket
p.tray.hole_D    = 0.003;    % m, Ø3 mm holes                        (CAD)
p.tray.hole_r    = 0.070;    % m, holes on a 70 mm radius            (CAD)
p.tray.depth_max = 0.016;    % m, water depth before the central collar overflows (CAD)
p.tray.Cd        = 0.62;     % sharp-edged hole                      (assumed)

% Main pump: AD20P-1230A, 12 V brushless, values from the pump label.
p.pump.name   = 'AD20P-1230A 12V brushless';
p.pump.Hmax   = 3.0;    % m      (label)
p.pump.Qmax   = 240;    % L/h    (label)
p.pump.P_W    = 4.8;    % W      (label)

%% Water use
p.water_L_per_plant_day = 0.35;  % annual mean, scaled by daily sun      (assumed)
p.tank_usable_L = p.tank_L * p.tank_W * (p.water_level - p.water_min_level) * 1000;

%% Electrical loads (W at the 12 V bus)
p.load.esp_W        = 1.1;   % ESP32 + pH, TDS, HC-SR04, DHT11, soil via buck (assumed)
p.load.air_W        = 0;     % no air pump in the built system
p.load.dose_W       = 5.0;   % one peristaltic nutrient pump (A or B)   (assumed)
p.load.dose_min_per_plant_day = 0.05;  % dosing run time scales with plants (assumed)
p.load.jetson_W     = 12.0;  % Jetson in 15 W mode + webcam, fed 12 V directly (assumed)
p.load.relay_W      = 0;     % pumps switched by MOSFETs, no relay coils

%% Schedules
p.sched.pump_duty_day   = 0.25;  % 15 min per hour while the sun is up
p.sched.pump_duty_night = 0.04;  % night eco mode: 5 min every 2 hours
p.sched.air_duty_day    = 0.25;  % 15 min per hour while the sun is up
p.sched.air_duty_night  = 0.10;  % night eco mode: 6 min per hour
p.sched.jetson_checks   = 6;     % plant checks per day (daylight only)
p.sched.jetson_min_sun  = 15;    % deg, only check when the sun is this high
p.sched.jetson_boot_min = 1.5;   % boot + shutdown time per check
p.sched.jetson_min_per_module = 0.5;  % image capture + inference per module
p.sched.jetson_always_on = false;     % true = Jetson on all daylight (comparison)

%% PV (18 V nominal panels, Cairo, 30 deg tilt facing south)
p.pv.Vmp      = 18.0;
p.pv.Isc_over_Imp = 1.08;
p.pv.alpha_I  = 0.0005;   % 1/K, current temp. coefficient
p.pv.gamma_P  = -0.004;   % 1/K, power temp. coefficient
p.pv.NOCT     = 45;       % deg C
p.pv.derate   = 0.90;     % dust / soiling / wiring (Cairo dust)      (assumed)
p.pv.controller = 'PWM';  % 'PWM' or 'MPPT'
p.pv.mppt_eff = 0.95;

%% Candidate panels and batteries with Egyptian prices (shop, EGP)
p.panels = struct( ...
  'name',  {'10 W mono','50 W mono','60 W mono','100 W'}, ...
  'W',     {10, 50, 60, 100}, ...
  'EGP',   {479, 1699, 1850, 3018.79});
p.batteries = struct( ...
  'name',  {'7 Ah VRLA','9 Ah VRLA','2x9 Ah VRLA','12 Ah VRLA','40 Ah VRLA','4S3P LiFePO4 DIY'}, ...
  'chem',  {'PbA','PbA','PbA','PbA','PbA','LFP'}, ...
  'Ah',    {7, 9, 18, 12, 40, 12}, ...
  'EGP',   {1199, 1019.90, 2039.80, 2025, 6475, 2400 + 600});  % LFP: 12 cells + BMS/holders (assumed 600)

%% Battery chemistry models
p.chem.PbA = struct('Vnom',12.0,'eta_c',0.85,'eta_d',0.97,'soc_min',0.50, ...
                    'c_rate_max',0.25,'taper_from',0.85,'V0',11.9,'V1',12.9,'Vch',14.2);
p.chem.LFP = struct('Vnom',12.8,'eta_c',0.97,'eta_d',0.98,'soc_min',0.10, ...
                    'c_rate_max',0.50,'taper_from',0.95,'V0',12.8,'V1',13.3,'Vch',14.4);
end
