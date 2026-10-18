% change the path to where your data is
cd C:\Users\steve\baja\23xt_sw\software\rpm_analysis\05-10-26-OregonDay4
folderpath = pwd;
fileExtension = '*.csv';
filelist = dir(fullfile(folderpath, fileExtension));
f = figure("Visible","off");
%% 
% skipped the first file because its bugged for some reason
% the last file in filelist is a combo of all the data from each csv file
for m = 2:length(filelist) 
        array22 = table2array(myTables{m-1, 1});
        array23 = table2array(myTables{m-1, 2});
        array24 = table2array(myTables{m-1, 3});
        array25 = table2array(myTables{m-1, 4});
        array26 = table2array(myTables{m-1, 5});
        array27 = table2array(myTables{m-1, 6});
        
        t = tiledlayout(3,2);
        nexttile;
        plot(array22(:,1), array22(:,2));
        title("WSG Channel 22");
        nexttile;
        plot(array23(:,1), array23(:,2));
        title("WSG Channel 23");
        nexttile;
        plot(array24(:,1), array24(:,2));
        title("WSG Channel 24");
        nexttile;
        plot(array25(:,1), array25(:,2));
        title("WSG Channel 25");
        nexttile;
        plot(array26(:,1), array26(:,2));
        title("WSG Channel 26");
        nexttile;
        plot(array27(:,1), array27(:,2));
        title("WSG Channel 27");
        % the plot is value of the channel against absolute time
        [path, datasetname, ext] = fileparts(filelist(m).name);
        targetFolder = 'C:\Users\steve\baja\23xt_sw\software\rpm_analysis\05-10-26-OregonDay4\plots';
        fileName = "abstime_" + datasetname + ".pdf";
        fullPath = fullfile(targetFolder, fileName);
        exportgraphics(t, fullPath);
end