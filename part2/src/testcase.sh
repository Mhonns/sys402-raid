make clean
make
cd ../bin

# Init cases
./hearty-store-init 0 
./hearty-store-init 1   
./hearty-store-init 2   
./hearty-store-init 3

# Put cases
./hearty-store-put 0 ../src/Makefile  
./hearty-store-put 0 ../src/Makefile
./hearty-store-put 2 ../src/Makefile

# Replicated Cases
# ./hearty-store-list
# ./hearty-store-replicate 0
# ./hearty-store-list
# ./hearty-store-destroy 0

# Sync replicate cases
# ./hearty-store-replicate 1
# ./hearty-store-put 1 ../src/testcase.sh
# ./hearty-store-list
# ./hearty-store-replicate 1 ../src/testcase.sh

# High available cases
# ./hearty-store-ha 1 2 3
# ./hearty-store-list
# ./hearty-store-destroy 2
# ./hearty-store-list
# ./hearty-store-destroy 3

# Parity cases
./hearty-store-ha 1 2 3
./hearty-store-list
./hearty-store-destroy 2
./hearty-store-get 2