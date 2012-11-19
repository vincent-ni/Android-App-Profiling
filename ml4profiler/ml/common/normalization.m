function B = normalization(A)

[n, d] = size(A);
min_A = min(A);
max_A = max(A);
range_A = max_A - min_A;
inds = find(range_A > 0 );

disp("min");
disp(min_A);
disp("range");
disp(range_A);

B = A;
B(:, inds) = (A(:, inds) - repmat(min_A(inds), [n, 1])) ./ repmat(range_A(inds), [n, 1]);
