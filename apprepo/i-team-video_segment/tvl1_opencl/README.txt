To compile:

mkdir build
cd build
cmake ..
make

To run:
(from build dir)

cp ../tvl1/kernels.cl .  
 (or)
ln -s ../tvl1/kernels.cl .

./demo

You MUST copy or symlink kernels.cl to be in same folder as the executable!
