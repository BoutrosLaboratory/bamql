# BAMQL

This software is a simple query language for filtering SAM/BAM sequences.

## Installation

In order to compile, [LLVM](http://llvm.org/), [CLANG](http://clang.llvm.org/), [HTSlib](https://github.com/samtools/htslib/), and libuuid are required.

On Debian/Ubuntu, these can be installed by:

    sudo apt-get install llvm-dev libhts-dev uuid-dev build-essential clang

In the source directory,

    autoreconf -i && ./configure && make && sudo make install

## The Query Language

The language consists of a number of predicates, things which match sequences, and connectives, which compose predicates.

For example, the following will match sequences on chromosome 7 which are from the read group labelled “RUN3”:

    chr(7) & read_group(RUN3)

The following will take a sub-sample for mitochondrial sequences and all the sequences that have matched to chromosomes starting with “ug”:

    chr(M) & random(0.2) | chr(ug*)

The details can be found in the manual page, which can be viewed by typing `man bamql_queries` at the command prompt.
