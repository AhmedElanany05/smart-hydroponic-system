function w = load_pvgis(file)
%LOAD_PVGIS  Read a PVGIS hourly "seriescalc" CSV (time in UTC).
%   w.t (datetime, Cairo time UTC+2), w.G (W/m2 in plane), w.sun (deg),
%   w.T (deg C), w.wind (m/s at 10 m), w.day (day index 1..365)
txt   = fileread(file);
rows  = regexp(txt, '(?m)^(\d{8}):(\d{2})\d{2},([\d.]+),([-\d.]+),([-\d.]+),([\d.]+)', 'tokens');
n     = numel(rows);
w.t   = NaT(n,1); w.G = zeros(n,1); w.sun = w.G; w.T = w.G; w.wind = w.G;
for k = 1:n
    r = rows{k};
    w.t(k)    = datetime([r{1} r{2}], 'InputFormat', 'yyyyMMddHH') + hours(2);
    w.G(k)    = str2double(r{3});
    w.sun(k)  = str2double(r{4});
    w.T(k)    = str2double(r{5});
    w.wind(k) = str2double(r{6});
end
w.day = floor(days(w.t - dateshift(w.t(1), 'start', 'day'))) + 1;
end
