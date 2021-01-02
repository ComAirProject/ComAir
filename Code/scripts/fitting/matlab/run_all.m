% Get a list of all files and folders in this folder.
path = 'The/Path/To/Folder';
files = dir(path);
% Get a logical vector that tells which is a directory.
dirFlags = [files.isdir];
% Extract only those that are directories.
subFolders = files(dirFlags);
% Print folder names to command window.
for k = 1 : length(subFolders)
    if subFolders(k).name == "." || subFolders(k).name == ".."   || subFolders(k).name == "collections406"
        continue
    end
    disp(subFolders(k).name)
    main(path,subFolders(k).name,8,0)
end