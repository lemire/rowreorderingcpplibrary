# The Row reordering C++ library -- External memory version 
![Ubuntu 20.04 CI (GCC 9)](https://github.com/lemire/rowreorderingcpplibrary/workflows/Ubuntu%2020.04%20CI%20(GCC%209)/badge.svg)

This is a set of row-reordering algorithms and data compression compression schemes implemented in C++. The goal of these algorithms is to maximize the compression ratio of database tables through row reordering.

This library is strictly for researchers with a working knowledge of C++ that are interested in studying our implementation.

For a simple demo (in Java) of the Vortex order described in the paper, please see https://github.com/lemire/SimpleVortex

# Requirements 

A POSIX C/C++ toolchain (linux, macOS)

# Usage 

    make tods2011
    ./tods2011 myfile.csv




