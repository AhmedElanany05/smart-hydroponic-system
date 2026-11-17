%PROTECTION_TEST  Battery protection (load shedding) test with tower_system.slx.
%   Runs 14 winter days twice: the chosen power system (50 W, 2x9 Ah) and an
%   undersized one (10 W, 9 Ah). Counts Jetson checks per day and night-time
%   pump runs, and saves figures/11_protection_test.png.
here = fileparts(mfilename('fullpath'));  cd(here);
cases = {'Chosen system: 50 W panel, 18 Ah battery', 50, 18;
         'Undersized system: 10 W panel, 9 Ah battery', 10, 9};
R = struct();
for c = 1:2
    clear P_panel_W P_bat_Ah N_modules sim_start sim_days P_autorefill
    P_panel_W = cases{c,2};  P_bat_Ah = cases{c,3};
    assignin('base', 'P_panel_W', P_panel_W);  assignin('base', 'P_bat_Ah', P_bat_Ah);
    evalin('base', 'clear N_modules sim_start sim_days P_autorefill');
    out = sim('tower_system');
    t   = out.sim_SOC.Time;
    L   = squeeze(out.sim_Lsrv.Data);  if size(L,1) == 5 && size(L,2) ~= 5, L = L.'; end
    soc = squeeze(out.sim_SOC.Data);
    sun = interp1(evalin('base','ws_sun.Time'), evalin('base','ws_sun.Data'), t, 'linear', 'extrap');
    day = floor(t/86400) + 1;  nd = floor(t(end)/86400);   % full days only
    jetOn  = L(:,5) > 0.1;  pumpOn = L(:,2) > 0.1;
    jetStart  = [false; diff(jetOn) > 0];
    pumpStart = [false; diff(pumpOn) > 0];
    R(c).name   = cases{c,1};
    R(c).t      = evalin('base','sim_start') + seconds(t);
    R(c).soc    = soc;
    R(c).jetPerDay   = accumarray(day(day<=nd), jetStart(day<=nd), [nd 1]);
    R(c).nightPumps  = accumarray(day(day<=nd), pumpStart(day<=nd) & sun(day<=nd) <= 0, [nd 1]);
    R(c).minSOC = min(soc);
    fprintf('%s\n  min SOC %.0f %%, Jetson checks/day mean %.1f (min %d), night pump runs/day mean %.1f\n', ...
        R(c).name, 100*R(c).minSOC, mean(R(c).jetPerDay), min(R(c).jetPerDay), mean(R(c).nightPumps));
end
evalin('base', 'clear P_panel_W P_bat_Ah');

C = [0.18 0.49 0.31; 0.78 0.20 0.29];
f = figure('Position', [100 100 1100 640]);
tiledlayout(3, 1, 'Padding', 'compact', 'TileSpacing', 'compact');
nexttile; hold on;
for c = 1:2, plot(R(c).t, 100*R(c).soc, 'Color', C(c,:), 'LineWidth', 1.4); end
yline(50, ':', 'protection limit 50 %', 'LabelHorizontalAlignment', 'left'); ylim([40 102]); ylabel('Battery SOC (%)'); grid on;
legend({R.name}, 'Location', 'southoutside', 'Orientation', 'horizontal'); title('(a) Battery state of charge');
nexttile; d = 1:numel(R(1).jetPerDay);
b = bar(d, [R(1).jetPerDay R(2).jetPerDay], 'grouped'); b(1).FaceColor = C(1,:); b(2).FaceColor = C(2,:);
ylabel('Jetson checks'); ylim([0 7]); grid on; title('(b) Plant checks carried out per day (6 scheduled)');
nexttile;
b = bar(d, [R(1).nightPumps R(2).nightPumps], 'grouped'); b(1).FaceColor = C(1,:); b(2).FaceColor = C(2,:);
xlabel('Day of the simulation (8-21 January 2023 weather)'); ylabel('Night pump runs'); grid on;
title('(c) Night-time irrigation cycles per day');
exportgraphics(f, fullfile('figures', '11_protection_test.png'), 'Resolution', 200);
copyfile(fullfile('figures', '11_protection_test.png'), fullfile('..', 'report', 'images', 'protection_test.png'));
disp('Saved figures/11_protection_test.png and copied to report/images');
