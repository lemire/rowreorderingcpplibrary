This is a set of row-reordering algorithms and data compression compression schemes implemented in C++. The goal of these algorithms is to maximize the compression ratio of database tables through row reordering.

## Is this for you? ##

This library is strictly for researchers with a working knowledge of C++ that are interested in studying our implementation.

For a simple demo (in Java) of the Vortex order described in the paper, please see https://github.com/lemire/SimpleVortex


## Requirements ##

LZO2 library: http://www.oberhumer.com/opensource/lzo/
Readily available on Linux. Available on macos through fink (for example).


## Usage ##

We expect CSV input files (comma-separated values).

make tods2011

./tods2011 myfile.csv