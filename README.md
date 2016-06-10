# BAMQL

This software is a simple query language for filtering SAM/BAM sequences.

## Installation

Ubuntu users can install from the PPA by executing:

    sudo apt-add-repository ppa:boutroslab/ppa && sudo apt-get update && sudo apt-get install bamql

In order to compile, [LLVM](http://llvm.org/) 3.4 - 3.7, [CLANG](http://clang.llvm.org/) (matched to LLVM version), [HTSlib](https://github.com/samtools/htslib/), and libuuid are required.

On Debian/Ubuntu, these can be installed by:

    sudo apt-get install autotools-dev build-essential clang libhts-dev libtool libpcre++-dev llvm-dev pkg-config uuid-dev zlib1g-dev libedit-dev

On RedHat/Fedora, these can be installed by:

    sudo yum groupinstall "Development Tools"
    sudo yum install clang libtool pcre-devel llvm-devel pkgconfig libuuid-devel zlib-devel libedit-devel

and HTSlib must be installed from sources.

In the source directory,

    autoreconf -i && ./configure && make && sudo make install

## The Query Language

The language consists of a number of predicates, things which match sequences, and connectives, which compose predicates.

For example, the following will match sequences on chromosome 7 which are from the read group labelled “RUN3”:

    chr(7) & read_group(RUN3)

The following will take a sub-sample for mitochondrial sequences and all the sequences that have matched to chromosomes starting with “ug”:

    chr(M) & random(0.2) | chr(ug*)

The details can be found in the manual page, which can be viewed by typing `man bamql_queries` at the command prompt.
