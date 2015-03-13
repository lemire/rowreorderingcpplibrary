/**
 * (c) Daniel Lemire 2010-2012
 * Apache License 2.0
 */

#ifndef ROWREORDERING_H_
#define ROWREORDERING_H_

#include <algorithm>
#include <vector>
#include <cassert>
using namespace std;

enum {
	SORTBUFFER = 50 * 1024 * 1024
};

namespace MultipleListsSortNS {

// convenience class used by MultipleLists
class LexicoOffset {
public:
	LexicoOffset(uint k, uint NumberOfColumns, vector<uint> & indexes) :
		K(k), mNumberOfColumns(NumberOfColumns), mIndexes(indexes) {
	}
	bool operator()(const vector<uint> & i, const vector<uint> & j) {
		for (uint k = K; k < mNumberOfColumns; ++k) {
			const uint thisk = mIndexes[k];
			if (i[thisk] < j[thisk])
				return true;
			if (i[thisk] > j[thisk])
				return false;
		}
		for (uint k = 0; k < K; ++k) {
			const uint thisk = mIndexes[k];
			if (i[thisk] < j[thisk])
				return true;
			if (i[thisk] > j[thisk])
				return false;
		}
		return false;
	}
	const uint K;
	const uint mNumberOfColumns;
	vector<uint> mIndexes;

};
class SortOnOneKey {
public:
	SortOnOneKey(uint k) :
		K(k) {
	}
	bool operator()(const vector<uint> & i, const vector<uint> & j) {
		return i[K] < j[K];
	}
	const uint K;
};

uint Hamming(const vector<uint> & i, const vector<uint> & j, uint N) {
	uint counter = 0;
	for (uint k = 0; k < N; ++k)
		if (i[k] != j[k])
			++counter;
	return counter;
}

uint unlinkAndFindBestNext(vector<vector<uint> > & Buffer, const uint pos,
		const uint NumberOfColumns) {
	const uint INVALID = UINT_MAX;
	uint bestHamming = INVALID;
	uint bestPos = INVALID;
	for (uint k = 0; k < NumberOfColumns; ++k) {
		const uint before = Buffer[pos][NumberOfColumns + 2 * k];
		const uint after = Buffer[pos][NumberOfColumns + 2 * k + 1];
		if (before < Buffer.size()) {
			const uint h =
					Hamming(Buffer[pos], Buffer[before], NumberOfColumns);
			if (h < bestHamming) {
				bestHamming = h;
				bestPos = before;
			}
			Buffer[before][NumberOfColumns + 2 * k + 1] = after;
		}
		if (after < Buffer.size()) {
			const uint h = Hamming(Buffer[pos], Buffer[after], NumberOfColumns);
			if (h < bestHamming) {
				bestHamming = h;
				bestPos = after;
			}
			Buffer[after][NumberOfColumns + 2 * k] = before;
		}
		//
	}
	return bestPos;

}

}
template<class C>
void MultipleListsSort(const typename C::iterator databegin,
		const typename C::iterator dataend, vector<uint> & indexes) {
	const uint n = dataend - databegin;
	const uint NumberOfColumns = databegin->size();
	if (NumberOfColumns * n == 0)
		return; // nothing to do
	const uint INVALID = UINT_MAX;
	vector<uint> tmp(3 * NumberOfColumns + 1, INVALID);
	const uint ID = 3 * NumberOfColumns;
	vector<vector<uint> > Buffer(n, tmp);
	for (typename C::iterator i = databegin; i != dataend; ++i) {
		copy(i->begin(), i->end(), Buffer[i - databegin].begin());
		Buffer[i - databegin][ID] = i - databegin; // ID
	}
	for (uint k = 0; k < NumberOfColumns; ++k) {
		MultipleListsSortNS::LexicoOffset l(k, NumberOfColumns, indexes);
		sort(Buffer.begin(), Buffer.end(), l);
		Buffer[0][NumberOfColumns + 2 * k + 1] = Buffer[1][ID];
		for (uint x = 1; x < Buffer.size() - 1; ++x) {
			Buffer[x][NumberOfColumns + 2 * k + 1] = Buffer[x + 1][ID];
			Buffer[x][NumberOfColumns + 2 * k] = Buffer[x - 1][ID];
		}
		Buffer[Buffer.size() - 1][NumberOfColumns + 2 * k]
				= Buffer[Buffer.size() - 2][ID];
	}
	tmp.clear();
	//tmp.resize(NumberOfColumns);
	MultipleListsSortNS::SortOnOneKey sook(ID);
	sort(Buffer.begin(), Buffer.end(), sook);
	// could start from random point, but let's go...
	uint location = 0;
	typename C::iterator writeiter = databegin;
	copy(Buffer[location].begin(), Buffer[location].begin() + NumberOfColumns,
			writeiter->begin());
	++writeiter;
	location = MultipleListsSortNS::unlinkAndFindBestNext(Buffer, location,
			NumberOfColumns);
	while (location != INVALID) {
		copy(Buffer[location].begin(),
				Buffer[location].begin() + NumberOfColumns, writeiter->begin());
		++writeiter;
		location = MultipleListsSortNS::unlinkAndFindBestNext(Buffer, location,
				NumberOfColumns);
	}
}

// this is an expensive Vortex sort, which is memory conscious
class Vortex {
public:
	Vortex(vector<uint> & indexes) :mIndexes(indexes){
			buffer1.reserve(indexes.size());
			buffer2.reserve(indexes.size());

	}
	Vortex() :mIndexes(){
	}
	Vortex& operator=(const Vortex & v) {
		mIndexes = v.mIndexes;
		buffer1.reserve(v.buffer1.size());
		buffer2.reserve(v.buffer2.size());
		return *this;
	}
	template<class RowType>
	bool operator()(const RowType & i, const RowType & j) const {
		buffer1.resize(i.size());
		buffer2.resize(j.size());
		for (uint k = 0; k < buffer1.size(); ++k) {
			const uint thisk = mIndexes[k];
			buffer1[k].first = i[thisk];
			buffer1[k].second = thisk;
			buffer2[k].first = j[thisk];
			buffer2[k].second = thisk;
		}
		bool order = true;
		sort(buffer1.begin(), buffer1.end());
		sort(buffer2.begin(), buffer2.end());
		for (uint k = 0; k < buffer1.size(); ++k) {
			if (buffer1[k].first < buffer2[k].first)
				return order;
			else if (buffer1[k].first > buffer2[k].first)
				return not (order);
			if (buffer1[k].second < buffer2[k].second)
				return order;
			else if (buffer1[k].second > buffer2[k].second)
				return not (order);
			order = not (order);
		}
		return false;// they are equal in fact
	}

	static vector<pair<uint, uint> > buffer1, buffer2;
	vector<uint> mIndexes;
};
vector<pair<uint, uint> > Vortex::buffer1;
vector<pair<uint, uint> > Vortex::buffer2;

#endif /* ROWREORDERING_H_ */
