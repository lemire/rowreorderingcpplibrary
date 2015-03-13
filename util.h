/**
 * (c) Daniel Lemire 2010-2012
 * Apache License 2.0
 */

#ifndef UTIL
#define UTIL

#include <vector>
#include <set>
#include <sys/time.h>
#include <sys/resource.h>
using namespace std;

typedef unsigned int uint;
typedef unsigned int uint32;
typedef unsigned long long uint64;

/*int bits(uint v) {
    int r (0);
    while(v>0) {
        ++r;
        v>>=1;
    }
    return r;
}*/
int bits(uint v) {
    int r (0);
    if(v>=(1U <<15)) {
    	v>>=16;r+=16;
    }
    if(v>=(1U <<7)) {
        v>>=8;r+=8;
    }
    if(v>=(1U <<3)) {
        v>>=4;r+=4;
    }
    if(v>=(1U <<1)) {
        v>>=2;r+=2;
    }
    if(v>=(1U <<0)) {
        v>>=1;r+=1;
    }
    return r;
}


int ceildiv(int x, int y) {
	return x/y + (x % y != 0);
}



template <class iterator>
uint64 minxorcost(const iterator & begin, const iterator & end) {
	uint64 counter = bits(*begin);
    for(iterator  k = begin; k+1 !=end; ++k) {
    	counter += bits(   (*k) ^ (*(k+1)) );
    }
    return counter;
}

template <class iterator>
uint64 maxxorcost(const iterator & begin, const iterator & end) {
	int mbits = bits(*begin);
    for(iterator  k = begin; k+1 !=end; ++k) {
    	int thisbits = bits(   (*k) ^ (*(k+1)) );
    	if(thisbits > mbits) mbits = thisbits;
    }
    return mbits * (end-begin);
}

uint delta (uint x, uint y) {
	if(x<=y)
		return 2*(y-x);
	else
		return 2*(x-y)+1;
}

template <class iterator>
uint64 mindeltacost(const iterator & begin, const iterator & end) {
	uint64 counter = bits(*begin);
    for(iterator  k = begin; k+1 !=end; ++k) {
    	counter += bits(  delta( (*k) , (*(k+1)) ) );
    }
    return counter;
}


template <class iterator>
uint64 fixedmodulocost(const iterator & begin, const iterator & end, const uint blocksizeinbytes) {
	const int mbits = maxbits(begin,end);
	uint64 counter = 0;//bits(*begin);
	const uint initcost = ceildiv(mbits+bits(end-begin),8*sizeof(uint))*8*sizeof(uint);
    for(iterator  k = begin; k !=end; ++k) {
    	iterator  myk = k;
    	uint currentcost = initcost; // price of initial value
    	myk+=1;
    	uint runlength = 0;
    	uint diffinbits = 0;
    	while((ceildiv(currentcost,8)<blocksizeinbytes) && (myk<end)) {
    		k=myk;
    		iterator target = myk+32;
    		if(target>end) target = end;
    		runlength += target - myk;
    		for(;myk<target;++myk) {
    			uint diff = (*(myk+1) - *myk) % (1<<mbits);
    			if(diff>>diffinbits > 0)
    				diffinbits = bits(diff);
    		}
    		currentcost = runlength * diffinbits + initcost;
    	}
    	counter += blocksizeinbytes*8;
    }
    return counter;
}

template <class iterator>
uint64 minmodulocost(const iterator & begin, const iterator & end) {
	const int mbits = maxbits(begin,end);
	uint64 counter = bits(*begin);
    for(iterator  k = begin; k+1 !=end; ++k) {
    	counter +=bits((*(k+1) - *k) % (1<<mbits));
    }
    return counter;
}

uint __patchedcost(uint runlength,vector<uint> & diffhisto ) {
	const int mbits = diffhisto.size()-1;
	int exceptions = 0;
	int bestL = mbits;
	uint bestcost = ceildiv(bits(runlength)+bestL*runlength,8*sizeof(uint))*8*sizeof(uint);
	for(int L = mbits-1; L>=0;--L) {
		exceptions += diffhisto[L+1];
		uint newcost = ceildiv(exceptions * ( bits(runlength) + (mbits-L))+bits(runlength)+L*runlength,8*sizeof(uint))*8*sizeof(uint);
		if(newcost < bestcost) {
			bestcost = newcost;
			bestL=L;
		}
	}
	return bestcost;
}

template <class iterator>
uint64 fixedpatchedmodulocost(const iterator & begin, const iterator & end, const uint blocksizeinbytes) {
	const int mbits = maxbits(begin,end);
	uint64 counter = 0;//bits(*begin);
	const uint initcost = ceildiv(mbits+bits(end-begin),8*sizeof(uint))*8*sizeof(uint);
	vector<uint> diffhisto(mbits+1,0);
	for(iterator  k = begin; k !=end; ++k) {
    	iterator  myk = k;
    	uint currentcost = initcost; // price of initial value
    	myk+=1;
    	uint runlength = 0;
    	diffhisto.clear(); diffhisto.resize(mbits+1,0);
    	while((ceildiv(currentcost,8)<blocksizeinbytes) && (myk<end)) {
    		k=myk;
    		iterator target = myk+32;
    		if(target>end) target = end;
    		runlength += target - myk;
    		for(;myk<target;++myk) {
    			uint diff = (*(myk+1) - *myk) % (1<<mbits);
    			diffhisto[bits(diff)]+=1;
    		}
    		currentcost =  initcost + __patchedcost(runlength,diffhisto );
    	}
    	counter += blocksizeinbytes*8;
    }
    return counter;
}

uint __patchedcost2(uint runlength,vector<uint> & diffhisto ) {
	const int mbits = diffhisto.size()-1;
	int exceptions = 0;
	int bestL = mbits;
	uint bestcost = ceildiv(runlength+bestL*runlength,8*sizeof(uint))*8*sizeof(uint);
	for(int L = mbits-1; L>=0;--L) {
		exceptions += diffhisto[L+1];
		uint newcost = ceildiv(exceptions * (  (mbits-L))+runlength+L*runlength,8*sizeof(uint))*8*sizeof(uint);
		if(newcost < bestcost) {
			bestcost = newcost;
			bestL=L;
		}
	}
	return bestcost;
}

template <class iterator>
uint64 fixedpatchedmodulocost2(const iterator & begin, const iterator & end, const uint blocksizeinbytes) {
	const int mbits = maxbits(begin,end);
	uint64 counter = 0;//bits(*begin);
	const uint initcost = ceildiv(mbits+bits(end-begin),8*sizeof(uint))*8*sizeof(uint);
	vector<uint> diffhisto(mbits+1,0);
	for(iterator  k = begin; k !=end; ++k) {
    	iterator  myk = k;
    	uint currentcost = initcost; // price of initial value
    	myk+=1;
    	uint runlength = 0;
    	diffhisto.clear(); diffhisto.resize(mbits+1,0);
    	while((ceildiv(currentcost,8)<blocksizeinbytes) && (myk<end)) {
    		k=myk;
    		iterator target = myk+32;
    		if(target>end) target = end;
    		runlength += target - myk;
    		for(;myk<target;++myk) {
    			uint diff = (*(myk+1) - *myk) % (1<<mbits);
    			diffhisto[bits(diff)]+=1;
    		}
    		currentcost =  initcost + __patchedcost2(runlength,diffhisto );
    	}
    	counter += blocksizeinbytes*8;
    }
    return counter;
}


template <class iterator>
uint64 danielrlecost(const iterator & begin, const iterator & end) {
    int mbits = maxbits(begin,end);
    int bestL =32;
    uint64 bestCost = bestL*(end-begin);
    iterator lastexception = begin;
    for(int L=0; L<=mbits;++L) {
    	// store first L bits flat, then rle
    	int howmanyruns = 0;
    	int maxdistance = 0;
    	int bitsforruns = 0;
    	for(iterator  k = begin; k+1 !=end; ++k) {
    		uint val1 = *k;
    		uint val2 = *(k+1);
    		uint diff = (val2-val1)%(1<<mbits);//delta(val1,val2);
    		if(diff>>L > 0) {
    			++howmanyruns;
    			if(k-lastexception > maxdistance) maxdistance = k-lastexception;
    			lastexception = k;
    			if(bitsforruns< bits(diff>>L)) bitsforruns = bits(diff>>L);
    		}
    	}
    	uint64 thiscost = howmanyruns * (bitsforruns + bits(maxdistance)) + L*(end-begin);
    	if(thiscost<bestCost) {
    		bestCost=thiscost;
    		bestL=L;
    	}
    }
    return bestCost;
}


template <class iterator>
uint64 danielrlecostxor(const iterator & begin, const iterator & end) {
    int mbits = maxbits(begin,end);
    int bestL =32;
    uint64 bestCost = bestL*(end-begin);
    iterator lastexception = begin;
    for(int L=0; L<=mbits;++L) {
    	// store first L bits flat, then rle
    	int howmanyruns = 0;
    	int maxdistance = 0;
    	int bitsforruns = 0;
    	for(iterator  k = begin; k+1 !=end; ++k) {
    		uint val1 = *k;
    		uint val2 = *(k+1);
    		uint diff = val1 ^ val2;//delta(val1,val2);
    		if(diff>>L > 0) {
    			++howmanyruns;
    			if(k-lastexception > maxdistance) maxdistance = k-lastexception;
    			lastexception = k;
    			if(bitsforruns< bits(diff>>L)) bitsforruns = bits(diff>>L);
    		}
    	}
    	uint64 thiscost = howmanyruns * (bitsforruns + bits(maxdistance)) + L*(end-begin);//
    	if(thiscost<bestCost) {
    		bestCost=thiscost;
    		bestL=L;
    	}
    }
   // cout<<"chose L="<<bestL<<endl;
    return bestCost;
}


template <class iterator>
uint64 danielrlecostrandomaccess(const iterator & begin, const iterator & end) {
    int mbits = maxbits(begin,end);
    //const uint costofarun=2*32;
    int logn=bits(end-begin);
    int bestL =32;
    uint64 bestCost = bestL*(end-begin);
    iterator lastexception = begin;
    for(int L=0; L<=mbits;++L) {
    	// store first L bits flat, then rle
    	int howmanyruns = 0;
    	int maxdistance = 0;
    	int bitsforruns = 0;
    	for(iterator  k = begin; k+1 !=end; ++k) {
    		uint val1 = *k;
    		uint val2 = *(k+1);
    		uint diff = val1 ^ val2;//delta(val1,val2);
    		if(diff>>L > 0) {
    			++howmanyruns;
    			if(k-lastexception > maxdistance) maxdistance = k-lastexception;
    			lastexception = k;
    			if(bitsforruns< bits(diff>>L)) bitsforruns = bits(diff>>L);
    		}
    	}//
    	uint64 thiscost = howmanyruns * (bitsforruns + bits(maxdistance)) + L*(end-begin);//bits(maxdistance)
    	if(thiscost<bestCost) {
    		bestCost=thiscost;
    		bestL=L;
    	}
    }
    return bestCost;
}



template <class iterator>
uint64 danielrlecostmodulorandomaccess(const iterator & begin, const iterator & end) {
    int mbits = maxbits(begin,end);
    //const uint costofarun=2*32;
    int logn=bits(end-begin);
    int bestL =32;
    uint64 bestCost = bestL*(end-begin);
    iterator lastexception = begin;
    for(int L=0; L<=mbits;++L) {
    	// store first L bits flat, then rle
    	int howmanyruns = 0;
    	int maxdistance = 0;
    	int bitsforruns = 0;
    	for(iterator  k = begin; k+1 !=end; ++k) {
    		uint val1 = *k;
    		uint val2 = *(k+1);
    		uint diff = (val2 - val1) %(1<<mbits);//delta(val1,val2);
    		if(diff>>L > 0) {
    			++howmanyruns;
    			if(k-lastexception > maxdistance) maxdistance = k-lastexception;
    			lastexception = k;
    			if(bitsforruns< bits(diff>>L)) bitsforruns = bits(diff>>L);
    		}
    	}
    	uint64 thiscost = howmanyruns * (bitsforruns + bits(maxdistance)) + L*(end-begin);//bits(maxdistance)
    	if(thiscost<bestCost) {
    		bestCost=thiscost;
    		bestL=L;
    	}
    }
    return bestCost;
}

template <class iterator>
uint64 danielrlecostmodulo2randomaccess(const iterator & begin, const iterator & end) {
    int mbits = maxbits(begin,end);
    int logn=bits(end-begin);
    int bestL =32;
    uint64 bestCost = bestL*(end-begin);
    iterator lastexception = begin;
    for(int L=0; L<=mbits;++L) {
    	// store first L bits flat, then rle
    	int howmanyruns = 0;
    	int maxdistance = 0;
    	int bitsforruns = 0;
    	for(iterator  k = begin; k+2 !=end; ++k) {
    		uint val1 = *k;
    		uint val2 = *(k+1);
    		uint val3 = *(k+2);
    		uint diff = (val3+ 2 * val2 - val1) %(1<<mbits);//delta(val1,val2);
    		if(diff>>L > 0) {
    			++howmanyruns;
    			if(k-lastexception > maxdistance) maxdistance = k-lastexception;
    			lastexception = k;
    			if(bitsforruns< bits(diff>>L)) bitsforruns = bits(diff>>L);
    		}
    	}
    	uint64 thiscost = howmanyruns * (bitsforruns + bits(maxdistance)) + L*(end-begin);//bits(maxdistance)
    	if(thiscost<bestCost) {
    		bestCost=thiscost;
    		bestL=L;
    	}
    }
    return bestCost;
}


template <class iterator>
uint64 maxdeltacost(const iterator & begin, const iterator & end) {
	int mbits = bits(*begin);
    for(iterator  k = begin; k+1 !=end; ++k) {
    	int thisbits = bits(  delta( (*k) , (*(k+1)) ) );
    	if(thisbits > mbits) mbits = thisbits;
    }
    return mbits * (end-begin);
}

template <class iterator>
uint64 mincost(const iterator & begin, const iterator & end) {
	uint64 counter (0);
    for(iterator  k = begin; k !=end; ++k) {
    	counter += bits(   *k );
    }
    return counter;
}

template <class iterator>
uint64 maxcost(const iterator & begin, const iterator & end) {
	return maxbits(begin,end) * (end-begin);
}


// from wikipedia
int floorLog2(unsigned int n) {
  if (n == 0)
    return -1;

  int pos = 0;
  if (n >= (1 <<16)) { n >>= 16; pos += 16; }
  if (n >= (1 << 8)) { n >>=  8; pos +=  8; }
  if (n >= (1 << 4)) { n >>=  4; pos +=  4; }
  if (n >= (1 << 2)) { n >>=  2; pos +=  2; }
  if (n >= (1 << 1)) {           pos +=  1; }
  return pos;
}


// provided a "permutation" of the number from 0 to n-1, compute the inverse
vector<uint> inversePermutation(const vector<uint> & perm) {
	vector<uint> inv(perm.size());
	for(uint k = 0; k<perm.size(); ++k) {
		inv[perm[k]] = k;
	}
	return inv;
}

template <class C>
uint Hamming(const C & i, const C & j) {
	uint counter = 0;
	for(uint k = 0; k<i.size();++k)
		if(i[k]!= j[k])++counter;
	return counter;
}

template <class C>
uint runCount(const C & column) {
	if(column.size()==0) return 0;
	uint answer = 1;
	for(uint k = 1; k<column.size();++k)
		if(column[k-1]!=column[k]) ++answer;
	return answer;
}
template <class C>
uint runCountp(const C & column, int BLOCKSIZE) {
	if(column.size()==0) return 0;
	uint answer = 1;
	for(uint k = 1; (k+1)*BLOCKSIZE<=column.size();++k) {
		for(int j = 0; j<BLOCKSIZE;++j) {
			if(column[k*BLOCKSIZE+j]!=column[j+(k-1)*BLOCKSIZE]) {
				++answer;
				break;
			}
		}
	}
	if(column.size() %  BLOCKSIZE != 0)
		++answer;
	return answer;
}
template <class C>
uint blockDistinct(const C & column, const uint blocksize) {
	set<uint> counter;
	uint answer = 0;
	for(vector<uint>::const_iterator i = column.begin(); i<column.end(); i+=blocksize) {
		vector<uint>::const_iterator end = i+blocksize;
		if(end>column.end()) end = column.end() ; // could be faster by moving the if out of the loop
		for(vector<uint>::const_iterator j = i; j!=end;++j)
			counter.insert(*j);
		answer += counter.size();
		counter.clear();
	}
	return answer;
}




uint min(uint a, uint b) {
	if(a<b) return a;
	return b;
}

template <class iterator>
int maxbits(const iterator & begin, const iterator & end) {
    int maxlog2 = 0;
    for(iterator  k = begin; k!=end; ++k) {
        if(((*k) >> maxlog2) >0)
            maxlog2 = bits(*k);
    }
    return maxlog2;
}
template <class C>
int maxbits(const C & in) {
    int maxlog2 = 0;
    for(typename C::const_iterator  k = in.begin(); k!=in.end(); ++k) {
        if(((*k) >> maxlog2) >0)
            maxlog2 = bits(*k);
    }
    return maxlog2;
}

double averageRunLength(const vector<uint> & in) {
	uint numberofruns = 1;
	for(vector<uint>::const_iterator  k = in.begin()+1; k!=in.end(); ++k) {
		if(*k != *(k-1)) ++numberofruns;
	}
	return in.size() * 1.0 / numberofruns;
}

void printMemoryUsage() {
	int who = RUSAGE_SELF; 
	struct rusage usage; 
	getrusage(who,&usage);
	cout<<endl<<"=========="<<"Memory usage = "<<usage.ru_maxrss<<"=========="<<endl;
}

inline uint endian_swap(uint x) {
    return (x>>24) | 
        ((x<<8) & 0x00FF0000) |
        ((x>>8) & 0x0000FF00) |
        (x<<24);
}

inline void endian_swap(int & x) {
	x = static_cast<int>(endian_swap(static_cast<uint>(x)));
}

#endif
