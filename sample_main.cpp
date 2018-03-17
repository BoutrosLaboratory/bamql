/*
 * For frequently reused queries, it is more efficient to compile a custom
 * binary.
 *
 * First, create a script in a file called `myscript.bamql`:
 *
 * check = <your script>;
 *
 * Then, compile the script as follows:
 *
 * bamql-compile myscript.bamql && c++ -o myscript --std=c++11 sample_main.cpp myscript.o $(pkg-config --cflags --libs bamql-itr bamql-rt)
 */
#include <bamql-iterator.hpp>
#include "myscript.h"
int main(int argc, char **argv) {
  return bamql::main(argc, argv, check, check_index);
}
