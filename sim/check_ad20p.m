%CHECK_AD20P  Re-run hydraulics and energy with the real main pump (AD20P-1230A).
%   Label: 12 V DC, 4.8 W, Hmax 3 m, Qmax 240 L/h, IP68.
here = fileparts(mfilename('fullpath'));  cd(here);
w = load_pvgis(fullfile('data', 'cairo_pvgis_2023_tilt30.csv'));

fprintf('--- Hydraulics, AD20P-1230A (Hmax 3 m, 240 L/h, 4.8 W)\n');
for N = [1 3 5 8 9 10]
    p = tower_params(N);  p.pump.Hmax = 3;  p.pump.Qmax = 240;  p.pump.P_W = 4.8;
    t = topfeed(p);
    fprintf('N=%2d  lift %.2f m  flow to the top %5.1f L/h  pocket %.1f L/h  ok=%d\n', ...
        N, t.static_m, t.Q_Lph, min(t.pocket_Lph), t.ok);
end

fprintf('--- Energy, N = 5, 50 W panel + 18 Ah battery, all sensors (1.1 W)\n');
names = {'old assumption (19 W pump)', 'AD20P (4.8 W pump)'};
for k = 1:2
    p = tower_params(5);  p.load.esp_W = 1.1;
    if k == 2, p.pump.Hmax = 3;  p.pump.Qmax = 240;  p.pump.P_W = 4.8; end
    r = sim_energy(p, w, p.panels(2), p.batteries(3));
    L = sum(r.load_Wh) / max(w.day);
    fprintf('%-28s load %3.0f Wh/day (main pump %3.0f) | PV %3.0f | min SOC %2.0f%% | pump %.2f%% | Jetson %.1f%%\n', ...
        names{k}, r.daily_load_Wh, L(2), r.daily_pv_Wh, 100*r.min_soc, 100*r.avail(2), 100*r.avail(5));
end
