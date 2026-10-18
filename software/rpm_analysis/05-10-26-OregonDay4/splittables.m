% Paths
folderpath = 'C:\Users\steve\baja\23xt_sw\software\rpm_analysis\05-10-26-OregonDay4';
outputpath = 'C:\Users\steve\baja\23xt_sw\software\rpm_analysis\05-10-26-OregonDay4\testplots';

% Create output folder if it doesn't exist
if ~exist(outputpath, 'dir')
    mkdir(outputpath);
end

filelist = dir(fullfile(folderpath, '*.csv'));

numFiles = length(filelist);
myTables = cell(numFiles - 1, 6);

% Create ONE hidden figure outside the loop to reuse
f = figure('Visible', 'off', 'Position', [100, 100, 1000, 800]);

%skipped the first file because its bugged
for m = 2:numFiles
    fileName = filelist(m).name;
    fullname = fullfile(folderpath, fileName);
    
    % Check if file can be read
    try
        data = readtable(fullname);
    catch
        warning('Could not read file: %s', fileName);
        continue;
    end
    
    % Clear the figure contents for the new iteration and recreate layout
    clf(f); 
    t = tiledlayout(f, 3, 2, 'TileSpacing', 'compact', 'Padding', 'compact');
    
    for n = 1:6
        channel = 21 + n; 
        idx = data.internal_channel_id == channel;
        
        temp_table = data(idx, :);
        
        % If data exists, clean it up. Otherwise, leave it empty.
        if height(temp_table) > 0
            % Delete columns 1 and 3 simultaneously
            temp_table(:, [1, 3]) = []; 
            
            % Two's complement conversion
            mask = temp_table.value > 32767;
            temp_table.value(mask) = temp_table.value(mask) - 65536;
        end
        
        myTables{m-1, n} = temp_table;
        
        % Plot regardless of whether data is empty or not
        nexttile(t);
        if height(temp_table) > 0
            arr = table2array(temp_table);
            
            % Convert microseconds (column 1) to seconds
            arr(:, 1) = arr(:, 1) / 1e6; 
            
            plot(arr(:, 1), arr(:, 2), 'LineWidth', 0.75);
        else
            plot(NaN, NaN); % Plots an empty frame so the tile layout stays intact
        end
        
        % Labels and Titles
        title(sprintf('Channel ID %d', channel), 'FontSize', 9);
        xlabel('seconds');
        ylabel('ADC Output');
    end
    
    % Export the layout graphic as a vector PDF
    [~, baseName, ~] = fileparts(fileName);
    layoutname = fullfile(outputpath, baseName + ".pdf");
    
    exportgraphics(t, layoutname, 'ContentType', 'vector');
    disp(['Successfully generated: ' char(layoutname)]);
end

% Close the figure window once at the very end
close(f);
