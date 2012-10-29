function [] = color_profile(FILE) 

A = textread(FILE);

% Discard first 4 columns and last one.
A = A(:, 5:end-1);

% Remove every fourth column.
M = repmat([0 1 1 1], 1, size(A,2) / 4);
M = logical(M);

A = A(:, M);

figure;
plot(A(:, 1:6:end));
title('Red response per checker');
xlabel('Frames');
ylabel('Red channel')


figure;
plot(A(:, 2:6:end));
title('Green response per checker');
xlabel('Frames');
ylabel('Green channel')

T = [ sum(A(:, 1:3:end),2) sum(A(:, 2:3:end),2) sum(A(:, 3:3:end), 2) ];
T = T ./ (size(A, 2) / 3);

figure;
plot(T);
title ('Response per color');
xlabel('Frames');
ylabel('Response');
legend({'B', 'G', 'R'});

gray = mean(T, 2);
weights = repmat(gray, [1 3]) ./ T;

figure;
plot(weights);
title ('Gray world weights');
xlabel('Frames');
ylabel('Weight');
legend({'B', 'G', 'R'});

end