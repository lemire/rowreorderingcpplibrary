#ifndef DELTA
#define DELTA

/**
 * When I started coding this project, I designed generic column-oriented CODECs.
 * Even when they were block-oriented, I didn't make the block-orientation explicit.
 */

#include <set>
#include <map>
#include <sstream>
#include <limits>
#include <cassert>
#include <algorithm>
#include <stdexcept>
#include "ztimer.h"
#include "flatfile.h"
//#include "memorystores.h"
#include "bitpacking.h"
#include "util.h"

using namespace std;
using std::runtime_error;

#ifndef COLUMNTYPE
#define COLUMNTYPE
typedef vector<uint> columntype;
#endif

#ifndef HISTOCOUNTER_TYPE
#define HISTOCOUNTER_TYPE
typedef int HISTOCOUNTERTYPE;
#endif

class CODEC {
public:
	/*
	 * compress array in to array out, using blockpointers to record
	 * the location of the blocks.
	 */
	virtual void compress(const columntype & in, columntype& out, columntype & blockpointers)=0;
	virtual void uncompress(const columntype & in, columntype & out)=0;
	virtual string name() const = 0;
	virtual uint blocksize() const = 0;
	// random access in a compressed array
	virtual uint getValueAt(const columntype & in,
	const columntype & blockpointers, const uint index) = 0;
	virtual ~CODEC() {
	}
};

const bool AVOID_MODULO = true;

enum {
	OPTIMAL,
	TOPDOWN,
	APPROXIMATE,
	BITOPTIMAL,
	GREEDY,
	XORMODE = 1,
	DELTAMODE = 0,
	FROMBASE = 1,
	SUCCESSIVE = 0
};


/*******************************************************************
 *
 *
 *
 *BlockedDictCoding
 *
 *
 *
 *******************************************************************/

template<uint BLOCKSIZE>
class BlockedDictCoding: public CODEC {
public:
	BlockedDictCoding() {
	}

	uint blocksize() const {
		return BLOCKSIZE;
	}

	string name() const {
		ostringstream convert;
		convert << "BlockedDictCoding<" << BLOCKSIZE << ">";
		return convert.str();
	}

	void compressBlock(const columntype::const_iterator & begin,
			const columntype::const_iterator & end, columntype& out,
			const int L) {
		const uint oldsize = out.size();
		const uint extrasize = ceildiv((end - begin) * L, 8 * sizeof(uint));
		out.resize(oldsize + extrasize, 0);
		BitPacking<> bp(&out[oldsize]);
		for (columntype::const_iterator i = begin; i != end; ++i) {
			bp.write(*i, L);
		}
	}

	void compress(const columntype & in, columntype& out,
			columntype & blockpointers) {

		// will need this at decompression time
		out.push_back(in.size());
		const int L = maxbits(in);
		out.push_back(L);
		// first, just the full blocks
		for (columntype::const_iterator i = in.begin(); i < in.end(); i
				+= BLOCKSIZE) {
			blockpointers.push_back(&out.back() + 1-&out.front());
			if (i + BLOCKSIZE <= in.end())
				compressBlock(i, i + BLOCKSIZE, out, L);
			else
				compressBlock(i, in.end(), out, L);
		}
	}

	// random access in a compressed array
	uint getValueAt(const columntype & in, const columntype & blockpointers,
			const uint index) {
		const uint whichblock = index / blocksize();
		uint whereinblock = index % blocksize();
		const uint * startofblock = & in[blockpointers[whichblock]];
		const int L = static_cast<int> (in[1]);
		BitUnpacking<> bp(startofblock);
		bp.skip(L * whereinblock);
		return bp.read(L);
	}

	void uncompress(const columntype & in, columntype & out) {
		if (in.size() < 1) {
			cerr << "Something is wrong. Input array too small." << endl;
		}
		const uint size = in[0];
		const int L = static_cast<int> (in[1]);
		BitUnpacking<> bp(&in[2]);
		out.reserve(size);
		while (out.size() < size) {
			uint sizeofblock = size - out.size() > BLOCKSIZE ? BLOCKSIZE : size
					- out.size();
			while (sizeofblock-- > 0) {
				out.push_back(bp.read(L));
			}
			bp.endCurrentWord();
		}
		assert(out.size() == size);
		assert((bp.P == &in.back()) or (bp.P == &in[in.size()]));
	}
};

/*******************************************************************
 *
 *
 *
 *PrefixCoding
 *
 *
 *
 *******************************************************************/

template<uint BLOCKSIZE>
class PrefixCoding: public CODEC {
public:
	PrefixCoding() :
		bitsBLOCK(bits(BLOCKSIZE - 1)) {
	}

	uint blocksize() const {
		return BLOCKSIZE;
	}

	string name() const {
		ostringstream convert;
		convert << "PrefixCoding<" << BLOCKSIZE << ">";
		return convert.str();
	}

	void compressBlock(const columntype::const_iterator & begin,
			const columntype::const_iterator & end, columntype& out,
			const int L) {
		columntype::const_iterator i = begin;
		++i;
		while ((i != end) and (*i == *begin))
			++i;
		const uint runlength = i - begin;
		assert(bits(runlength - 1) <= bitsBLOCK);
		const uint oldsize = out.size();
		const uint extrasize = ceildiv(bitsBLOCK
				+ (end - begin - runlength + 1) * L, sizeof(uint) * 8);
		out.resize(oldsize + extrasize, 0);
		BitPacking<> bp(&out[oldsize]);
		bp.write(runlength - 1, bitsBLOCK);
		bp.write(*begin, L);
		for (; i != end; ++i) {
			bp.write(*i, L);
		}
	}

	void compress(const columntype & in, columntype& out,
			columntype & blockpointers) {

		// will need this at decompression time
		out.push_back(in.size());
		const int L = maxbits(in);
		out.push_back(L);
		// first, just the full blocks
		for (columntype::const_iterator i = in.begin(); i < in.end(); i
				+= BLOCKSIZE) {
			blockpointers.push_back(&out.back() + 1-&out.front());
			if (i + BLOCKSIZE <= in.end())
				compressBlock(i, i + BLOCKSIZE, out, L);
			else
				compressBlock(i, in.end(), out, L);
		}
	}

	// random access in a compressed array
	uint getValueAt(const columntype & in, const columntype & blockpointers,
			const uint index) {
		const uint whichblock = index / blocksize();
		uint whereinblock = index % blocksize();
		const uint * startofblock = & in[blockpointers[whichblock]];
		const int L = static_cast<int> (in[1]);
		BitUnpacking<> bp(startofblock);
		// ok, so, first is it part of the run?
		uint runlength = bp.read(bitsBLOCK) + 1;
		if (whereinblock < runlength)
			return bp.read(L);
		bp.skip(L + (whereinblock - runlength) * L);
		return bp.read(L);
	}

	void uncompress(const columntype & in, columntype & out) {
		if (in.size() < 1) {
			cerr << "Something is wrong. Input array too small." << endl;
		}
		const uint size = in[0];
		const int L = static_cast<int> (in[1]);
		BitUnpacking<> bp(&in[2]);
		out.reserve(size);
		while (out.size() < size) {
			uint sizeofblock = size - out.size() > BLOCKSIZE ? BLOCKSIZE : size
					- out.size();
			//
			uint runlength = bp.read(bitsBLOCK) + 1;
			assert(sizeofblock >= runlength);
			sizeofblock -= runlength;
			const uint val = bp.read(L);
			//while(runlength-- > 0) out.push_back(val);
			out.resize(out.size() + runlength, val);
			while (sizeofblock-- > 0) {
				out.push_back(bp.read(L));
			}
			bp.endCurrentWord();
		}
		assert(out.size() == size);
		assert((bp.P == &in.back()) or (bp.P == &in[in.size()]));
	}

	int bitsBLOCK;
};

/*******************************************************************
 *
 *
 *
 *SparseCoding
 *
 *
 *
 *******************************************************************/

/*template <class uword=uint>
 class TrivialBoolArray {
 public:
 TrivialBoolArray(const uint n):buffer(n / (sizeof(uword)*8) + (n % (sizeof(uword)*8) == 0 ? 0 : 1),0)//,sizeinbits(n)
 {	}
 void set(const uint pos) {
 buffer[pos/(sizeof(uword)*8) ] |= ( static_cast<uword>(1) << (pos % (sizeof(uword)*8) ) ) ;
 }
 bool get(const uint pos) const {
 return (buffer[pos/(sizeof(uword)*8) ] & ( static_cast<uword>(1) << (pos % (sizeof(uword)*8) ) )) != 0;
 }
 void reset(const uint n) {
 buffer.clear();
 buffer.resize(n / (sizeof(uword)*8) + (n % (sizeof(uword)*8) == 0 ? 0 : 1) , 0  );
 }
 vector<uword>  buffer;
 //uint sizeinbits;
 };*/

template<uint BLOCKSIZE>
class SparseCoding: public CODEC {
public:
	SparseCoding() {
	}
	string name() const {
		ostringstream convert;
		convert << "SparseCoding<" << BLOCKSIZE << ">";
		return convert.str();
	}

	uint blocksize() const {
		return BLOCKSIZE;
	}

	void compressBlock(const columntype::const_iterator & begin,
			const columntype::const_iterator & end, columntype& out,
			const int L, columntype & copy) {// TrivialBoolArray<> & bs,
		copy.resize(end - begin);
		std::copy(begin, end, copy.begin());
		sort(copy.begin(), copy.end());
		uint maxfreq = 0;
		uint maxvalue = 0;
		for (columntype::const_iterator z = copy.begin(); z != copy.end();) {
			const columntype::const_iterator initz = z;
			++z;
			const uint myvalue = *initz;
			for (;; ++z) {//z!=copy.end()
				if (z == copy.end())
					break;
				if (*z != myvalue)
					break;
			}
			const uint myfreq = z - initz;
			if (myfreq > maxfreq) {
				maxfreq = myfreq;
				maxvalue = myvalue;
			}

		}
		// at this point, I only need to worry about maxvalue
		//bs.buffer.clear();
		/*bs.reset(end - begin);
		 for (columntype::const_iterator i = begin; i != end; ++i) {
		 if (*i == maxvalue)
		 bs.set(i - begin);
		 }*/
		const uint oldsize = out.size();
		const uint extrasize = ceildiv((end - begin) + (end - begin - maxfreq
				+ 1) * L, sizeof(uint) * 8);//sizeof(uint)*8*2+
		out.resize(oldsize + extrasize, 0);
		BitPacking<> bp(&out[oldsize]);
		//assert(bs.buffer.size()==static_cast<uint>(ceildiv((end-begin),sizeof(uint)*8)));
		/*if ((end - begin) % (sizeof(uint) * 8) == 0) {
		 for (columntype::const_iterator i = bs.buffer.begin(); i
		 != bs.buffer.end(); ++i) {
		 bp.write(*i, 8 * sizeof(uint));
		 }
		 } else {
		 for (columntype::const_iterator i = bs.buffer.begin(); i
		 != bs.buffer.end() - 1; ++i) {
		 bp.write(*i, 8 * sizeof(uint));
		 }
		 bp.write(bs.buffer.back(), (end - begin) % (sizeof(uint) * 8));
		 }*/
		bp.write(maxvalue, L);
		for (columntype::const_iterator i = begin; i != end; ++i) {
			if (*i != maxvalue) {

				bp.write(*i << 1, L + 1);
			} else
				bp.writeWithNoOverflow(1U, 1);

		}
		assert((bp.P == &out.back()) or (bp.P == &out[out.size()]));
	}

	void compress(const columntype & in, columntype& out,
			columntype & blockpointers) {

		// will need this at decompression time
		out.push_back(in.size());
		const int L = maxbits(in);
		out.push_back(L);
		// first, just the full blocks
		//TrivialBoolArray<> bs(BLOCKSIZE);
		columntype copy(BLOCKSIZE);
		for (columntype::const_iterator i = in.begin(); i < in.end(); i
				+= BLOCKSIZE) {
			blockpointers.push_back(&out.back() + 1-&out.front());
			if (i + BLOCKSIZE <= in.end())
				compressBlock(i, i + BLOCKSIZE, out, L, copy);
			else
				compressBlock(i, in.end(), out, L, copy);
		}
	}

	// random access in a compressed array
	uint getValueAt(const columntype & in, const columntype & blockpointers,
			const uint index) {
		const uint whichblock = index / blocksize();
		uint whereinblock = index % blocksize();
		const uint * startofblock = & in[blockpointers[whichblock]];
		const int L = static_cast<int> (in[1]);
		BitUnpacking<> bp(startofblock);
		const uint popularvalue = bp.read(L);
		while (whereinblock-- > 0) {
			if (bp.readWithNoOverflow(1) == 0)
				bp.read(L);
		}
		if (bp.readWithNoOverflow(1) == 0)
			return bp.read(L);
		else
			return popularvalue;
	}

	void uncompress(const columntype & in, columntype & out) {
		if (in.size() < 1) {
			cerr << "Something is wrong. Input array too small." << endl;
		}
		const uint size = in[0];
		const int L = static_cast<int> (in[1]);
		BitUnpacking<> bp(&in[2]);
		out.reserve(size);
		//TrivialBoolArray<> bs(BLOCKSIZE);
		while (out.size() < size) {
			const uint sizeofblock = size - out.size() > BLOCKSIZE ? BLOCKSIZE
					: size - out.size();

			const uint popularvalue = bp.read(L);
			for (uint k = 0; k < sizeofblock; ++k) {
				if (bp.readWithNoOverflow(1) == 1)
					out.push_back(popularvalue);
				else {
					out.push_back(bp.read(L));
				}
			}
			bp.endCurrentWord();
		}
		assert(out.size() == size);
		assert((bp.P == &in.back()) or (bp.P == &in[in.size()]));
	}

};

/*******************************************************************
 *
 *
 *
 *IndirectCoding
 *
 *
 *
 *******************************************************************/

template<uint BLOCKSIZE>
class IndirectCoding: public CODEC {
public:
	IndirectCoding() :
		bitsBLOCK(bits(BLOCKSIZE - 1)) {
	}

	string name() const {
		ostringstream convert;
		convert << "IndirectCoding<" << BLOCKSIZE << ">";
		return convert.str();
	}
	uint blocksize() const {
		return BLOCKSIZE;
	}

	void compressBlock(const columntype::const_iterator & begin,
			const columntype::const_iterator & end, columntype& out,
			const int L) {
		typedef std::tr1::unordered_map<uint, uint> mymap;
		mymap values;
		for (columntype::const_iterator i = begin; i != end; ++i) {
			if (values.find(*i) == values.end())
				values[*i] = 0;
		}
		// ok, so total storage will be
		const uint oldsize = out.size();
		const int bitsvalue = bits(values.size() - 1);
		const uint extrasize = ceildiv(bitsBLOCK + (values.size()) * L
				+ bitsvalue * (end - begin), sizeof(uint) * 8);
		out.resize(oldsize + extrasize, 0);
		BitPacking<> bp(&out[oldsize]);
#ifndef NDEBUG
		uint howmanybits = bitsBLOCK;
#endif
		assert(bits(values.size() - 1) <= bitsBLOCK);
		bp.write(values.size() - 1, bitsBLOCK);
		uint mycounter = 0;
		for (mymap::iterator i = values.begin(); i != values.end(); ++i) {
#ifndef NDEBUG
			howmanybits += L;
#endif
			bp.write(i->first, L);
			i->second = mycounter;
			++mycounter;
		}
		for (columntype::const_iterator i = begin; i != end; ++i) {
#ifndef NDEBUG
			howmanybits += bitsvalue;
#endif
			assert(values.find(*i) != values.end());
			assert(values[*i] < values.size());
			assert(bits(values[*i]) <= bitsvalue);
			bp.write(values[*i], bitsvalue);
		}
		assert(howmanybits == bitsBLOCK + (values.size()) * L + bitsvalue
				* (end - begin));
	}

	void compress(const columntype & in, columntype& out,
			columntype & blockpointers) {

		// will need this at decompression time
		out.push_back(in.size());
		const int L = maxbits(in);
		out.push_back(L);
		// first, just the full blocks
		for (columntype::const_iterator i = in.begin(); i < in.end(); i
				+= BLOCKSIZE) {
			blockpointers.push_back(&out.back() + 1-&out.front());
			if (i + BLOCKSIZE <= in.end())
				compressBlock(i, i + BLOCKSIZE, out, L);
			else
				compressBlock(i, in.end(), out, L);
		}
	}

	// random access in a compressed array
	uint getValueAt(const columntype & in, const columntype & blockpointers,
			const uint index) {
		const uint whichblock = index / blocksize();
		uint whereinblock = index % blocksize();
		const uint * startofblock = & in[blockpointers[whichblock]];
		const int L = static_cast<int> (in[1]);
		BitUnpacking<> bp(startofblock);
		uint setsize = bp.read(bitsBLOCK) + 1;
		const int bitsvalue = bits(setsize - 1);
		bp.skip(setsize * L + whereinblock * bitsvalue);
		const uint indexofanswer = bp.read(bitsvalue);
		bp.reinitializeTo(startofblock);
		bp.skip(bitsBLOCK + indexofanswer * L);// we rewind
		return bp.read(L);
	}

	void uncompress(const columntype & in, columntype & out) {
		if (in.size() < 1) {
			cerr << "Something is wrong. Input array too small." << endl;
		}
		const uint size = in[0];
		const int L = static_cast<int> (in[1]);
		BitUnpacking<> bp(&in[2]);
		out.reserve(size);
		vector<uint> buffer;
		buffer.reserve(BLOCKSIZE);
		while (out.size() < size) {
			uint sizeofblock = size - out.size() > BLOCKSIZE ? BLOCKSIZE : size
					- out.size();
#ifndef NDEBUG
			const uint wantedsizeofblock = sizeofblock;
			uint counter = 0;
#endif
			//
			uint setsize = bp.read(bitsBLOCK) + 1;
			const int bitsvalue = bits(setsize - 1);
			if(bitsvalue>0) {
				buffer.clear();
				while (setsize-- > 0) {
					buffer.push_back(bp.read(L));
				}
				while (sizeofblock-- > 0) {
#ifndef NDEBUG
					counter++;
#endif
					out.push_back(buffer.at(bp.read(bitsvalue)));
				}
#ifndef NDEBUG
				assert(counter == wantedsizeofblock);
#endif
			} else {
				out.resize(out.size()+sizeofblock,bp.read(L));
			}
			bp.endCurrentWord();
		}
		assert(out.size() == size);
		assert((bp.P == &in.back()) or (bp.P == &in[in.size()]));
	}

	int bitsBLOCK;
};



/*******************************************************************
 *
 *
 *
 *
 *
 *SimpleBlockedRunLengthEncoding
 *
 *******************************************************************/

template<uint BLOCKSIZE>
class SimpleBlockedRunLengthEncoding: public CODEC {
public:
	string name() const {
		ostringstream convert;
		convert << "SimpleBlockedRunLengthEncoding<" << BLOCKSIZE << ">";
		return convert.str();
	}

	static const int bitsBLOCKSIZE;

	uint blocksize() const {
		return BLOCKSIZE;
	}

	uint countNumberOfRuns(const columntype::const_iterator & i,
			const columntype::const_iterator & endpoint) {
		uint numberofruns = 0;
		for (columntype::const_iterator j(i); j != endpoint;) {
			const uint val = *j;
			columntype::const_iterator start(j);
			++j;
			for (; j != endpoint; ++j)
				if (*j != val)
					break;
			//const uint runlength = j-start;
			++numberofruns;
		}
		return numberofruns;
	}

	void compressBlock(columntype::const_iterator & i,
			columntype::const_iterator & endpoint, columntype& out,
			const int L) {
		const uint numberofruns = countNumberOfRuns(i, endpoint);
		const uint oldsize = out.size();
		out.resize(oldsize + ceildiv(numberofruns * (L + bitsBLOCKSIZE), 8
				* sizeof(uint)));
		BitPacking<> bp(&out[oldsize]);
		for (; i != endpoint;) {
			const uint val = *i;
			columntype::const_iterator start(i);
			++i;
			for (; i != endpoint; ++i)
				if (*i != val)
					break;
			const uint runlength = i - start;
			bp.write(runlength - 1, bitsBLOCKSIZE);
			bp.write(val, L);

		}
	}

	void compress(const columntype & in, columntype& out,
			columntype & blockpointers) {

		const int L = maxbits(in);
		out.resize(2, 0);
		out[0] = in.size();
		out[1] = L;
		//const uint maxcounterbits = bits(bits(BLOCKSIZE-1));
		for (columntype::const_iterator i = in.begin(); i < in.end();) {
			blockpointers.push_back(&out.back() + 1-&out.front());
			columntype::const_iterator endpoint = i + BLOCKSIZE;
			if (endpoint > in.end())
				endpoint = in.end();
			compressBlock(i, endpoint, out, L);
		}
	}

	// random access in a compressed array
	uint getValueAt(const columntype & in, const columntype & blockpointers,
			const uint index) {
		const uint whichblock = index / blocksize();
		uint whereinblock = index % blocksize();
		const uint * startofblock = & in[blockpointers[whichblock]];
		const int L = static_cast<int> (in[1]);
		BitUnpacking<> bp(startofblock);
		//const uint counterbit = bp.read(maxcounterbits);
		uint runlength = bp.read(bitsBLOCKSIZE) + 1;
		while (whereinblock >= runlength) {
			whereinblock -= runlength;
			bp.skip(L);
			runlength = bp.read(bitsBLOCKSIZE) + 1;
		}
		return bp.read(L);
	}

	void uncompress(const columntype & in, columntype & out) {
		const uint size = in[0];
		out.reserve(size);
		const int L = static_cast<int> (in[1]);
		//const uint maxcounterbits = bits(bits(BLOCKSIZE-1));
		BitUnpacking<> bp(&in[2]);
		while (out.size() < size) {
			//const uint counterbit = bp.read(maxcounterbits);
			uint targetsize = out.size() + BLOCKSIZE > size ? size : out.size()
					+ BLOCKSIZE;

			while (out.size() < targetsize) {
				const uint runlength = bp.read(bitsBLOCKSIZE) + 1;
				const uint val = bp.read(L);
				//out.insert(out.end(),runlength,val);
				//while(runlength-->0) out.push_back(val);
				out.resize(out.size() + runlength, val);
			}

			bp.endCurrentWord();
		}
	}

	static const uint maxcounterbits;

};

template<uint BLOCKSIZE>
const int SimpleBlockedRunLengthEncoding<BLOCKSIZE>::bitsBLOCKSIZE = bits(
		BLOCKSIZE - 1);

/*******************************************************************
 *
 *
 *
 *
 *
 *TripleBlockedRunLengthEncoding
 *
 *******************************************************************/

template<uint BLOCKSIZE>
class TripleBlockedRunLengthEncoding: public CODEC {
public:
	string name() const {
		ostringstream convert;
		convert << "TripleBlockedRunLengthEncoding<" << BLOCKSIZE << ">";
		return convert.str();
	}

	static const int bitsBLOCKSIZE;

	uint blocksize() const {
		return BLOCKSIZE;
	}

	uint countNumberOfRuns(const columntype::const_iterator & i,
			const columntype::const_iterator & endpoint) {
		uint numberofruns = 0;
		for (columntype::const_iterator j(i); j != endpoint;) {
			const uint val = *j;
			columntype::const_iterator start(j);
			++j;
			for (; j != endpoint; ++j)
				if (*j != val)
					break;
			++numberofruns;
		}
		return numberofruns;
	}

	void compressBlock(columntype::const_iterator & i,
			columntype::const_iterator & endpoint, columntype& out,
			const int L) {
		const uint numberofruns = countNumberOfRuns(i, endpoint);
		assert(numberofruns>0);
		const uint oldsize = out.size();
		out.resize(oldsize + ceildiv(numberofruns * (L + 2 * bitsBLOCKSIZE) + bitsBLOCKSIZE, 8
				* sizeof(uint)));
		BitPacking<> bp(&out[oldsize]);
		bp.write(numberofruns-1, bitsBLOCKSIZE);
		const columntype::const_iterator startofblock(i);
		for (; i != endpoint;) {
			const uint val = *i;
			columntype::const_iterator start(i);
			++i;
			for (; i != endpoint; ++i)
				if (*i != val)
					break;
			const uint runlength = i - start;
			bp.write(start - startofblock, bitsBLOCKSIZE);
			bp.write(runlength - 1, bitsBLOCKSIZE);
			bp.write(val, L);
		}
		assert((bp.P==&out.back()) or ((bp.P==&out.back() + 1) and (bp.inwordpointer==0) ));
	}

	void compress(const columntype & in, columntype& out,
			columntype & blockpointers) {

		const int L = maxbits(in);
		out.resize(2, 0);
		out[0] = in.size();
		out[1] = L;
		for (columntype::const_iterator i = in.begin(); i < in.end();) {
			blockpointers.push_back(&out.back() + 1-&out.front());
			columntype::const_iterator endpoint = i + BLOCKSIZE;
			if (endpoint > in.end())
				endpoint = in.end();
			compressBlock(i, endpoint, out, L);
		}
	}

	// random access in a compressed array
	uint getValueAt(const columntype & in, const columntype & blockpointers,
			const uint index) {
		const uint whichblock = index / blocksize();
		uint whereinblock = index % blocksize();
		const uint * startofblock = & in[blockpointers[whichblock]];
		const int L = static_cast<int> (in[1]);
		BitUnpacking<> bp(startofblock);

		const uint sizeofrun = 2 * bitsBLOCKSIZE + L;
		const uint numberofruns = bp.read(bitsBLOCKSIZE) + 1;
		assert(numberofruns>0);
		uint firstrun = 0;
		uint lastrun = numberofruns - 1;
		while (true) {
			// we try in the middle
			assert(lastrun >= firstrun);
			uint middle = firstrun + (lastrun - firstrun) / 2;
			bp.reinitializeTo(startofblock);

			bp.skip(middle * sizeofrun + bitsBLOCKSIZE);
			const uint whereisit = bp.read(bitsBLOCKSIZE);
			if (whereisit > whereinblock) {
				lastrun = middle - 1;
				assert(lastrun >= firstrun);
				continue;
			}
			const uint howlongisit = bp.read(bitsBLOCKSIZE) + 1;
			if (whereisit + howlongisit - 1 < whereinblock) {
				bp.skip(L);
				firstrun = middle + 1;
				assert(lastrun >= firstrun);

				continue;
			}
			return bp.read(L);
		}
	}

	void uncompress(const columntype & in, columntype & out) {
		const uint size = in[0];
		out.reserve(size);
		const int L = static_cast<int> (in[1]);
		BitUnpacking<> bp(&in[2]);
		while (out.size() < size) {
			uint targetsize = out.size() + BLOCKSIZE > size ? size : out.size()
					+ BLOCKSIZE;
			bp.skip(bitsBLOCKSIZE);//numberofruns
			while (out.size() < targetsize) {
				bp.skip(bitsBLOCKSIZE);
				const uint runlength = bp.read(bitsBLOCKSIZE) + 1;

				const uint val = bp.read(L);
				out.resize(out.size() + runlength, val);
			}
			bp.endCurrentWord();
		}
	}

	static const uint maxcounterbits;

};

template<uint BLOCKSIZE>
const int TripleBlockedRunLengthEncoding<BLOCKSIZE>::bitsBLOCKSIZE = bits(
		BLOCKSIZE - 1);

#endif
