%PLOT_ANSYS_RESULTS  Compare ANSYS results (ansys/ansys_results.csv) with the
%   hand calculations, as a function of the number of modules N.
here = fileparts(mfilename('fullpath'));  cd(here);
A = readtable(fullfile('..', 'ansys', 'ansys_results.csv'));
H = handcalc_structure(unique(A.N)', [15 30]);

f = figure('Position', [100 100 1050 360]);
tiledlayout(1, 3, 'Padding', 'compact');
for V = [15 30]
    a = A(A.V_ms == V, :);  h = H(H.V_ms == V, :);
    nexttile(1); hold on;
    plot(h.N, h.top_defl_mm, '--o', 'DisplayName', sprintf('hand %d m/s', V));
    plot(a.N, a.top_defl_X_mm, '-s', 'DisplayName', sprintf('ANSYS %d m/s', V));
    nexttile(2); hold on;
    plot(h.N, h.bend_stress_MPa, '--o', 'DisplayName', sprintf('hand %d m/s (plain tube)', V));
    plot(a.N, a.max_vonMises_MPa, '-s', 'DisplayName', sprintf('ANSYS %d m/s (peak)', V));
end
nexttile(1); grid on; xlabel('Modules N'); ylabel('Top deflection (mm)'); legend('Location', 'northwest'); title('Deflection');
nexttile(2); grid on; xlabel('Modules N'); ylabel('Stress (MPa)'); yline(10, ':', 'allowable 10 MPa');
legend('Location', 'northwest'); title('Stress');
nexttile(3); hold on; h = H(H.V_ms == 15, :); a = A(A.V_ms == 0, :);
plot(h.N, h.f1_Hz, '--o', 'DisplayName', 'hand');
plot(a.N, a.mode1_Hz, '-s', 'DisplayName', 'ANSYS mode 1');
grid on; xlabel('Modules N'); ylabel('Frequency (Hz)'); legend; title('First natural frequency');
exportgraphics(f, fullfile('figures', '08_ansys_vs_handcalc.png'), 'Resolution', 200);
disp('Saved figures/08_ansys_vs_handcalc.png');
