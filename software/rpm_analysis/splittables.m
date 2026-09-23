cd C:\Users\steve\baja\23xt_sw\05-10-26-OregonDay4
folderpath = pwd;
fileExtension = '*.csv';
filelist = dir(fullfile(folderpath, fileExtension));

%% 

for m = 2:length(filelist) 

    file = filelist(m).name;
    fullname = fullfile(folderpath, file);
    data = readtable(fullname);
    for n = 1:6
        temp_table = data(data.internal_channel_id == 21+n, :);
        temp_table(:, 1) = [];
        temp_table(:, 2) = [];
        myTables{m,n} = temp_table;
        array22 = table2array(myTables{1});
        array23 = table2array(myTables{2});
        array24 = table2array(myTables{3});
        array25 = table2array(myTables{4});
        array26 = table2array(myTables{5});
        array27 = table2array(myTables{6});

        figure(1);
        plot(array22(:, 1), array22(:, 2));
        title('WSG Channel ID 22');

        figure(2);
        plot(array23(:, 1), array23(:, 2));
        title('WSG Channel ID 23');

        figure(3);
        plot(array24(:, 1), array24(:, 2));
        title('WSG Channel ID 24');

        figure(4);
        plot(array25(:, 1), array25(:, 2));
        title('WSG Channel ID 25');

        figure(5);
        plot(array26(:, 1), array26(:, 2));
        title('WSG Channel ID 26');

        figure(6);
        plot(array27(:, 1), array27(:, 2));
        title('WSG Channel ID 27');
    end
    myTables = cell(1, 6);
end

