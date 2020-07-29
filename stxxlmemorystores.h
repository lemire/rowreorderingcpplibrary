/**
 * (c) Daniel Lemire 2010-2012
 * Apache License 2.0
 */

#ifndef MEMSTORES
#define MEMSTORES

#include <fstream>

#include <unistd.h>
#include <unordered_set>
#include "externalvector.h"
#include "stxxlrowreordering.h"

using namespace std;

typedef unsigned long long uint64;
#include "array.h"

namespace parameters {
const bool verbose = false;
const bool verboseMem = false;
}
;


// Lexico (basic)
template<int c> // number of columns
class Cmp {
public:

	Cmp(vector<uint> & indexes) :
		mIndexes(indexes) {

	}
	Cmp() :mIndexes(){
	}
	Cmp& operator=(const Cmp & v) {
		mIndexes = v.mIndexes;
		return *this;
	}
	bool operator ()(const lazyboost::array<uint, c> & a, const lazyboost::array<uint, c> & b) const {
		for (vector<uint>::const_iterator i = mIndexes.begin(); i
				!= mIndexes.end(); ++i) {
			uint k = *i;
			if (a[k] < b[k])
				return true;
			else if (a[k] > b[k])
				return false;
		}
		return false;

	}
	vector<uint> mIndexes;
};

template<int c> // number of columns
class RowStore {
public:
	RowStore(uint NumberOfRows) :
		data(NumberOfRows) {
	}
	RowStore() :
		data() {
	}
	
	~RowStore() {
		clear();
	}

	void top(const uint number, RowStore<c> & o) const {
		externalvector<lazyboost::array<uint, c> > newdata = data.top(number);
		o.data.swap(newdata);
	}


	void fillWithSample(const uint number, RowStore<c> & o) {
		externalvector<lazyboost::array<uint, c> > newdata = data.buildSample(number);
		o.data.swap(newdata);
	}

	void clear() {
		data.close();
		// we do want to release memory!
	}


	template<class FF>
	RowStore(FF & f, const uint maxnumberofrows) :
	data() {
		if(parameters::verbose) cout<<"opening data"<<endl;
		data.open();
		if(parameters::verbose) cout<<"opening data:ok"<<endl;
		lazyboost::array<int, c> cont;
		lazyboost::array<uint, c> rowbuffer;
		if(parameters::verboseMem) printMemoryUsage();
		if(maxnumberofrows>0) {
			uint nbrrows = 0;
			while (f.nextRow(cont)) {
				for (uint k = 0; k < cont.size(); ++k) {
					rowbuffer[k] = cont[k];
				}
				++nbrrows;
				if(!data.append(rowbuffer)) {
					cerr<<"Failure to append, bailing out"<<endl;
					break;
				}
				if(nbrrows == maxnumberofrows) break;
			}
		} else {
			while (f.nextRow(cont)) {
				for (uint k = 0; k < cont.size(); ++k) {
					rowbuffer[k] = cont[k];
				}
				if(!data.append(rowbuffer)) {
					cerr<<"Failure to append, bailing out"<<endl;
					break;
				}
			}
		}
		if(parameters::verbose) cout<<"I wrote "<<data.size()<<" elements!"<<endl;
	}

	uint size() const {
		return data.size() * c * sizeof(uint);
	}

	void sortRows(vector<uint> & indexes) {
		Cmp<c> cmp(indexes);
		data.sort(cmp);
	}

	void vortexSortRows(vector<uint> & indexes) {
		if (data.size() == 0)
			return;//no data
		Vortex v(indexes);
		data.sort(v);
	}

	void MultipleListsSortRowsPerBlock(vector<uint> & indexes,
			int BLOCKSIZE = 16384) {
		cout << "# multiplelists sorting with blocks of size " << BLOCKSIZE
				<< endl;
		if (data.size() == 0)
			return;//no data
		vector<lazyboost::array<uint,c> > buffer;
		for (uint64 k = 0; k < data.size(); k += BLOCKSIZE) {
			if (k + BLOCKSIZE < data.size()) {
				data.loadACopy(buffer,k,k+BLOCKSIZE);
				MultipleListsSort<vector<lazyboost::array<uint,c> > > (
						buffer.begin(), buffer.end(), indexes);
				data.copyAt(buffer,k);
			} else {
				data.loadACopy(buffer,k,data.size());
				MultipleListsSort<vector<lazyboost::array<uint,c> > > (
						buffer.begin(), buffer.end(), indexes);
				data.copyAt(buffer,k);
			}
		}
	}
	void shuffleRows(const uint BLOCKSIZE = externalvector<uint>::DEFAULTBLOCKSIZE) {//1048576
		data.shuffle(BLOCKSIZE);
	}

	void printFirstRows(const uint n) {
		vector<lazyboost::array<uint, c> > buffer;
		data.loadACopy(buffer,0, n);
		for (uint k = 0; k < n; ++k) {
			cout<<buffer[k]<<endl;
		}
	}

	uint64 countZeroes(const uint BLOCKSIZE = externalvector<uint>::DEFAULTBLOCKSIZE) {
		vector<lazyboost::array<uint, c> > buffer;
		uint64 sum = 0;
		for (uint64 k = 0; k < data.size(); k += BLOCKSIZE) {
			if (k + BLOCKSIZE < data.size()) {
				data.loadACopy(buffer,k,k+BLOCKSIZE);
			} else {
				data.loadACopy(buffer,k,data.size());
			}
			for(typename vector<lazyboost::array<uint, c> >::iterator i = buffer.begin(); i!= buffer.end(); ++i) {
				for(uint k = 0; k<c;++k)
					if((*i)[k]==0) {
						sum++;
					}
			}
		}
		return sum;
	}

	externalvector<lazyboost::array<uint, c> > data;
};

template<int c> // number of columns
class NaiveColumnStore {
public:
	NaiveColumnStore() :
		data() {
	}
	void makeColumnsIndependent(const uint BLOCKSIZE = externalvector<uint>::DEFAULTBLOCKSIZE) {//1048576
		for	(vector<externalvector<uint> >::iterator i =  data.begin(); i!= data.end(); ++i)
			i->shuffle(BLOCKSIZE);
	}



	template<class FF>
	NaiveColumnStore(FF & f) :
		data() {
		if (f.getNumberOfColumns() == 0)
			return;
		data.resize(f.getNumberOfColumns());
		for (uint k = 0; k < data.size(); ++k) {
			data[k].open();
		}
		vector<int> cont(f.getNumberOfColumns(), 0);
		while (f.nextRow(cont)) {
			for (uint k = 0; k < cont.size(); ++k) {
				data[k].append(cont[k]);
			}
		}
	}
	
	~NaiveColumnStore() { clear();}

	void clear() {
		for (uint k = 0; k < data.size(); ++k)
			data[k].close();
		data.clear();
	}

	uint64 computeRunCount(const uint MAPSIZE = externalvector<uint>::DEFAULTBLOCKSIZE) {
		uint64 answer = 0;
		for (vector<externalvector<uint> >::iterator i = data.begin(); i
				!= data.end(); ++i) {
			vector<uint> buffer;
			for (uint64 rowindex = 0; rowindex < i->size(); rowindex+=MAPSIZE) {
						i->loadACopy(buffer,rowindex,
								rowindex + MAPSIZE > i->size() ? i->size()
										: rowindex + MAPSIZE);
				      answer += runCount(buffer);
			}
		}
		return answer;
	}

	uint64 computeRunCountp(const uint BLOCKSIZE, const uint MAPSIZE = externalvector<uint>::DEFAULTBLOCKSIZE) {
		uint64 answer = 0;
				for (vector<externalvector<uint> >::iterator i = data.begin(); i
						!= data.end(); ++i) {
					vector<uint> buffer;
					for (uint64 rowindex = 0; rowindex < i->size(); rowindex+=MAPSIZE) {
						i->loadACopy(buffer,rowindex,
								rowindex + MAPSIZE > i->size() ? i->size()
										: rowindex + MAPSIZE);
						      answer += runCountp(buffer,BLOCKSIZE);
					}
				}
				return answer;
	}

	NaiveColumnStore(const RowStore<c> & rs) :
		data() {
		reloadFromRowStore(rs);
	}
	void copyToRowStore(RowStore<c> & rs,const uint MAPSIZE = externalvector<uint>::DEFAULTBLOCKSIZE) {
		rs.data.close();
		rs.data.open();
		vector<vector<uint> > buffer(MAPSIZE);
		lazyboost::array<uint, c> rowbuffer;
		uint64 nr = numberOfRows();
		for(uint64 begin = 0; begin<nr; begin+=MAPSIZE) {
			uint64 end = begin+ MAPSIZE;
			if(end>nr) end = nr;
			for (int k = 0; k < c; ++k) {
				data[k].loadACopy(buffer[k],begin,end);
			}
			for(uint64 i = 0; i!= end-begin; ++i) {
				for(uint k = 0; k<c;++k)
					rowbuffer[k] = buffer[k][i];
				if(!rs.data.append(rowbuffer)) {
						cerr<<"Failure to append, bailing out"<<endl;
						break;
				}
			}
		}
	}

	void reloadFromRowStore(RowStore<c> & rs, const uint maxsize = 0) {
		if (rs.data.size() == 0)
			return;
		uint64 begin = 0;
		data.resize(c);
		for (int k = 0; k < c; ++k) {
			data[k].close();// just in case
			data[k].open();//
		}
		const uint64 MAPSIZE = getpagesize() * 2048; // appears to default at 1024// 16777216;

		vector<lazyboost::array<uint, c> > buffer;
		if ((maxsize == 0) or (maxsize >= rs.data.size())) {

			for (uint64 rowindex = 0; rowindex < rs.data.size(); rowindex
					+= MAPSIZE) {
				rs.data.loadACopy(
						buffer,
						begin,
						begin + MAPSIZE > rs.data.size() ? rs.data.size()
								: begin + MAPSIZE);
				for (uint k = 0; k < buffer.size(); ++k) {
					const lazyboost::array<uint, c> & thisarray = buffer[k];
					for (uint k = 0; k < data.size(); ++k) {
						externalvector<uint> & thiscolumn = data[k];
						thiscolumn.append(thisarray[k]);
					}
				}
			}
		} else {
			for (uint64 rowindex = 0; rowindex < maxsize; rowindex += MAPSIZE) {
				rs.data.loadACopy(buffer, begin,
						begin + MAPSIZE > maxsize ? maxsize : begin + MAPSIZE);
				for (uint k = 0; k < buffer.size(); ++k) {
					const lazyboost::array<uint, c> & thisarray = buffer[k];
					for (uint k = 0; k < data.size(); ++k) {
						externalvector<uint> & thiscolumn = data[k];
						thiscolumn.append(thisarray[k]);
					}
				}
			}
		}
	}

	uint64 size() const {
		if (data.size() == 0)
			return 0;
		return data.size() * data[0].size() * sizeof(uint);
	}


	uint64 numberOfRows() const {
		if (data.size() == 0)
			return 0;
		return data[0].size() ;
	}

	bool operator==(const NaiveColumnStore & n) const {
		return data == n.data;
	}

	bool operator!=(const NaiveColumnStore & n) const {
		return data != n.data;
	}
	vector<externalvector<uint> > data;
};

#endif
