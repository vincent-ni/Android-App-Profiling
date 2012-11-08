%function [num_orig_feats] = foba_poly_model_init(dir_input, key, runs)

%% Parsing argument list
arg_list = argv ();
for i = 1:nargin
    printf ('%s\n', arg_list{i});
end
dir_input = arg_list{1};
key = '';
runs = str2num(arg_list{3});

%% Input files
file_time   = strcat(dir_input, key, '/exectime.mat');
file_data   = strcat(dir_input, key, '/feature_data.mat');
file_var    = strcat(dir_input, key, '/varying_features.mat');
file_costly = strcat(dir_input, key, '/costly_features.txt');

%% Reading data
% Execution time file
% Format: raw_time
%     r1  r2  r3
% m1
% m2
% m3
file_exectime = strcat(dir_input, key, '/exectime.txt');
raw_time = load(file_exectime);
printf("raw_time = load(file_exectime): \n");
disp(raw_time);
% 10
% 5
% 15
runtime = sum(raw_time, 2)/runs;
printf("runtime = sum(raw_time, 2)/runs: \n");
disp(runtime);
% 3.333
% 1.666
% 5.000

% Cost file
% Format: costs after being transposed
% c1 c2 c3 c4
file_feature_cost = strcat(dir_input, key, '/feature_cost.txt');
costs = load(file_feature_cost);
costs = costs';
printf("costs = load(file_feature_cost)': \n"); 
disp(costs);
% 1 1 1 1

% Feature file
% Format: raw_data after being transposed
%    f1  f2  f3  f4
% m1
% m2
% m3
file_feature_data = strcat(dir_input, key, '/feature_data.txt');
raw_data = load(file_feature_data);
raw_data = raw_data';
num_orig_feats = size(raw_data, 2);
printf("raw_data = load(file_feature_data)': \n");
disp(raw_data);
% 10 1 1 1
% 1 10 1 1
% 1 1 10 1
printf("num_orig_feats = size(raw_data, 2): \n");
disp(num_orig_feats);
% 4

%% Intermediate files
% Get features with variability
% Format: var_data
%  f1  f2  f3  f4  column-wise
var_data = var(raw_data);
printf("var_data = var(raw_data): \n");
disp(var_data);
% 27 27 27 0
% find non-zero columns
var_f = find(var_data > 0);
printf("var_f = find(var_data > 0): \n");
disp(var_f);
% 1 2 3
%disp(raw_data(:, var_f));
% 10 1 1
% 1 10 1
% 1 1 10
%disp(costs(var_f));
% 1 1 1
% Remove redundant features
[org_data, unique_f] = remove_identical_cols(raw_data(:, var_f), costs(var_f));
printf("call [org_data, unique_f] = remove_identical_cols(raw_data(:, var_f), costs(var_f)): \n");
printf("matrix raw_data(:, var_f): \n");
disp(raw_data(:, var_f));
printf("matrix costs(var_f): \n");
disp(costs(var_f));
printf("matrix unique_f: \n");
disp(unique_f);
printf("matrix org_data: \n");
disp(org_data);
% 1 0 0
% 0 1 0
% 0 0 1
%disp(unique_f);
% 1 2 3
var_f = var_f(unique_f);
printf("var_f = var_f(unique_f): \n");
disp(var_f);
% 1 2 3

raw_data = normalization(raw_data);
printf("raw_data = normalization(raw_data): \n");
disp(raw_data);
% 1 0 0 1
% 0 1 0 1
% 0 0 1 1
costly_f = [0];

[num_data, D] = size(raw_data);
printf("[num_data, D] = size(raw_data)): \n");
disp([num_data, D]);
% 3 4
rand_indics = randperm(num_data);
printf("rand_indics = randperm(num_data): \n");
disp(rand_indics);
% 3 2 1
var_data = raw_data(rand_indics, var_f);
printf("var_data = raw_data(rand_indics, var_f): \n");
disp(var_data);
% 0 1 0
% 1 0 0
% 0 0 1
runtime = runtime(rand_indics);
printf("runtime = runtime(rand_indics): \n");
disp(runtime);
% 5.000
% 1.666
% 3.333

% For storing intermediate results
save(file_time, 'runtime');
save(file_data, 'var_data');
save(file_var, 'var_f', 'num_orig_feats');

% For costly features, initialized to be empty file.
fid = fopen(file_costly, 'w');
fprintf(fid, '%d ', costly_f);
fclose(fid);

