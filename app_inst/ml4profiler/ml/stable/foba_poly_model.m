%function foba_poly_model(dir_input, key, round)

%% Parsing argument list
arg_list = argv ();
for i = 1:nargin
    printf ('%s\n', arg_list{i});
end
dir_input = arg_list{1};
key = '';
round = str2num(arg_list{2});
max_terms = str2num(arg_list{3});

%% Hyper-parameters for the algorithm
nu = 0.5;
degree = 3;
threshold = 0.01;
f_scale = 0.1;
portion = 0.1;


%% Input & output file names
file_time   = strcat(dir_input, key, '/exectime.mat');
file_data   = strcat(dir_input, key, '/feature_data.mat');
file_var    = strcat(dir_input, key, '/varying_features.mat');
file_costly = strcat(dir_input, key, '/costly_features.txt');
file_chosen = strcat(dir_input, key, '/currently_chosen_features.txt');
file_out    = strcat(dir_input, key, '/rejecting_costly_features.txt');

%% Read in input files                    
load(file_time);
load(file_data);
load(file_var);

costly_f = load(file_costly);
printf("costly_f = load(file_costly): \n");
disp(costly_f);
printf("num_orig_feats: \n");
disp(num_orig_feats);
raw_data = zeros(length(runtime), num_orig_feats);
printf("raw_data = zeros(length(runtime), num_orig_feats): \n");
disp(raw_data);
raw_data(:, var_f) = var_data;
printf("raw_data(:, var_f) = var_data: \n");
disp(raw_data(:, var_f));

useful_f = setdiff(var_f, costly_f);
printf("useful_f = setdiff(var_f, costly_f): \n");
disp(useful_f);
features = (raw_data(:, useful_f));
printf("features = (raw_data(:, useful_f)): \n");
disp(features);
[num_data, D] = size(features);
printf("[num_data, D] = size(features): \n");
disp([num_data, D]);
costs = ones(1, D);
printf("costs = ones(1, D): \n");
disp(costs);

%% Randomly split data into training and testing sets
rand_indics = 1:num_data;
printf("rand_indics = 1:num_data: \n");
disp(rand_indics);
num_train = floor(portion*num_data);
printf("num_train = floor(portion*num_data): \n");
disp(num_train);
train_indics = rand_indics(1:num_train);
printf("train_indics = rand_indics(1:num_train): \n");
disp(train_indics);
test_indics  = rand_indics(num_train+1:num_data);
printf("test_indics  = rand_indics(num_train+1:num_data): \n");
disp(test_indics);
y      = runtime(train_indics);
printf("y      = runtime(train_indics): \n");
disp(y);
data   = features(train_indics, :);
printf("data   = features(train_indics, :): \n");
disp(data);
y_test    = runtime(test_indics, 1);
printf("y_test    = runtime(test_indics, 1): \n");
disp(y_test);
data_test = features(test_indics, :);
printf("data_test = features(test_indics, :): \n");
disp(data_test);

%% Foba polynomial regression
[err_sp_nl, num_chosen_feats, num_chose_terms, x_sp_nl, chosen_seqs, y_predict] = ...
   foba_poly_fitting_testing(y, data, y_test, data_test, costs, degree, threshold, nu, f_scale, max_terms, 0);
printf("calling [err_sp_nl, num_chosen_feats, num_chose_terms, x_sp_nl, chosen_seqs, y_predict] = foba_poly_fitting_testing(y, data, y_test, data_test, costs, degree, threshold, nu, f_scale, max_terms, 0): \n");
printf("err_sp_nl: \n");
disp(err_sp_nl);
printf("num_chosen_feats: \n");
disp(num_chosen_feats);
printf("num_chose_terms: \n");
disp(num_chose_terms);
printf("x_sp_nl: \n");
disp(x_sp_nl);
printf("chosen_seqs: \n");
disp(chosen_seqs);
printf("y_predict: \n");
disp(y_predict);
non0 = find(sum(chosen_seqs)>0);
printf("non0 = find(sum(chosen_seqs)>0): \n");
disp(non0);
chosen_feats = useful_f(non0(2:end)-1);
printf("chosen_feats = useful_f(non0(2:end)-1): \n");
disp(chosen_feats);
poly_terms = sequence2term(chosen_seqs, useful_f, 1);
printf("poly_terms = sequence2term(chosen_seqs, useful_f, 1): \n");
disp(poly_terms);

%% output information
%% The model to screen:
coefficients = x_sp_nl';
fprintf('Round %d:\n', round);
fprintf('%% The foba poly prediction error = %.3f, using the following %d terms model:\n', ...
         err_sp_nl, length(x_sp_nl));
fprintf('%% M(f) = %8.3f', coefficients(1));
for i = 2:length(coefficients)
    wterms = strcat(num2str(coefficients(i), '%8.3f'), char(poly_terms(i)));
    if (coefficients(i) > 0)
        formats_str = ' + %s';
    else
        formats_str = ' %s';
    end
    fprintf(formats_str, wterms);
end
fprintf('\n%% with features ');
fprintf('%d ', chosen_feats);
fprintf('\n');

%% The model to file:
fid = fopen(file_out, 'a');   
fprintf(fid, 'Round %d:\n', round);
fprintf(fid, '%% The foba poly prediction error = %.3f, using the following %d terms model:\n', ...
         err_sp_nl, length(x_sp_nl));
fprintf(fid, '%% M(f) = %8.3f', coefficients(1));
for i = 2:length(coefficients)
    wterms = strcat(num2str(coefficients(i), '%8.3f'), char(poly_terms(i)));
    if (coefficients(i) > 0)
        formats_str = ' + %s';
    else
        formats_str = ' %s';
    end
    fprintf(fid, formats_str, wterms);
end
fprintf(fid, '\n%% with features ');
fprintf(fid, '%d ', chosen_feats);
fprintf(fid, '\n');

fclose(fid);

%% The chosen features
fid = fopen(file_chosen, 'w');   
fprintf(fid, '%.3f ', err_sp_nl);
fprintf(fid, '%d ', chosen_feats);
fclose(fid);
