/**
 * (c) Daniel Lemire 2010-2012
 * Apache License 2.0
 */

/**
 * Packing and unpacking can be quite a bit faster if you do it in batch. See
 * my blog post on this topic:
 *
 * How fast is bit packing?
 * http://lemire.me/blog/archives/2012/03/06/how-fast-is-bit-packing/
 */





#ifndef BITPACKING
#define BITPACKING
#include <cassert>
#include "util.h"




void unpack1(uint * in, uint *out) {
	for(uint k = 0; k<32; ++k) {
		*out = (*in & (1<<k))>>k;
		++out;
	}
}

void unpack2(uint * in, uint *out) {
	for(uint k = 0; k<16; ++k) {
		*out = (*in & (3<<(2*k)))>>(2*k);
		++out;
	}
	++in;
	for(uint k = 0; k<16; ++k) {
		*out = (*in & (3<<(2*k)))>>(2*k);
		++out;
	}
}

void unpack4(uint * in, uint *out) {
	for(uint k = 0; k<8; ++k) {
		*out = (*in & (15<<(4*k)))>>(4*k);
		++out;
	}
	++in;
	for(uint k = 0; k<8; ++k) {
		*out = (*in & (15<<(4*k)))>>(4*k);
		++out;
	}
	++in;
	for(uint k = 0; k<8; ++k) {
		*out = (*in & (15<<(4*k)))>>(4*k);
		++out;
	}
	++in;
	for(uint k = 0; k<8; ++k) {
		*out = (*in & (15<<(4*k)))>>(4*k);
		++out;
	}
}




const uint masks[33] = { (1U<<(0))-1U, (1U<<(1))-1U, (1U<<(2))-1U, (1U<<(3))-1U, (1U<<(4))-1U, (1U<<(5))-1U, (1U<<(6))-1U, (1U<<(7))-1U, (1U<<(8))-1U,
        (1U<<(9))-1U, (1U<<(10))-1U, (1U<<(11))-1U, (1U<<(12))-1U, (1U<<(13))-1U, (1U<<(14))-1U, (1U<<(15))-1U, (1U<<(16))-1U, (1U<<(17))-1U,
        (1U<<(18))-1U, (1U<<(19))-1U, (1U<<(20))-1U, (1U<<(21))-1U, (1U<<(22))-1U, (1U<<(23))-1U, (1U<<(24))-1U, (1U<<(25))-1U, (1U<<(26))-1U,
        (1U<<(27))-1U, (1U<<(28))-1U, (1U<<(29))-1U, (1U<<(30))-1U, (1U<<(31))-1U,0xFFFFFFFFU
      };


template<class word=uint>
class BitPacking {
public:

    BitPacking(word * p) : P( p ), inwordpointer(0) {}

    inline void writeWithNoOverflow(const uint value, const int bits) {
    	*P |= (value<<inwordpointer);
    	const int firstpass = sizeof(word) * 8 - inwordpointer;
        if(bits<firstpass) {
            inwordpointer+=bits;
        } else {
            ++P;
            assert(bits-firstpass == 0);
            inwordpointer=0;
        }
    }
    inline void write(const uint value, const int bits) {
    	*P |= (value<<inwordpointer);
    	const int firstpass = sizeof(word) * 8 - inwordpointer;
        if(bits<firstpass) {
            inwordpointer+=bits;
        } else {
            ++P;
            if(firstpass<static_cast<int>(sizeof(word)*8)) *P |= (value>> firstpass);
            inwordpointer=bits-firstpass;
        }
    }
    template<class Iterator>
    inline void writeWord(const Iterator & begin, const Iterator & end, const int bits) {
    	assert(begin<=end);
    	assert(inwordpointer == 0);
    	for(Iterator i = begin; i != end; ++i) {
    		*P |= ((*i)<<inwordpointer);
    		inwordpointer+=bits;
    	}
    	++P;
    	inwordpointer = 0;
    }


    word * P;
    int inwordpointer;
};



template<class word=uint>
class BitUnpacking {
public:
    BitUnpacking(const word * p) : P( p ), inwordpointer(0) {
    }
    void reinitializeTo(const word * p) {
    	P=p;
    	inwordpointer = 0;
    }

    inline void skip(const int bits) {
    	inwordpointer += bits;
    	const uint wordstoskip = inwordpointer / ( sizeof(word)*8 );
    	P+= wordstoskip;
    	inwordpointer -= sizeof(word)*8 * wordstoskip;
    }
    inline uint readWithNoOverflow(const int bits) {
    	assert(bits<=sizeof(word) * 8 );
        uint answer = ((*P)>>inwordpointer) &  masks[bits];
        const int firstpass = sizeof(word) * 8 - inwordpointer;
        if(bits<firstpass) {
            inwordpointer+=bits;
        } else {
            ++P;
            assert(bits-firstpass == 0);
            inwordpointer=0;
        }
        return answer;
    }

    inline uint readNextBit() {
    	uint answer = ((*P)>>inwordpointer) &  1;
    	if(1+inwordpointer < sizeof(word) * 8 )
    		++inwordpointer;
    	else {
    		inwordpointer = 0;
    		++P;
    	}
    	return answer;
    }

    /*
     * This is a bit faster than repeatedly calling read.
     */
    template<class outiter>// outiter might be uint *
    inline void readBunchFast(const int bits,  const outiter outbegin, const outiter outend) {
    	assert(bits<=sizeof(word) * 8 );
    	for(outiter i = outbegin;i!=outend;) {
    		for(;inwordpointer<sizeof(word) * 8;inwordpointer+=bits) {
    			*i = ( (*P)>> (inwordpointer & 0x1f) )  &  masks[bits];
    			++i;
    			if(i==outend) break;
    		}
    		inwordpointer -=sizeof(word) * 8;
    		++P;
    		*(i-1) |= ((*P) & masks[inwordpointer]) << (bits - inwordpointer);
    	}

    }




    inline uint read(const int bits) {
    	assert(static_cast<uint>(bits)<=sizeof(word) * 8 );
        uint answer = ((*P)>>inwordpointer) &  masks[bits];
        const int firstpass = sizeof(word) * 8 - inwordpointer;
        assert(firstpass>=0);
        if(bits<firstpass) {
            inwordpointer+=bits;
        } else {
            ++P;
            assert(bits-firstpass<=static_cast<int>(sizeof(word) * 8) );
            if(firstpass<static_cast<int>(8*sizeof(word)))  answer |= ((*P) & masks[bits-firstpass]) << firstpass;
            inwordpointer=bits-firstpass;
        }
        return answer;
    }

    inline void readWord(vector<uint> & value, const int howmany, const int bits) {
    	assert(inwordpointer == 0);
    	const uint bufmask = masks[bits];
    	for(; inwordpointer<howmany*bits; inwordpointer+=bits) {
    		value.push_back ( ((*P)>>inwordpointer) &  bufmask);//masks[bits]);

    	}
    	++P;
    	inwordpointer = 0;
    }

    void endCurrentWord() {
        if(inwordpointer!=0) {
            ++P;
            inwordpointer = 0;
        }
    }
    const word * P;
    int inwordpointer;
};

#endif
