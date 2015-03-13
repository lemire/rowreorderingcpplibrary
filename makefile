#
.SUFFIXES:
#
.SUFFIXES: .cpp .o .c .h


# not really useful:
CXXFLAGS = -g0 -O3  -Wall  -Woverloaded-virtual  -Wsign-promo -Wold-style-cast  -mssse3 -pipe  -DNDEBUG
#-fopenmp -D_GLIBCXX_PARALLEL 


###
## Normally, you should install LZO where gcc can find it... but in cases you can't:
LZO_ROOT=./mylzo
LZOFLAGS=-I$(LZO_ROOT)/include
LZOLIBS=-L$(LZO_ROOT)/lib

all: tods2011

tods2011: tods2011.cpp  lzocodec.h flatfile.h columnwidecodecs.h ztimer.h stxxlmemorystores.h externalvector.h columncodecs.h bitpacking.h stxxlrowreordering.h array.h util.h
	g++  -DNDEBUG -Wall -Wextra -Weffc++ -O3 -o  tods2011 tods2011.cpp  -llzo2 $(LZOFLAGS) $(LZOLIBS)

DIRPOS = RowReorderingForLongRuns/code
ZIPNAME=cppreorder_`date +%Y-%m-%d`.zip

todspackage: 
	echo $(DIRPOS)	
	cd ../..;zip -9 $(ZIPNAME) $(DIRPOS)/makefile $(DIRPOS)/README $(DIRPOS)/tods2011.cpp $(DIRPOS)/lzocodec.h $(DIRPOS)/flatfile.h $(DIRPOS)/columnwidecodecs.h $(DIRPOS)/ztimer.h $(DIRPOS)/stxxlmemorystores.h $(DIRPOS)/externalvector.h $(DIRPOS)/columncodecs.h $(DIRPOS)/bitpacking.h $(DIRPOS)/stxxlrowreordering.h array.h $(DIRPOS)/util.h ; mv $(ZIPNAME) $(DIRPOS)/; cd $(DIRPOS)/; zip -Tv $(ZIPNAME)

clean:
	rm -f *.o tods2011 
