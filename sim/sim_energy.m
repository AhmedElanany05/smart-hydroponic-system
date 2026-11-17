function r = sim_energy(p, w, panel, batt)
%SIM_ENERGY  Hourly energy balance for one year (Cairo weather w).
%   panel : struct with field W     (rated power)
%   batt  : struct with fields chem ('PbA'|'LFP') and Ah
%   Loads are shed in priority order when the battery reaches its minimum
%   state of charge: Jetson -> air pump -> dosing -> main pump -> ESP32.
c   = p.chem.(batt.chem);
Cwh = c.Vnom * batt.Ah;                 % battery energy capacity
n   = numel(w.G);
nd  = max(w.day);

%% PV power delivered to the battery bus (W)
Tc  = w.T + w.G * (p.pv.NOCT - 20) / 800;
Imp = panel.W / p.pv.Vmp;
if strcmp(p.pv.controller, 'MPPT')
    Ppv_fn = @(soc, k) panel.W * w.G(k)/1000 * (1 + p.pv.gamma_P*(Tc(k)-25)) * p.pv.derate * p.pv.mppt_eff;
else  % PWM: panel current at battery voltage
    Ppv_fn = @(soc, k) (c.V0 + (c.Vch - c.V0)*soc) * Imp * p.pv.Isc_over_Imp * ...
                       w.G(k)/1000 * (1 + p.pv.alpha_I*(Tc(k)-25)) * p.pv.derate;
end

%% Daily water use (scaled by sunshine) -> dosing time and Jetson schedule
Gday  = accumarray(w.day, w.G);
sunny = Gday / mean(Gday);
dose_h_day = p.load.dose_min_per_plant_day * p.plants / 60 .* sunny;   % pump-hours per day

check_h = zeros(n,1);                    % Jetson hours of operation per hour slot
t_check = (p.sched.jetson_boot_min + p.sched.jetson_min_per_module * p.N) / 60;
for d = 1:nd
    idx = find(w.day == d & w.sun >= p.sched.jetson_min_sun);
    if isempty(idx), continue; end
    if p.sched.jetson_always_on
        check_h(w.day == d & w.sun > 0) = 1;
    else
        pick = idx(round(linspace(1, numel(idx), min(p.sched.jetson_checks, numel(idx)))));
        check_h(pick) = check_h(pick) + t_check;
    end
end

%% Hour-by-hour simulation
soc = 0.9;  SOC = zeros(n,1);
L = zeros(n,5);  Lserved = L;            % [ESP32 main air dose jetson] in Wh
Epv = zeros(n,1); Ecurt = Epv;
for k = 1:n
    day = w.sun(k) > 0;
    pump_duty = day*p.sched.pump_duty_day + ~day*p.sched.pump_duty_night;
    L(k,:) = [p.load.esp_W, ...
              (p.pump.P_W + p.load.relay_W) * pump_duty, ...
              (p.load.air_W + p.load.relay_W) * (day*p.sched.air_duty_day + ~day*p.sched.air_duty_night), ...
              (p.load.dose_W + p.load.relay_W) * dose_h_day(w.day(k)) / 24, ...
              (p.load.jetson_W + p.load.relay_W) * check_h(k)];
    pv  = max(Ppv_fn(soc, k), 0);
    srv = L(k,:);
    % shed loads if the battery cannot cover the deficit
    for shed = [5 3 4 2 1]
        deficit = sum(srv) - pv;
        if deficit <= 0 || soc - deficit/c.eta_d/Cwh >= c.soc_min, break; end
        avail = max((soc - c.soc_min)*Cwh*c.eta_d, 0) + pv;
        srv(shed) = max(0, srv(shed) - (sum(srv) - avail));
    end
    net = pv - sum(srv);
    if net >= 0
        acc  = c.c_rate_max * batt.Ah * c.Vnom;          % max charge power
        if soc > c.taper_from, acc = acc * (1 - soc)/(1 - c.taper_from); end
        chg  = min(net, acc);
        soc  = min(soc + chg*c.eta_c/Cwh, 1);
        Ecurt(k) = net - chg;
    else
        soc  = max(soc + net/c.eta_d/Cwh, 0);
    end
    Epv(k) = pv;  Lserved(k,:) = srv;  SOC(k) = soc;
end

%% Results
r.SOC          = SOC;
r.load_Wh      = L;
r.served_Wh    = Lserved;
r.pv_Wh        = Epv;
r.curtailed_Wh = Ecurt;
r.unserved_Wh  = sum(L - Lserved);                       % per load, per year
r.avail        = 1 - r.unserved_Wh ./ max(sum(L), eps);  % energy availability per load
r.critical_ok  = r.unserved_Wh(1) < 1 && r.unserved_Wh(2) < 1;
r.daily_load_Wh = sum(L(:)) / nd;
r.daily_pv_Wh   = sum(Epv) / nd;
dmin = accumarray(w.day, SOC, [], @min);  dmax = accumarray(w.day, SOC, [], @max);
r.mean_daily_DoD = mean(dmax - dmin);
r.min_soc        = min(SOC);
end
