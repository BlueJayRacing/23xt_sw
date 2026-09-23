cd C:\Users\steve\baja\23xt_sw\software\rpm_analysis\05-10-26-OregonDay4
folderpath = pwd;
fileExtension = '*.csv';
filelist = dir(fullfile(folderpath, fileExtension));

%% 

for m = 2:length(filelist) 

    file = filelist(m).name;
    fullname = fullfile(folderpath, file);
    data = readtable(fullname);
    for n = 1:6
        channel = 21+n;
        idx = data.internal_channel_id == channel;
        temp_table = data(idx, :);
        temp_table(:, 1) = [];
        temp_table(:, 2) = [];
        myTables{m-1,n} = temp_table;
    end
        array22 = table2array(myTables{m-1, 1});
        array23 = table2array(myTables{m-1, 2});
        array24 = table2array(myTables{m-1, 3});
        array25 = table2array(myTables{m-1, 4});
        array26 = table2array(myTables{m-1, 5});
        array27 = table2array(myTables{m-1, 6});

        t = tiledlayout(3,2);
        nexttile;
        plot(array22(:, 1), array22(:, 2));
        nexttile;
        plot(array23(:, 1), array23(:, 2));
        nexttile;
        plot(array24(:, 1), array24(:, 2));
        nexttile;
        plot(array25(:, 1), array25(:, 2));
        nexttile;
        plot(array26(:, 1), array26(:, 2));
        nexttile;
        plot(array27(:, 1), array27(:, 2));

        plotval = m-1;
        layoutname = "layout" + plotval + ".pdf";
        exportgraphics(t, layoutname);
end

