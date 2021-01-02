# Fitting the curve to generate complexity for each function

You need to install [Matlab](https://www.mathworks.com/products/get-matlab.html) to run the scripts in curr dir.

They take the output file from In-house (or Production-run) as input:

`function_id,rms,cost`

The output is a file `complexity.csv`:

`function_id,complexity`, where complexity is 1 for O(1), 2 for O(N^2), 99 for O(e^k), others are irrelevant.

