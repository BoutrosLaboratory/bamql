# BAMQL

This software is a simple query language for filtering SAM/BAM sequences.

## Installation

In order to compile, [LLVM](http://llvm.org/) 15-20, [HTSlib](https://github.com/samtools/htslib/), and libuuid are required.

On Debian/Ubuntu, these can be installed by:

    sudo apt-get install autotools-dev build-essential libhts-dev libtool libpcre3-dev llvm-dev pkg-config uuid-dev zlib1g-dev libossp-uuid-dev

On RedHat/Fedora, these can be installed by:

    sudo yum groupinstall "Development Tools"
    sudo yum install libtool pcre-devel llvm-devel pkgconfig libuuid-devel zlib-devel

and HTSlib must be installed from sources.

In the source directory,

    autoreconf -i && ./configure && make && sudo make install

If you do not have static LLVM libraries available and the configure step above fails, try

    autoreconf -i && ./configure --enable-static=no --enable-static-llvm=no && make && sudo make install

## The Query Language

The language consists of a number of predicates, things which match sequences, and connectives, which compose predicates.

For example, the following query will match sequences on chromosome 7 which are from the read group labelled “RUN3”:

    chr(7) & read_group : RUN3

To filter read, use `bamql` like this:

    bamql -i input.bam -o reads_i_live.bam -O reads_i_loathe.bam 'chr(7) & read_group : RUN3'

The following will take a sub-sample for mitochondrial sequences and all the sequences that have matched to chromosomes starting with “ug”:

    chr(M) & random(0.2) | chr(ug*)

Again, to filter, use `bamql` like this:

    bamql -f input.bam -o mitochondrial_subsample_with_traps.bam 'chr(M) & random(0.2) | chr(ug*)'

The details can be found in the manual page, which can be viewed by typing `man bamql_queries` at the command prompt or [view the manual online](http://artefacts.masella.name/bamql_queries.html).
