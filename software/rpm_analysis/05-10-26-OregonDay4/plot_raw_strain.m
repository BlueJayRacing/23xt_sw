% Load, convert, and plot. No filtering or smoothing.
folder = fileparts(mfilename('fullpath'));
loaded = load(fullfile(folder, 'wsg_data_seperated', 'splitwsgdata.mat'));
tables = loaded.myTables(1:end-1, :); % Skip the duplicate combined row

% quarter bridge


% these are from spreadsheet
Vref = 2.5;  % importantly not sure about this one! spreadsheet says 2.5 but 
% its possible its a different value...
Rg = 80.6; 
GF = 2;
Vex = 3.3;
gain = 5 + 80000 / Rg;
% converting adc values to voltage/strain
voltsPerCount = Vref / 32768;
% Convert ADC counts to microstrain using math from spreadsheet/seimens doc
uePerCount = 4e6 * voltsPerCount / (gain * GF * Vex);

figure;
tiledlayout(3, 2);
for channel = 1:6
    values = cellfun(@(t) double(t.value), tables(:, channel), ...
        'UniformOutput', false);
    vals = vertcat(values{:});

    reference = 0;
strainMin = (min(vals) - reference) * uePerCount;
strainMax = (max(vals) - reference) * uePerCount;

fprintf('Channel %d: min = %.2f, max = %.2f, range = %.2f microstrain\n', ...
    channel + 21, strainMin, strainMax, strainMax - strainMin);

    nexttile;
    hold on;

    for row = 1:size(tables, 1)
        data = tables{row, channel};
        vals = double(data.value);
        
        % subtract reference then multiply by calculated ratio
        strain = (vals - reference) * uePerCount;
        plot(double(data.recorded_time_ms) / 3.6e6, strain, 'b');
    end

    title(sprintf('Channel %d', channel + 21));
    xlabel('Logger time (hours)');
    ylabel('Relative strain (microstrain)');
    grid on;

    
end
