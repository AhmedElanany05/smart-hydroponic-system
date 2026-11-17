%RUN_ALL  Runs every tower simulation and saves the thesis figures + summary.
%   Output: sim/figures/*.png and sim/results.md
clear; close all;
here = fileparts(mfilename('fullpath'));  cd(here);
if ~isfolder(fullfile(here, 'figures')), mkdir(fullfile(here, 'figures')); end
set(groot, 'defaultAxesFontSize', 11, 'defaultLineLineWidth', 1.6);
C = [0.18 0.49 0.31; 0.78 0.20 0.29; 0.85 0.54 0.04; 0.17 0.42 0.69; 0.45 0.45 0.45];

w = load_pvgis(fullfile('data', 'cairo_pvgis_2023_tilt30.csv'));
Ns = 1:8;
D_cad = 3.5e-3;  D_new = 1.0e-3;  Ds = [3.5 2.0 1.5 1.0]*1e-3;
md = {};   % lines of results.md

%% 1) Hydraulics: flow per module, CAD emitters vs recommended emitters
p5 = tower_params(5);
hA = hydraulics(p5, D_cad);  hB = hydraulics(p5, D_new);
f = figure('Position', [100 100 900 380]);
tiledlayout(1, 2, 'Padding', 'compact');
nexttile; barh(1:5, hA.pocket_Lph, 'FaceColor', C(2,:)); hold on;
xline(p5.min_pocket_Lph, '--k', 'minimum'); title('Current CAD: Ø3.5 mm outlets');
ylabel('Module (1 = bottom)'); xlabel('Flow per pocket (L/h)'); grid on;
nexttile; barh(1:5, hB.pocket_Lph, 'FaceColor', C(1,:)); hold on;
xline(p5.min_pocket_Lph, '--k', 'minimum'); title('Recommended: Ø1.0 mm outlets');
xlabel('Flow per pocket (L/h)'); grid on;
exportgraphics(f, 'figures/01_flow_per_module_N5.png', 'Resolution', 200);

% min pocket flow vs number of modules for several outlet sizes
minq = zeros(numel(Ds), numel(Ns));  unif = minq;
for i = 1:numel(Ds)
    for j = Ns
        h = hydraulics(tower_params(j), Ds(i));
        minq(i,j) = min(h.pocket_Lph);  unif(i,j) = h.uniformity;
    end
end
f = figure('Position', [100 100 900 380]);
tiledlayout(1, 2, 'Padding', 'compact');
nexttile; hold on;
for i = 1:numel(Ds), plot(Ns, minq(i,:), '-o', 'Color', C(i,:)); end
yline(p5.min_pocket_Lph, '--k', 'minimum'); set(gca, 'YScale', 'log'); ylim([0.1 200]);
xlabel('Number of modules N'); ylabel('Driest pocket (L/h)'); grid on;
legend(compose('Ø%.1f mm', Ds*1e3), 'Location', 'southwest'); title('Does one pump feed every pocket?');
nexttile; hold on;
for i = 1:numel(Ds), plot(Ns, 100*unif(i,:), '-o', 'Color', C(i,:)); end
xlabel('Number of modules N'); ylabel('Top / bottom module flow (%)'); grid on; ylim([0 105]);
title('Flow uniformity');
exportgraphics(f, 'figures/02_pump_limit_vs_modules.png', 'Resolution', 200);

Nmax = zeros(1, numel(Ds));
for i = 1:numel(Ds), k = find(minq(i,:) >= p5.min_pocket_Lph, 1, 'last'); if isempty(k), k = 0; end; Nmax(i) = k; end

md{end+1} = '## 1. Hydraulics (one JT-750 pump, Hmax 5 m, 700 L/h)';
md{end+1} = sprintf('- N = 5, CAD outlets Ø3.5 mm: total %.0f L/h, flow per module (bottom→top) %s L/h. **Modules 3–5 get no water.**', hA.Q_Lph, mat2str(round(hA.module_Lph')));
md{end+1} = sprintf('- N = 5, Ø1.0 mm outlets: total %.0f L/h, flow per module %s L/h, driest pocket %.1f L/h, uniformity %.0f %%.', hB.Q_Lph, mat2str(round(hB.module_Lph')), min(hB.pocket_Lph), 100*hB.uniformity);
md{end+1} = '- Maximum modules one pump can feed (driest pocket ≥ 1 L/h):';
for i = 1:numel(Ds), md{end+1} = sprintf('  - Ø%.1f mm outlets → **N max = %d**', Ds(i)*1e3, Nmax(i)); end

%% 2) Energy: recommended cheapest configuration, N = 5
p5.emitter_D = D_new;
pan  = p5.panels([p5.panels.W] == 50);
bat  = p5.batteries(strcmp({p5.batteries.name}, '9 Ah VRLA'));
r5   = sim_energy(p5, w, pan, bat);

f = figure('Position', [100 100 1000 520]);
tiledlayout(2, 1, 'Padding', 'compact');
for s = 1:2
    if s == 1, d0 = datetime(2023,1,8); ttl = 'Winter week (January)'; else, d0 = datetime(2023,7,9); ttl = 'Summer week (July)'; end
    k = find(w.t >= d0 & w.t < d0 + days(7));
    nexttile; yyaxis left;
    area(w.t(k), r5.pv_Wh(k), 'FaceColor', C(3,:), 'FaceAlpha', 0.35, 'EdgeColor', 'none'); hold on;
    plot(w.t(k), sum(r5.served_Wh(k,:), 2), '-', 'Color', C(4,:));
    ylabel('Power (W)'); yyaxis right; plot(w.t(k), 100*r5.SOC(k), '-', 'Color', C(1,:));
    ylabel('Battery SOC (%)'); ylim([0 105]); yline(100*p5.chem.PbA.soc_min, ':', 'min SOC');
    title(sprintf('%s: 50 W panel + 9 Ah lead-acid, N = 5', ttl)); grid on;
    legend('Solar to battery bus', 'Loads', 'SOC', 'Location', 'northwest');
end
exportgraphics(f, 'figures/03_energy_weeks_N5.png', 'Resolution', 200);

% daily energy split
Ld = sum(r5.load_Wh) / max(w.day);
f = figure('Position', [100 100 560 380]);
b = bar(categorical({'ESP32+sensors','Main pump','Air pump','Dosing','Jetson'}, ...
        {'ESP32+sensors','Main pump','Air pump','Dosing','Jetson'}), Ld, 'FaceColor', 'flat');
b.CData = C([4 1 3 2 5],:); ylabel('Wh per day'); grid on; title('Where the energy goes (N = 5)');
exportgraphics(f, 'figures/04_energy_split_N5.png', 'Resolution', 200);

% Jetson always-on comparison
pOn = p5; pOn.sched.jetson_always_on = true;
rOn = sim_energy(pOn, w, pan, bat);

md{end+1} = '';
md{end+1} = '## 2. Energy (Cairo 2023 hourly weather, 30° south tilt, PWM controller)';
md{end+1} = sprintf('- N = 5 daily load: **%.0f Wh/day** (ESP32 %.0f, main pump %.0f, air pump %.0f, dosing %.1f, Jetson %.1f).', r5.daily_load_Wh, Ld);
md{end+1} = sprintf('- 50 W panel delivers %.0f Wh/day on average.', r5.daily_pv_Wh);
md{end+1} = sprintf('- 50 W + 9 Ah lead-acid: energy availability ESP32 %.2f %%, main pump %.2f %%, air pump %.2f %%, Jetson %.2f %%. Mean daily depth of discharge %.0f %%.', 100*r5.avail([1 2 3 5]), 100*r5.mean_daily_DoD);
md{end+1} = sprintf('- If the Jetson stayed on all daylight instead of %d short checks: load rises to %.0f Wh/day and Jetson availability falls to %.1f %%.', p5.sched.jetson_checks, rOn.daily_load_Wh, 100*rOn.avail(5));

%% 3) Panel x battery sizing for N = 5 (cheapest that keeps critical loads up)
np = numel(p5.panels); nb = numel(p5.batteries);
availCrit = zeros(np, nb); availJet = availCrit; cost = availCrit;
for i = 1:np
    for j = 1:nb
        r = sim_energy(p5, w, p5.panels(i), p5.batteries(j));
        availCrit(i,j) = min(r.avail(1:2));  availJet(i,j) = r.avail(5);
        cost(i,j) = p5.panels(i).EGP + p5.batteries(j).EGP;
    end
end
feasible = availCrit >= 0.999 & availJet >= 0.98;
f = figure('Position', [100 100 900 360]);
imagesc(100*availCrit); colormap(flipud(sky(256))); clim([90 100]); cb = colorbar; cb.Label.String = 'Pump + ESP32 availability (%)';
set(gca, 'XTick', 1:nb, 'XTickLabel', {p5.batteries.name}, 'YTick', 1:np, 'YTickLabel', {p5.panels.name});
for i = 1:np, for j = 1:nb
    txt = sprintf('%.1f%%\n%s EGP', 100*availCrit(i,j), num2str(round(cost(i,j))));
    if feasible(i,j), txt = [txt char(10) '✔']; end
    text(j, i, txt, 'HorizontalAlignment', 'center', 'FontSize', 8);
end, end
title('Panel × battery sizing, N = 5 (✔ = meets availability targets)');
exportgraphics(f, 'figures/05_sizing_matrix_N5.png', 'Resolution', 200);
c = cost; c(~feasible) = inf; [cmin, ix] = min(c(:)); [ip, ib] = ind2sub(size(c), ix);
md{end+1} = '';
md{end+1} = '## 3. Cheapest panel + battery (N = 5)';
md{end+1} = '- Target: ESP32 and main pump ≥ 99.9 % of their energy over the year, Jetson ≥ 98 %.';
if isfinite(cmin)
    md{end+1} = sprintf('- **Cheapest feasible: %s + %s = %.0f EGP.**', p5.panels(ip).name, p5.batteries(ib).name, cmin);
else
    md{end+1} = '- No combination meets the targets.';
end

%% 4) Modularity sweep N = 1..8
best = strings(1, numel(Ns)); bestCost = nan(1, numel(Ns)); load_d = nan(1, numel(Ns));
water_d = nan(1, numel(Ns)); refill_days = water_d; pumps = water_d; elecCost = water_d;
fixed_EGP = 400 + 3584.36 + 1950 + 450 + 85 + 170 + 245 + 100 + 195 + 30 + 105 + 999 + 100 ...  % essentials
          + 650 + 210 + 175 + 100 + 40 + 275;                                                   % recommended
fixed_EGP = fixed_EGP - 999;   % controller is counted separately below
for j = Ns
    pj = tower_params(j); pj.emitter_D = D_new;
    pumps(j) = ceil(j / max(Nmax(end), 1));
    pj.pump.P_W = pj.pump.P_W * pumps(j);
    cc = inf;  rb = sim_energy(pj, w, pj.panels(end), pj.batteries(end));
    for i = 1:np
        for k = 1:nb
            r = sim_energy(pj, w, pj.panels(i), pj.batteries(k));
            cst = pj.panels(i).EGP + pj.batteries(k).EGP;
            if min(r.avail(1:2)) >= 0.999 && r.avail(5) >= 0.98 && cst < cc
                cc = cst; best(j) = sprintf('%s + %s', pj.panels(i).name, pj.batteries(k).name); rb = r;
            end
        end
    end
    bestCost(j) = cc;  load_d(j) = rb.daily_load_Wh;
    water_d(j)  = pj.plants * pj.water_L_per_plant_day;
    refill_days(j) = pj.tank_usable_L / water_d(j);
    elecCost(j) = fixed_EGP + 550*pumps(j) + 999 + cc;
end
f = figure('Position', [100 100 1000 620]);
tiledlayout(2, 2, 'Padding', 'compact');
nexttile; bar(Ns, load_d, 'FaceColor', C(4,:)); grid on; xlabel('Modules N'); ylabel('Wh/day'); title('Daily energy demand');
nexttile; bar(Ns, bestCost, 'FaceColor', C(3,:)); grid on; xlabel('Modules N'); ylabel('EGP'); title('Cheapest panel + battery');
nexttile; yyaxis left; plot(Ns, water_d, '-o'); ylabel('Water used (L/day)');
yyaxis right; plot(Ns, refill_days, '-s'); ylabel('Days between tank refills'); grid on; xlabel('Modules N');
title(sprintf('Water (tank %.0f L usable)', p5.tank_usable_L));
nexttile; plot(Ns, elecCost ./ (8*Ns), '-o', 'Color', C(1,:)); grid on; xlabel('Modules N'); ylabel('EGP per plant');
title('Electronics + energy cost per plant');
exportgraphics(f, 'figures/06_modularity_sweep.png', 'Resolution', 200);

md{end+1} = '';
md{end+1} = '## 4. Modularity sweep (Ø1.0 mm outlets, PWM, Cairo)';
md{end+1} = '| N | Plants | Pumps | Load (Wh/day) | Cheapest panel + battery | Water (L/day) | Refill every (days) | Electronics+energy per plant (EGP) |';
md{end+1} = '|---|---|---|---|---|---|---|---|';
for j = Ns
    md{end+1} = sprintf('| %d | %d | %d | %.0f | %s (%.0f EGP) | %.1f | %.1f | %.0f |', j, 8*j, pumps(j), load_d(j), best(j), bestCost(j), water_d(j), refill_days(j), elecCost(j)/(8*j));
end

%% 5) Redesign: top-feed drip trays (edit 1) vs original side outlets
Nt = 1:16;  Qtop = nan(size(Nt));  okT = false(size(Nt));  okS = okT;
for j = Nt
    pj = tower_params(j);  tt = topfeed(pj);
    Qtop(j) = tt.Q_Lph;  okT(j) = tt.ok;
    okS(j) = hydraulics(pj, D_cad).ok;
end
NmaxTop  = find(okT, 1, 'last');  NmaxSide = find(okS, 1, 'last');
tilts = 0:0.5:5;  uni = arrayfun(@(a) topfeed(p5, a).uniformity, tilts);
holes = (2:8)*1e-3;  cap = arrayfun(@(d) topfeed(p5, 0, d).hole_capacity_Lph, holes);

f = figure('Position', [100 100 1050 380]);
tiledlayout(1, 3, 'Padding', 'compact');
nexttile; hold on;
bar(Nt, Qtop, 'FaceColor', C(1,:));
yline(8*p5.min_pocket_Lph, '--k', 'min. for 8 pockets');
xlabel('Number of modules N'); ylabel('Water reaching the top (L/h)'); grid on;
title(sprintf('Top-feed: one pump feeds up to N = %d', NmaxTop));
nexttile;
b = bar(categorical({'Original (side outlets Ø3.5)', 'Redesign (top-feed trays)'}), [NmaxSide NmaxTop], 'FaceColor', 'flat');
b.CData = C([2 1],:); ylabel('Max modules on one pump'); grid on; title('Modularity limit: before vs after');
text(1:2, [NmaxSide NmaxTop], string([NmaxSide NmaxTop]), 'HorizontalAlignment', 'center', 'VerticalAlignment', 'bottom', 'FontWeight', 'bold');
nexttile; plot(tilts, 100*uni, '-o', 'Color', C(4,:)); grid on;
xlabel('Tower tilt (deg)'); ylabel('Driest / wettest pocket (%)'); ylim([50 102]);
title('Tray evenness vs tilt');
exportgraphics(f, 'figures/07_topfeed_redesign.png', 'Resolution', 200);

t5 = topfeed(p5);
md{end+1} = '';
md{end+1} = '## 5. Redesign: top-feed drip trays (CAD of 12 Oct 2026)';
md{end+1} = sprintf('- Each tray has 8 × Ø%.0f mm holes on a %.0f mm radius; water can stand %.0f mm deep before overflowing the central collar.', p5.tray.hole_D*1e3, p5.tray.hole_r*1e3, p5.tray.depth_max*1e3);
md{end+1} = sprintf('- N = 5: the pump lifts %.0f L/h to the top (static lift %.2f m). Every pocket gets **%.1f L/h, identical in all modules**; %.0f L/h overflows the collar to the next tray.', t5.Q_Lph, t5.static_m, min(t5.pocket_Lph), t5.overflow_Lph);
md{end+1} = sprintf('- **Max modules on one JT-750: original design %d → redesign %d.**', NmaxSide, NmaxTop);
md{end+1} = sprintf('- Tilt sensitivity: 1° → %.0f %%, 2° → %.0f %%, 3° → %.0f %% (driest/wettest pocket). Keep the tower within ~2° of vertical.', 100*uni(tilts==1), 100*uni(tilts==2), 100*uni(tilts==3));
md{end+1} = sprintf('- Tray hole capacity at full depth: %s L/h for Ø%s mm holes.', mat2str(round(cap)), mat2str(holes*1e3));

%% Assumptions
md{end+1} = '';
md{end+1} = '## Assumptions to replace with real data';
md{end+1} = '- Pump JT-750: max head 5 m and 19 W are typical values, not from the datasheet.';
md{end+1} = '- Air pump 5 W, ESP32 + sensors 0.8 W, Jetson 12.2 W at 12 V incl. converter losses.';
md{end+1} = '- Plant water use 0.35 L/plant/day (annual mean), scaled by daily sunshine.';
md{end+1} = '- Orifice discharge coefficient 0.62; minimum 1 L/h per pocket.';
md{end+1} = '- Weather: PVGIS-SARAH3 hourly data for Cairo (30.044 N, 31.236 E), year 2023.';

hdr = {'# Simulation results: solar hydroponic tower', '', ...
       sprintf('Generated by `sim/run_all.m` on %s.', string(datetime('now', 'Format', 'yyyy-MM-dd HH:mm'))), ''};
fid = fopen('results.md', 'w', 'n', 'UTF-8'); fprintf(fid, '%s\n', hdr{:}, md{:}); fclose(fid);
fprintf('%s\n', md{:});
