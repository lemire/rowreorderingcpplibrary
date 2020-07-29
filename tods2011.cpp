/**
 * (c) Daniel Lemire 2010-2012
 * Apache License 2.0
 */
#include <iostream>
#include <sstream>
#include <string>
#include <getopt.h>
#include <string.h>
#include <memory>

#include "externalvector.h"

#include "lzocodec.h"
#include "flatfile.h"
#include "columnwidecodecs.h"
#include "ztimer.h"
#include "stxxlmemorystores.h"

using namespace std;


vector<shared_ptr<SimpleCODEC> > myalgos;
enum {
	BLOCKSIZE = 128
};

void initAlgos() {
	myalgos.clear();
	shared_ptr<SimpleCODEC> tmp(new DictCODEC());
	myalgos.push_back(tmp);

	tmp.reset(new SimpleCODECWrapper<SparseCoding<BLOCKSIZE> > ());
	myalgos.push_back(tmp);
	tmp.reset(new SimpleCODECWrapper<IndirectCoding<BLOCKSIZE> > ());
	myalgos.push_back(tmp);
	tmp.reset(new SimpleCODECWrapper<PrefixCoding<BLOCKSIZE> > ());
	myalgos.push_back(tmp);

	tmp.reset(new lzo());
	myalgos.push_back(tmp);
	tmp.reset(new NaiveDictCODEC());
	myalgos.push_back(tmp);
	tmp.reset(new RunLengthEncoding<> ());
	myalgos.push_back(tmp);
	tmp.reset(new NaiveRunLengthEncoding());
	myalgos.push_back(tmp);
}

class Results {
public:
	Results(string n) :
		sizeinmb(), name(n), compressiontime(), decompressiontime() {
	}
	vector<double> sizeinmb;
	string name;
	vector<double> compressiontime;
	vector<double> decompressiontime;
	void add(double cr, double ct, double dt) {
		sizeinmb.push_back(cr);
		compressiontime.push_back(ct);
		decompressiontime.push_back(dt);
	}
	string justtotal() {
		stringstream ss;
		ss << name << "\t";
		double cr(0), ct(0), dt(0);
		for (uint k = 0; k < sizeinmb.size(); ++k) {
			cr += sizeinmb.at(k);
			ct += compressiontime.at(k);
			dt += decompressiontime.at(k);
		}
		ss << cr << " " << ct << " " << dt;
		return ss.str();
	}

	operator const string() {
		stringstream ss;
		ss << name << "\t";
		double cr(0), ct(0), dt(0);
		for (uint k = 0; k < sizeinmb.size(); ++k) {
			ss << sizeinmb.at(k) << "\t" << compressiontime.at(k) << "\t"
					<< decompressiontime.at(k) << "\t";
			cr += sizeinmb.at(k);
			ct += compressiontime.at(k);
			dt += decompressiontime.at(k);
		}
		ss << cr << " " << ct << " " << dt;
		return ss.str();
	}
};

template<class MyNaiveColumnStore>
void testCodec(SimpleCODEC & mycodec, MyNaiveColumnStore & n,
		vector<Results> & v, const uint smallsetrepeats) {
	const uint uncompressedsize = n.size();
	cout << "# computing " << mycodec.name() << " ... " << endl;
	if (uncompressedsize == 0)
		return;
	Results r(mycodec.name());
	for (uint columnindex = 0; columnindex < n.data.size(); ++columnindex) {
		uint compressiontime(0), decompressiontime(0);
		double sizeinmb(0);
		vector<uint> incolumn;
		const uint MAXSIZE=10*1024*1024;// 50 million or about 50MB
		for(uint64 begin = 0; begin<n.data[columnindex].size(); begin+=MAXSIZE) {
			uint64 end = begin+MAXSIZE;
			if(end > n.data[columnindex].size())
				end = n.data[columnindex].size();
		    n.data[columnindex].loadACopy(incolumn,begin,end);
			ZTimer z;
			columntype out;
			for (uint k = 0; k < smallsetrepeats; ++k) {
				out.clear();
				mycodec.compress(incolumn,out);
			}
			compressiontime += z.split();
			sizeinmb += (out.size() * 1.0 / (1024.0 * 1024.0));
			z.reset();
			
			for (uint k = 0; k < smallsetrepeats; ++k) {
				columntype recovered;
				mycodec.uncompress(out, recovered);
			}

			decompressiontime += z.split();
		}
		r.add(sizeinmb, compressiontime, decompressiontime);
	}
	v.push_back(r);
}

template<class MyNaiveColumnStore>
void runtests(MyNaiveColumnStore & ncs, bool skiprepeats, bool justvolume = false) {
	if (ncs.size() == 0)
		return;
	cout << "# " << endl;
	vector<Results> vr;
	const uint uncompressedsize = ncs.size();
	uint smallsetrepeats = 1;
	if (!skiprepeats) {
		const uint decentsize = 10000000;
		const bool smallset = uncompressedsize < decentsize;
		smallsetrepeats = smallset ? 2 + 2 * decentsize / uncompressedsize : 1;
		if (smallset)
			cout << "# we have a small set, repeating all tests "
					<< smallsetrepeats << " times " << endl;
		else
			cout << "# we have a large data set, repeat all tests "
					<< smallsetrepeats << " times " << endl;
	} else {
	}
	initAlgos();
	for (vector<shared_ptr<SimpleCODEC> >::const_iterator pCODEC =
			myalgos.begin(); pCODEC != myalgos.end(); ++pCODEC) {
		testCodec(**pCODEC, ncs, vr, smallsetrepeats);
	}
	if(justvolume) {
		for (uint k = 0; k < vr.size(); ++k)
			cout << vr[k].justtotal()<<endl;
	} else {
		for (uint k = 0; k < vr.size(); ++k)
			cout << static_cast<string> (vr[k]) << endl;
	}
}

enum {
	SHUFFLE, LEXICO, VORTEX, GRAYCODED, MULTIPLELISTS, BLOCKWISEMULTIPLELISTS
};



template<int c>
void __displayStats(CSVFlatFile & ff) {
	ZTimer z;
	cout<<"#Loading into row store..."<<endl;
	RowStore<c> rs(ff,0);
	ff.close();
	cout << "# " << z.split() << " ms to load " << rs.size()
			<< " bytes into row store" << endl;
	cout<<"# fraction of tuples with zeroes = "<<  rs.countZeroes() * 1. / (rs.data.size() * c)<<endl;
	cout<<"# number of attribute values = "<< ff.numberOfAttributeValues()<<endl;
	cout<<"# excepted fraction = "<<c * 1.0 / ff.numberOfAttributeValues()<<endl;
}

template<int c>
void __readCSV(CSVFlatFile & ff, int sort, int columnorderheuristic,
		bool skiprepeats, const uint sample, const uint64 maxsize, const bool makeColumnIndependent) {
	ZTimer z;
	cout<<"#Loading into row store..."<<endl;
	//printMemoryUsage();
	RowStore<c> rs(ff,0);
	ff.close();
	cout << "# " << z.split() << " ms to load " << rs.size()
			<< " bytes into row store" << endl;
	if(sample>0) {
		    z.reset();
 		    RowStore<c> rstmp;
			rs.fillWithSample(sample,rstmp);
			rs.data.swap(rstmp.data);
			cout << "# " << z.split() << " ms to extract sample containing "<< sample<<" tuples" << endl;

	}
	cout << "# detected " << c << " columns" << endl;
	vector<uint> indexes = ff.computeColumnOrderAndReturnColumnIndexes(
			columnorderheuristic);
	cout<<"# clearing histogram memory..."<<endl;
	ff.clear();
	//cout<<"# fraction of tuples with zeroes = "<<  rs.countZeroes() * 1. / (rs.data.size() * c)<<endl;
	cout<<"# sorting..."<<endl;
	NaiveColumnStore<c> ncs;
	if(makeColumnIndependent) {
		cout<<"# shuffling columns independently"<<endl;
		cout<<"# shuffling columns independently (part 1: loading into column store)"<<endl;
		ncs.reloadFromRowStore(rs);
		cout<<"# shuffling columns independently (part 2: shuffling)"<<endl;
		ncs.makeColumnsIndependent();
		cout<<"# shuffling columns independently (part 3: copying back to row store)"<<endl;
		ncs.copyToRowStore(rs);
	}
	if (sort == LEXICO) {
		z.reset();
		rs.sortRows(indexes);
		cout << "# " << z.split() << " ms to sort rows" << endl;
		if(maxsize>0) rs.top(maxsize,rs);
		z.reset();
		ncs.reloadFromRowStore(rs);
		rs.clear();
		cout << "# " << z.split() << " ms to reload " << ncs.size()
				<< " bytes into column store" << endl;
	} else if (sort == MULTIPLELISTS) {
		cout << "not supported" << endl;
	} else if (sort == BLOCKWISEMULTIPLELISTS) {
		z.reset();
		rs.sortRows(indexes);
		cout << "# " << z.split() << " ms to sort rows lexicographically"
				<< endl;
		if(maxsize>0) rs.top(maxsize,rs);
		z.reset();
		rs.MultipleListsSortRowsPerBlock(indexes, 131072);//65536);
		cout << "# " << z.split() << " ms to sort rows in multiplelists order"
				<< endl;
		z.reset();
		ncs.reloadFromRowStore(rs);
		rs.clear();
		cout << "# " << z.split() << " ms to reload " << ncs.size()
				<< " bytes into column store" << endl;
	} else if (sort == VORTEX) {
		z.reset();
		rs.vortexSortRows(indexes);
		cout << "# " << z.split() << " ms to sort rows in vortex order" << endl;
		if(maxsize>0) rs.top(maxsize,rs);
		z.reset();
		ncs.reloadFromRowStore(rs);
		rs.clear();
		cout << "# " << z.split() << " ms to reload " << ncs.size()
				<< " bytes into column store" << endl;
	} else if (sort == GRAYCODED) {
		cerr << "not supported" << endl;
	} else {// shuffling
		z.reset();
		rs.shuffleRows();
		cout << "# " << z.split() << " ms to shuffle rows" << endl;
		if(maxsize>0) rs.top(maxsize,rs);
		z.reset();
		ncs.reloadFromRowStore(rs);
		rs.clear();
		cout << "# " << z.split() << " ms to reload " << ncs.size()
				<< " bytes into column store" << endl;
	}
	cout << "# got RunCount = " << ncs.computeRunCount() << endl;
	cout << "# got RunCount" << BLOCKSIZE << " = " << ncs.computeRunCountp(
			BLOCKSIZE) << endl;
	cout << "# block size = " << BLOCKSIZE << endl;
	runtests(ncs, skiprepeats);
	ncs.clear();
}



template<int c>
void __growCSV(CSVFlatFile & ff,  int columnorderheuristic) {
	ZTimer z;
	cout<<"#Loading into row store..."<<endl;
	//printMemoryUsage();
	RowStore<c> rs(ff,0);
	ff.close();
	cout << "# " << z.split() << " ms to load " << rs.size()
			<< " bytes into row store" << endl;
	cout << "# detected " << c << " columns" << endl;
	vector<uint> indexes = ff.computeColumnOrderAndReturnColumnIndexes(
			columnorderheuristic);
	cout<<"# clearing histogram memory..."<<endl;
	ff.clear();
	rs.sortRows(indexes);
	NaiveColumnStore<c> ncs;
	for(uint k = 131072*20; k<=rs.data.size();k+=131072*20) {
        //rstmp.sortRows(indexes);
		ncs.reloadFromRowStore(rs,k);
		cout<<"#=============================#"<<endl;
		cout<<"# number of rows = "<<k<<endl;
		cout<<"#=============================#"<<endl;
		cout<<"# lexico  "<<endl;
		runtests(ncs, true,  true);
		cout << "# got RunCount = " << ncs.computeRunCount() << endl;
		ncs.clear();
		cout<<"############################"<<endl;
		cout<<"# multiple list  "<<endl;
		RowStore<c> rstmp;
		rs.top(k,rstmp);
		rstmp.MultipleListsSortRowsPerBlock(indexes, 131072);
		ncs.reloadFromRowStore(rstmp);
		runtests(ncs, true,  true);
		rstmp.clear();
		cout << "# got RunCount = " << ncs.computeRunCount() << endl;
		ncs.clear();
		cout<<endl;
	}
	rs.clear();
	ncs.clear();
}




template<int c>
void __scaleCSV(CSVFlatFile & ff) {
	ZTimer z;
	cout<<"#Loading into row store..."<<endl;
	RowStore<c> rs(ff,0);
	ff.close();
	cout << "# " << z.split() << " ms to load " << rs.size()
			<< " bytes into row store" << endl;
	cout << "# detected " << c << " columns" << endl;
	vector<uint> indexes = ff.computeColumnOrderAndReturnColumnIndexes(
			INCREASINGCARDINALITY);
	cout<<"# clearing histogram memory..."<<endl;
	ff.clear();

	cout<<"# sorting..."<<endl;
	if (true) {
		z.reset();
		rs.sortRows(indexes);
		cout << "# " << z.split() << " ms to sort rows" << endl;
		z.reset();
		NaiveColumnStore<c> ncs;
		ncs.reloadFromRowStore(rs);
		//rs.clear();
		cout << "# " << z.split() << " ms to reload " << ncs.size()
				<< " bytes into column store" << endl;
		cout << "# got RunCount = " << ncs.computeRunCount() << endl;
		cout << "# got RunCount" << BLOCKSIZE << " = " << ncs.computeRunCountp(
				BLOCKSIZE) << endl;
		cout << "# block size = " << BLOCKSIZE << endl;
		runtests(ncs, true,true);
		cout<<endl;
	}
	uint numberofrows = rs.data.size();
	cout << "# detected " << numberofrows << " rows" << endl;
	if (true) {
		for (uint blocksize = 16; blocksize <= min(8388608,numberofrows); blocksize *= 2) {
			cout << "# blocksize " << blocksize << " rows" << endl;
			z.reset();
			rs.MultipleListsSortRowsPerBlock(indexes, blocksize);//65536);
			cout << "# " << z.split()
					<< " ms to sort rows in multiplelists order with blocksize =  "
					<< blocksize << endl;
			z.reset();
			NaiveColumnStore<c> ncs;
			ncs.reloadFromRowStore(rs);
			//rs.clear();
			cout << "# " << z.split() << " ms to reload " << ncs.size()
					<< " bytes into column store" << endl;
			cout << "# got RunCount = " << ncs.computeRunCount() << endl;
			cout << "# got RunCount" << BLOCKSIZE << " = "
					<< ncs.computeRunCountp(BLOCKSIZE) << endl;
			cout << "# block size = " << BLOCKSIZE << endl;
			runtests(ncs, true,true);
			cout << endl;
		}
	}
	if(false) {
		z.reset();
		rs.vortexSortRows(indexes);
		cout << "# " << z.split() << " ms to sort rows in vortex order" << endl;
		z.reset();
		NaiveColumnStore<c> ncs;
		ncs.reloadFromRowStore(rs);
		rs.clear();
		cout << "# " << z.split() << " ms to reload " << ncs.size()
			<< " bytes into column store" << endl;
		cout << "# got RunCount = " << ncs.computeRunCount() << endl;
		cout << "# got RunCount" << BLOCKSIZE << " = " << ncs.computeRunCountp(
				BLOCKSIZE) << endl;
		cout << "# block size = " << BLOCKSIZE << endl;
		runtests(ncs, true,true);
		cout<<endl;
	}
}
void readCSV(char * filename, int sort, const int normtype,
		int columnorderheuristic,
		bool skiprepeats, const uint sample, const uint64 maxsize, const bool makeColumnIndependent) {
	cout << "# loading CSV file \"" << filename << "\" from disk" << endl;
	CSVFlatFile ff(filename, normtype);
	cout<<"#file loaded"<<endl;
	const uint c = ff.getNumberOfColumns();
	// this is an ugly hack to get around the limitations
	// of STXXL which I ended up no using, so this was wasted hacking
	switch (c) {
	case 1:
		__readCSV<1> (ff, sort, columnorderheuristic, skiprepeats,sample,maxsize, makeColumnIndependent);
		break;
	case 2:
		__readCSV<2> (ff, sort, columnorderheuristic, skiprepeats,sample,maxsize, makeColumnIndependent);
		break;
	case 3:
		__readCSV<3> (ff, sort, columnorderheuristic, skiprepeats,sample,maxsize, makeColumnIndependent);
		break;
	case 4:
		__readCSV<4> (ff, sort, columnorderheuristic, skiprepeats,sample,maxsize, makeColumnIndependent);
		break;
	case 5:
		__readCSV<5> (ff, sort, columnorderheuristic, skiprepeats,sample,maxsize, makeColumnIndependent);
		break;
	case 6:
		__readCSV<6> (ff, sort, columnorderheuristic, skiprepeats,sample,maxsize, makeColumnIndependent);
		break;
	case 7:
		__readCSV<7> (ff, sort, columnorderheuristic, skiprepeats,sample,maxsize, makeColumnIndependent);
		break;
	case 8:
		__readCSV<8> (ff, sort, columnorderheuristic, skiprepeats,sample,maxsize, makeColumnIndependent);
		break;
	case 9:
		__readCSV<9> (ff, sort, columnorderheuristic, skiprepeats,sample,maxsize, makeColumnIndependent);
		break;
	case 10:
		__readCSV<10> (ff, sort, columnorderheuristic, skiprepeats,sample,maxsize, makeColumnIndependent);
		break;
	case 16:
		__readCSV<16> (ff, sort, columnorderheuristic, skiprepeats,sample,maxsize, makeColumnIndependent);
		break;
	case 19:
		__readCSV<19> (ff, sort, columnorderheuristic, skiprepeats,sample,maxsize, makeColumnIndependent);
		break;
	case 15:
		__readCSV<15> (ff, sort, columnorderheuristic, skiprepeats,sample,maxsize, makeColumnIndependent);
		break;
	case 17:
		__readCSV<17> (ff, sort, columnorderheuristic, skiprepeats,sample,maxsize, makeColumnIndependent);
		break;
	case 41:
		__readCSV<41> (ff, sort, columnorderheuristic, skiprepeats,sample,maxsize, makeColumnIndependent);
		break;
	case 42:
		__readCSV<42> (ff, sort, columnorderheuristic, skiprepeats,sample,maxsize, makeColumnIndependent);
		break;
	default:
		cerr << " # of columns " << c
				<< " not supported: yes, this is lame and a long story. Only some specific number of columns are supported due to a hack."
				<< endl;

	}
}


void displayStats(char * filename, const int normtype) {
	cout << "# loading CSV file \"" << filename << "\" from disk" << endl;
	CSVFlatFile ff(filename, normtype);
	cout<<"#file loaded"<<endl;
	const uint c = ff.getNumberOfColumns();
	// this is an ugly hack to get around the limitations
	// of STXXL which I ended up no using, so this was wasted hacking
	switch (c) {
	case 1:
		__displayStats<1> (ff);
		break;
	case 2:
		__displayStats<2> (ff);
		break;
	case 3:
		__displayStats<3> (ff);
		break;
	case 4:
		__displayStats<4> (ff);
		break;
	case 5:
		__displayStats<5> (ff);
		break;
	case 6:
		__displayStats<6> (ff);
		break;
	case 7:
		__displayStats<7> (ff);
		break;
	case 8:
		__displayStats<8> (ff);
		break;
	case 9:
		__displayStats<9> (ff);
		break;
	case 10:
		__displayStats<10> (ff);
		break;
	case 16:
		__displayStats<16> (ff);
		break;
	case 19:
		__displayStats<19> (ff);
		break;
	case 15:
		__displayStats<15> (ff);
		break;
	case 17:
		__displayStats<17> (ff);
		break;
	case 41:
		__displayStats<41> (ff);
		break;
	case 42:
		__displayStats<42> (ff);
		break;
	default:
		cerr << " # of columns " << c
				<< " not supported: yes, this is lame and a long story. Only some specific number of columns are supported due to a hack."
				<< endl;

	}
}



void growCSV(char * filename, const int normtype,int columnorderheuristic) {
	cout << "# loading CSV file \"" << filename << "\" from disk" << endl;
	CSVFlatFile ff(filename, normtype);
	cout<<"#file loaded"<<endl;
	const uint c = ff.getNumberOfColumns();
	// this is an ugly hack to get around the limitations
	// of STXXL which I ended up no using, so this was wasted hacking
	switch (c) {
	case 1:
		__growCSV<1> (ff,columnorderheuristic);
		break;
	case 2:
		__growCSV<2> (ff,columnorderheuristic);
		break;
	case 3:
		__growCSV<3> (ff,columnorderheuristic);
		break;
	case 4:
		__growCSV<4> (ff,columnorderheuristic);
		break;
	case 5:
		__growCSV<5> (ff,columnorderheuristic);
		break;
	case 6:
		__growCSV<6> (ff,columnorderheuristic);
		break;
	case 7:
		__growCSV<7> (ff,columnorderheuristic);
		break;
	case 8:
		__growCSV<8> (ff,columnorderheuristic);
		break;
	case 9:
		__growCSV<9> (ff,columnorderheuristic);
		break;
	case 10:
		__growCSV<10> (ff,columnorderheuristic);
		break;
	case 16:
		__growCSV<16> (ff,columnorderheuristic);
		break;
	case 19:
		__growCSV<19> (ff,columnorderheuristic);
		break;
	case 15:
		__growCSV<15> (ff,columnorderheuristic);
		break;
	case 17:
		__growCSV<17> (ff,columnorderheuristic);
		break;
	case 41:
		__growCSV<41> (ff,columnorderheuristic);
		break;
	case 42:
		__growCSV<42> (ff,columnorderheuristic);
		break;
	default:
		cerr << " # of columns " << c
				<< " not supported: yes, this is lame and a long story. Only some specific number of columns are supported due to a hack."
				<< endl;

	}
}

void scaleCSV(char * filename, const int normtype) {
	cout << "# loading CSV file \"" << filename << "\" from disk" << endl;
	CSVFlatFile ff(filename, normtype);
	//printMemoryUsage();
	const int c = ff.getNumberOfColumns();
	// this is an ugly hack to get around the limitations
	// of STXXL which I ended up no using, so this was wasted hacking
	switch (c) {
	case 1:
		__scaleCSV<1> (ff);
		break;
	case 2:
		__scaleCSV<2> (ff);
		break;
	case 3:
		__scaleCSV<3> (ff);
		break;
	case 4:
		__scaleCSV<4> (ff);
		break;
	case 5:
		__scaleCSV<5> (ff);
		break;
	case 6:
		__scaleCSV<6> (ff);
		break;
	case 7:
		__scaleCSV<7> (ff);
		break;
	case 8:
		__scaleCSV<8> (ff);
		break;
	case 9:
		__scaleCSV<9> (ff);
		break;
	case 10:
		__scaleCSV<10> (ff);
		break;
	case 15:
		__scaleCSV<15> (ff);
		break;
	case 16:
		__scaleCSV<16> (ff);
		break;
	case 19:
		__scaleCSV<19> (ff);
		break;
	case 17:
		__scaleCSV<17> (ff);
		break;
	case 41:
		__scaleCSV<41> (ff);
		break;
	case 42:
		__scaleCSV<42> (ff);
		break;
	default:
		cerr << " # of columns " << c
				<< " not supported: yes, this is lame and a long story. Only some specific number of columns are supported due to a hack."
				<< endl;

	}
}




int main(int argc, char **argv) {
	if (argc < 2) {
		cerr << " usage : tods2011 filename.csv " << endl;
		return -1;
	}
	uint maxsize = 0;
	uint sample = 0; // by default, don't sample
	int normtype = CSVFlatFile::FREQNORMALISATION;
	cout << "# normalizing by frequency" << endl;
	char * filename = argv[1];
	bool makeColumnIndependent(false);
	if(argc>2) {
		char * parameter = argv[1];
		filename = argv[2];
		if(   strcmp(parameter,"-scale")==0   ) {
			 scaleCSV(filename, normtype);
			 return 0;
		} else if( strcmp(parameter,"-makeindependent")==0 ) {
			makeColumnIndependent = true;
		} else	if(   strcmp(parameter,"-stats")==0   ) {
			displayStats(filename, normtype);
			return 0;
		} else	if(   strcmp(parameter,"-grow")==0   ) {
			growCSV(filename, normtype,INCREASINGCARDINALITY);
			return 0;
		} else	if(   strcmp(parameter,"-top")==0   ) {
			cout << "#top "  << endl;
			maxsize = 131072;
		} else	if(   strcmp(parameter,"-sample")==0   ) {
			cout << "#sampling "  << endl;
			sample = 65536;
		} else	if(   strcmp(parameter,"-shuffling")==0   ) {
			cout << "#shuffling " << filename << endl;
			readCSV(filename, SHUFFLE, normtype, INCREASINGCARDINALITY, false, sample,maxsize,makeColumnIndependent);
			return 0;
		} else if(strcmp(parameter,"-lexup")==0) {
			cout << "#sort--increasing column cardinality " << filename << endl;
			readCSV(filename, LEXICO, normtype, INCREASINGCARDINALITY, false, sample,maxsize,makeColumnIndependent);
			return 0;
		} else if(strcmp(parameter,"-lexdown")==0) {
			cout << "#sort--decreasing column cardinality " << filename << endl;
			readCSV(filename, LEXICO, normtype, DECREASINGCARDINALITY, false, sample,maxsize,makeColumnIndependent);
			return 0;
		} else if(strcmp(parameter,"-vortexup")==0) {
			cout << "#sort--vortex increasing column cardinality " << filename << endl;
			readCSV(filename, VORTEX, normtype, INCREASINGCARDINALITY, false, sample,maxsize,makeColumnIndependent);
			return 0;
		} else if(strcmp(parameter,"-vortexdown")==0) {
			cout << "#sort--vortex decreasing column cardinality " << filename << endl;
			readCSV(filename, VORTEX, normtype, DECREASINGCARDINALITY, false, sample,maxsize,makeColumnIndependent);
			return 0;
		} else if(strcmp(parameter,"-multiup")==0) {
			cout << "#sort--blockwisemultiplelists increasing column cardinality "
					<< filename << endl;
			readCSV(filename, BLOCKWISEMULTIPLELISTS, normtype, INCREASINGCARDINALITY,
					false, sample,maxsize,makeColumnIndependent);
			return 0;
		} else if(strcmp(parameter,"-multidown")==0) {
			cout << "#sort--blockwisemultiplelists decreasing column cardinality "
					<< filename << endl;
			readCSV(filename, BLOCKWISEMULTIPLELISTS, normtype, DECREASINGCARDINALITY,
					false, sample,maxsize,makeColumnIndependent);
			return 0;
		} else {
			cout<<" unknown parameter"<<parameter<<endl;
			return -1;
		}
	}
	cout << "#shuffling " << filename << endl;
	readCSV(filename, SHUFFLE, normtype, INCREASINGCARDINALITY, false,sample,maxsize,makeColumnIndependent);
	cout << endl;
	cout << "#sort--increasing column cardinality " << filename << endl;
	readCSV(filename, LEXICO, normtype, INCREASINGCARDINALITY, false, sample,maxsize,makeColumnIndependent);
	cout << endl;
	cout << "#sort--decreasing column cardinality " << filename << endl;
	readCSV(filename, LEXICO, normtype, DECREASINGCARDINALITY, false, sample,maxsize,makeColumnIndependent);
	cout << endl;
	cout << "#sort--vortex increasing column cardinality " << filename << endl;
	readCSV(filename, VORTEX, normtype, INCREASINGCARDINALITY, false, sample,maxsize,makeColumnIndependent);
	cout << endl;
	cout << "#sort--vortex decreasing column cardinality " << filename << endl;
	readCSV(filename, VORTEX, normtype, DECREASINGCARDINALITY, false, sample,maxsize,makeColumnIndependent);
	cout << endl;
	cout << "#sort--blockwisemultiplelists increasing column cardinality "
			<< filename << endl;
	readCSV(filename, BLOCKWISEMULTIPLELISTS, normtype, INCREASINGCARDINALITY,
			false, sample,maxsize,makeColumnIndependent);
	cout << endl;
	cout << "#sort--blockwisemultiplelists decreasing column cardinality "
			<< filename << endl;
	readCSV(filename, BLOCKWISEMULTIPLELISTS, normtype, DECREASINGCARDINALITY,
			false, sample,maxsize,makeColumnIndependent);
	cout << endl;
	return 0;
}

