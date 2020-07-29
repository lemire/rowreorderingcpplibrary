#
.SUFFIXES:
#
.SUFFIXES: .cpp .o .c .h


# not really useful:
CXXFLAGS = -g0 -O3  -Wall  -Woverloaded-virtual  -Wsign-promo -Wold-style-cast  -mssse3 -pipe  -DNDEBUG
#-fopenmp -D_GLIBCXX_PARALLEL 


all: tods2011


minilzo.o : lzocodec.h lzoconf.h  lzodefs.h  minilzo.c  minilzo.h
	cc  -DNDEBUG -O3 -c  minilzo.c

tods2011: tods2011.cpp  lzocodec.h flatfile.h columnwidecodecs.h ztimer.h stxxlmemorystores.h externalvector.h columncodecs.h bitpacking.h stxxlrowreordering.h array.h util.h lzocodec.h lzoconf.h  lzodefs.h  minilzo.o  minilzo.h
	c++  -DNDEBUG  -O3 -o  tods2011 tods2011.cpp   minilzo.o


clean:
	rm -f *.o tods2011 
