/**
 * (c) Daniel Lemire 2010-2012
 * Apache License 2.0
 */

#ifndef COLUMNWIDECODECS_H_
#define COLUMNWIDECODECS_H_

#include <limits.h>
#include "columncodecs.h"

#ifndef COLUMNTYPE
#define COLUMNTYPE
typedef vector<uint> columntype;
#endif





class SimpleCODEC {
public:
    virtual void compress(const columntype & in, columntype& out)=0;
    virtual void uncompress(const columntype & in, columntype & out)=0;
    virtual string name() const = 0;
    virtual ~SimpleCODEC() {}
};



template<class SimpleCODECType>
class CODECWrapper : public CODEC{
public:
	CODECWrapper() : mCO() {}

	void compress(const columntype & in, columntype& out, columntype &) {
		 mCO.compress(in,out);
	}
	void uncompress(const columntype & in, columntype & out) {
		mCO.uncompress(in,out);
	}
	string name() const {
		return mCO.name();
	}
	uint blocksize() const {return 0;}
	// random access in a compressed array
	uint getValueAt(const columntype & ,
	const columntype & , const uint) {return 0;};
	virtual ~CODECWrapper() {
	}

    SimpleCODECType mCO;

};


// convenient wrapper class
template<class SimpleCODECType>
class SimpleCODECWrapper : public SimpleCODEC{
public:
	SimpleCODECWrapper() : mCO() {}

    void compress(const columntype & in, columntype& out) {
    	 columntype  blockpointers;
    	 mCO.compress(in,out,blockpointers);
    }
    void uncompress(const columntype & in, columntype & out) {
    	mCO.uncompress(in,out);
    }
    string name() const {return mCO.name();};
    ~SimpleCODECWrapper() {}

    SimpleCODECType mCO;
};


/*******************************************************************
 *
 *
 *
 *DictCODEC
 *
 *
 *
 *******************************************************************/

class DictCODEC : public SimpleCODEC {
public:
    DictCODEC() {}

    string name() const {
        return "DictCODEC";
    };

    void compress(const columntype & in, columntype& out) {
        int maxlog2 = maxbits(in);
        out.resize(ceildiv(in.size()*maxlog2, 8*sizeof(uint))  +2,0);
        out[0] = maxlog2;
        out[1] = in.size();
        __pack(in,&out[2],maxlog2);
    }


    void uncompress(const columntype & in, columntype & out) {
        if(in.size()<2) {
            cerr<<"Something is wrong. Input array too small."<<endl;
        }
        int maxlog2 = in[0];
        uint size = in[1];
        const uint * pin = &in[2];
        __unpack(pin,out,size, maxlog2);
    }



    // convenience method
    void __pack(const columntype & in, uint * __restrict__ pout, int bits) {
        BitPacking<uint> bp(pout);
        const uint *  __restrict__ pin = & in[0];
        const uint * const pinend = pin + in.size();
        for(; pin!=pinend; ++pin)
            bp.write(*pin,bits);
    }

    // convenience method
    void __unpack(const uint * __restrict__ pin, columntype & out, uint size, int bits) {
        BitUnpacking<> bp(pin);
        out.reserve(size);
        while(out.size() < size)
            out.push_back(bp.read(bits));
    }

};


/*******************************************************************
 *
 *
 *
 *NaiveDictCODEC
 *
 *
 *
 *******************************************************************/

class NaiveDictCODEC : public SimpleCODEC {
public:
	NaiveDictCODEC() {}

    string name() const {
        return "NaiveDictCODEC";
    };

    void compress(const columntype & in, columntype& out) {
        int maxlog2 = maxbits(in);
        if(maxlog2<=8) maxlog2=8;
        else if((maxlog2>8) and (maxlog2<=16)) maxlog2=16;
        else maxlog2=32;
        out.resize(ceildiv(in.size()*maxlog2, 8*sizeof(uint))  +2,0);
        out[0] = maxlog2;
        out[1] = in.size();
        if(maxlog2 == 8) {
        	unsigned char * pout = reinterpret_cast<unsigned char *>(&out[2]);
        	for(columntype::const_iterator i = in.begin(); i!=in.end(); ++i) {
        		*pout = static_cast<unsigned char>(*i);
        		++pout;
        	}
        } else if(maxlog2 == 16) {
        	unsigned short * pout = reinterpret_cast<unsigned short *>(&out[2]);
        	for(columntype::const_iterator i = in.begin(); i!=in.end(); ++i) {
        		*pout = static_cast<unsigned short>(*i);
        		++pout;
        	}
        } else {
        	memcpy(&out[2],&in[0],in.size()*sizeof(uint));
        	/*vector<uint>::iterator pout = out.begin()+2;
        	for(vector<uint>::const_iterator i = in.begin(); i!=in.end(); ++i,++pout) {
        		*pout = *i;
        	}*/
        }
    }


    void uncompress(const columntype & in, columntype & out) {
        if(in.size()<2) {
            cerr<<"Something is wrong. Input array too small."<<endl;
        }
        int maxlog2 = in[0];
        uint size = in[1];
        out.resize(size);
        if(maxlog2 == 8) {
        	const unsigned char * pin = reinterpret_cast<const unsigned char *>(&in[2]);
        	for(columntype::iterator i = out.begin(); i!=out.end(); ++i) {
        		*i = *(pin);
        		++pin;
        	}
        } else if(maxlog2 == 16) {
        	const unsigned short * pin = reinterpret_cast<const unsigned short *>(&in[2]);
        	for(columntype::iterator i = out.begin(); i!=out.end(); ++i) {
        		*i = *(pin);
        		++pin;
        	}
        } else {
        	memcpy(&out[0],&in[2],out.size()*sizeof(uint));
        	/*vector<uint>::const_iterator j = in.begin()+2;
        	for(vector<uint>::iterator i = out.begin(); i!=out.end(); ++i,++j) {
        		*i = *j;
        	}*/
        }
    }



    // convenience method
    void __pack(const columntype & in, uint * __restrict__ pout, int bits) {
        BitPacking<uint> bp(pout);
        const uint *  __restrict__ pin = & in[0];
        const uint * const pinend = pin + in.size();
        for(; pin!=pinend; ++pin)
            bp.write(*pin,bits);
    }

    // convenience method
    void __unpack(const uint * __restrict__ pin, columntype & out, uint size, int bits) {
        BitUnpacking<> bp(pin);
        out.reserve(size);
        while(out.size() < size)
            out.push_back(bp.read(bits));
    }

};






/*******************************************************************
 *
 *
 *RunLengthEncoding
 *
 *
 *
 *
 *******************************************************************/


template<int TYPE=OPTIMAL>
class RunLengthEncoding  : public SimpleCODEC {
public:
    string name() const {
    	if(TYPE==OPTIMAL)
    		return "OptimizedRunLengthEncoding";
    	else if(TYPE==BITOPTIMAL)
            return "BitOptimizedRunLengthEncoding";

    }

    void computeRunHistogram(const columntype & in, map<uint,uint> & histo) const {
        for(columntype::const_iterator i = in.begin(); i!= in.end(); ) {
            const uint val = *i;
            columntype::const_iterator start (i);
            ++i;
            for(; i!=in.end(); ++i)
                if(*i != val) break;
            const uint runlength = i-start;
            histo[runlength] +=1;
       }
    }


    uint computeOptimalRunBits(const columntype & in,const int L, uint & bestcost) const {
        map<uint,uint>  histo;
        computeRunHistogram(in, histo);
        const uint MaxCounterWidth = bits(histo.rbegin()->first);
        int bestbit = -1;
        bestcost = UINT_MAX;// cost
        for(uint k = 1; k<=MaxCounterWidth; ++k) {
            uint thiscost = 0;
            const uint maxvalue = (1U << k) ;
            for(map<uint,uint>::const_iterator i = histo.begin() ; i!=histo.end(); ++i) {
                const uint runlength = i->first;
                const uint numberofruns = i->second;
                const uint numberofrunsgenerated = ceildiv(runlength,maxvalue);
                thiscost += numberofrunsgenerated * numberofruns * (k + L);
            }
            if(thiscost < bestcost) {
                bestbit = k;
                bestcost = thiscost;
            }
        }
        assert(bestbit >= 1);
        return bestbit;
    }
    void computeBitHistogram(const columntype & in, columntype & histo) const {
        for(columntype::const_iterator i = in.begin(); i!= in.end(); ) {
            const uint val = *i;
            columntype::const_iterator start (i);
            ++i;
            for(; i!=in.end(); ++i)
                if(*i != val) break;
            const uint runlength = i-start;
            histo[bits(runlength)] +=1;
       }
    }

    uint computeBitOptimalRunBits(const columntype & in,const int L, uint & bestcost) const {
        vector<uint>  histo(sizeof(uint)*8+1,0);
        computeBitHistogram(in, histo);
        uint maxnonzero =0;
        for(uint k = 0; k<histo.size();++k) if(histo[k]!=0) maxnonzero = k;
        int bestbit = -1;
        bestcost = UINT_MAX;// cost
        for(uint k = 1; k<maxnonzero+1; ++k) {
            uint thiscost = 0;
            const uint maxvalue = (1U << k) ;
            for(uint K = 0; K<maxnonzero+1;++K ) {
                const uint numberofrunsgenerated = ceildiv(1<<K,maxvalue);
                thiscost += numberofrunsgenerated * histo[K] * (k + L);//could do a bit better with modelling
            }
            if(thiscost < bestcost) {
                bestbit = k;
                bestcost = thiscost;
            }
        }
        assert(bestbit >= 1);
        return bestbit;
    }


    void compress(const columntype & in, columntype& out) {
        const int L = maxbits(in);
        uint bestcost (0);
        const uint counterbit = TYPE==OPTIMAL ? computeOptimalRunBits(in,L,bestcost) : computeBitOptimalRunBits(in,L,bestcost);
        const uint maxrunvalue = (1U << counterbit) ;
        const uint numberofoutputwords = ceildiv(bestcost, 8*sizeof(uint));
        if(TYPE==OPTIMAL)
        	out.resize(3+numberofoutputwords);
        else if(TYPE== BITOPTIMAL)
        	out.resize(3+in.size());// bit optimality is only approximative
        out[0] = in.size();
        out[1] = counterbit;
        out[2] = L;
        BitPacking<> bp(&out[3]);
#ifndef NDEBUG
        uint counter = 0;
#endif
        for(columntype::const_iterator i = in.begin(); i!= in.end(); ) {
            const uint val = *i;
            columntype::const_iterator start (i);
            ++i;
            for(; i!=in.end(); ++i)
                if(*i != val) break;
            uint runlength = i-start;
#ifndef NDEBUG
            const uint predictednumber = ceildiv(runlength,maxrunvalue);
            uint num = 0;
#endif
            while(runlength>0) {
#ifndef NDEBUG
                ++num;
#endif
                const uint thisrunlength = runlength >= maxrunvalue ? maxrunvalue : runlength;
                assert(thisrunlength<=runlength);
                runlength -= thisrunlength;
                assert(bits(thisrunlength-1)<=static_cast<int>(counterbit));
                bp.write(thisrunlength-1,counterbit);
                bp.write(val,L);
#ifndef NDEBUG
                counter+= L+counterbit;
#endif
            }
            assert(num == predictednumber);
        }
        if(TYPE==OPTIMAL)
        	assert(counter == bestcost);
        else if(TYPE== BITOPTIMAL) {
        	if(bp.inwordpointer>0)
        		out.resize(bp.P-&out[0]+1);
        	else
        		out.resize(bp.P-&out[0]);
        }
    }

    void uncompress(const columntype & in, columntype & out) {
        if(in.size()<3) {
            cerr<<"Something is wrong. Input array too small."<<endl;
        }
        const uint size = in[0];
        const uint counterbit = in[1];
        const uint L = in[2];
        BitUnpacking<> bp(&in[3]);
        out.reserve(size);
        if(counterbit>5) {
        	while(out.size()<size) {
            	const uint runlength = bp.read(counterbit) + 1;
            	const uint value = bp.read(L);
            	out.resize(out.size() + runlength,value);
        	}
       } else if(counterbit>=1) {
        	while(out.size()<size) {
            	uint runlength = bp.read(counterbit) + 1;
            	const uint value = bp.read(L);
            	while(runlength-->0)
            		out.push_back(value);
        	}

        } else {
        	while(out.size()<size) {
        		out.push_back(bp.read(L));
        	}
        }
        assert(out.size() == size);
        assert((bp.P==&in.back()) or (bp.P==&in[in.size()] ));
    }
};


/**
 * Naive rle
 */


class NaiveRunLengthEncoding  : public SimpleCODEC {
public:
    string name() const {
    	return "NaiveRunLengthEncoding";
    }


    void compress(const columntype & in, columntype& out) {
        const int L = maxbits(in);
        const uint runcount = runCount(in);
        const uint counterbit = bits(in.size());
        const uint numberofoutputwords = ceildiv(runcount * (L+2*counterbit), 8*sizeof(uint));
        out.resize(3+numberofoutputwords);
        out[0] = in.size();
        out[1] = counterbit;
        out[2] = L;
        BitPacking<> bp(&out[3]);
        for(columntype::const_iterator i = in.begin(); i!= in.end(); ) {
            const uint val = *i;
            columntype::const_iterator start (i);
            ++i;
            for(; i!=in.end(); ++i)
                if(*i != val) break;
            bp.write(i-in.begin(),counterbit);
            bp.write(i-start,counterbit);
            bp.write(val,L);
        }
    }

    void uncompress(const columntype & in, columntype & out) {
        if(in.size()<3) {
            cerr<<"Something is wrong. Input array too small."<<endl;
        }
        const uint size = in[0];
        const uint counterbit = in[1];
        const uint L = in[2];
        BitUnpacking<> bp(&in[3]);
        out.reserve(size);
        while(out.size()<size) {
        	 bp.read(counterbit) ; // don't need this
        	 const uint runlength = bp.read(counterbit) ;
             const uint value = bp.read(L);
             out.resize(out.size() + runlength,value);
        }
        assert(out.size() == size);
        assert((bp.P==&in.back()) or (bp.P==&in[in.size()] ));
    }
};
#endif /* COLUMNWIDECODECS_H_ */
