/**
 * (c) Daniel Lemire 2010-2012
 * Apache License 2.0
 */

#ifndef FLATFILE
#define FLATFILE

// this provides read support for a very simple flat file format

#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include "util.h"

using namespace std;



enum {INCREASINGCARDINALITY, DECREASINGCARDINALITY};
/**
* Comma-Separate Values
*/
class CSVReader{
	public:



	CSVReader(istream * in, const string delimiter = ",",
			const char commentmarker = '#') :
		line(), mDelimiter(delimiter), mDelimiterPlusSpace(delimiter),
				mCommentMarker(commentmarker), mIn(in), currentData() {
	}
	CSVReader(const CSVReader & o) :
		line(o.line), mDelimiter(o.mDelimiter), mDelimiterPlusSpace(o.mDelimiterPlusSpace),
				mCommentMarker(o.mCommentMarker), mIn(o.mIn), currentData(o.currentData) {
	}
	CSVReader & operator=(const CSVReader & o) {
		line = o.line;
		mDelimiter = o.mDelimiter;
		mDelimiterPlusSpace = o.mDelimiterPlusSpace;
		mCommentMarker = o.mCommentMarker;
		mIn = o.mIn;
		currentData = o.currentData;
		return *this;
	}

	virtual ~CSVReader() {
	}

	void linkStream(istream * in) {
		mIn = in;
	}

	inline bool hasNext() {
		while (getline(*mIn, line)) {
			if (line.size() == 0)
				continue;
			if (line.at(0) == mCommentMarker) continue;
				tokenize(line);
			    return true;
			}
			return false;
		}

		inline const vector<string> & nextRow()  const {
			return currentData;
		}

		string line;

	private:

		inline void tokenize(const string& str){
		    uint counter(0);
			string::size_type lastPos = str.find_first_not_of(mDelimiterPlusSpace, 0);
			string::size_type pos     = str.find_first_of(mDelimiter, lastPos);
			string::size_type pos_w = str.find_last_not_of(' ',pos);
			while (string::npos != pos || string::npos != lastPos){
				const string::size_type fieldlength = pos == string::npos ?   pos_w + 1 - lastPos: pos_w -lastPos;
		        if(currentData.size() < ++counter) currentData.resize(counter);
		        currentData[counter-1].resize(fieldlength);
		        currentData[counter-1].assign(str,lastPos, fieldlength);
		    	lastPos = str.find_first_not_of(mDelimiterPlusSpace, pos);
		    	pos = str.find_first_of(mDelimiter, lastPos);
		    	pos_w = str.find_last_not_of(' ',pos);
			}
		    currentData.resize(counter);
		}

		string mDelimiter;
		string mDelimiterPlusSpace;
		char mCommentMarker;
		istream * mIn;
		vector<string> currentData;

};



class CSVFlatFile {
	public:

	typedef map<string,int>  maptype;
	typedef map<string,uint>  umaptype;

	enum{FREQNORMALISATION,DOMAINNORMALISATION};
	CSVFlatFile(const char * filename, const int normtype) : in(), mainreader(NULL),mapping(), NumberOfLines(0) {
		cout<<"# computing normalization of file "<<filename<<endl;
		if(normtype==FREQNORMALISATION)
			computeFreqNormalization(filename);
		else if(normtype==DOMAINNORMALISATION)
			computeDomainNormalization(filename);
		in.open(filename);
		mainreader.linkStream(&in);
	}


	uint getNumberOfRows() const {return NumberOfLines;}
	uint getNumberOfColumns() const {return mapping.size();}

    uint64 numberOfAttributeValues() {
    	uint64 sum = 0;
    	for(uint k = 0; k<mapping.size();++k) {
    		sum+=mapping[k].size();
    	}
    	return sum;
    }

    vector<uint> computeColumnOrderAndReturnColumnIndexes(int order = INCREASINGCARDINALITY) {
    	vector<uint> cardinalities;
    	for(uint k = 0; k<mapping.size();++k) {
			cardinalities.push_back(mapping[k].size());
		}
        vector<pair<uint,uint> > cardinalitiesindex;
        for(uint k = 0; k<cardinalities.size() ; ++k)
            cardinalitiesindex.push_back(pair<uint,uint>(cardinalities.at(k),k));
        if(order == INCREASINGCARDINALITY)
            sort(cardinalitiesindex.begin(),cardinalitiesindex.end());
        else
            sort(cardinalitiesindex.rbegin(),cardinalitiesindex.rend());
        vector<uint> answer(getNumberOfColumns());
        for(uint k = 0; k< answer.size(); ++k)
        	answer[k] = cardinalitiesindex[k].second;
        return answer;
    }



	// a bit awkward... but convenient:
	set<uint> AllButTopCardinalityColumnsExceptThoseWithCardinalityLowerThan(uint topcolumns, uint threshold) {
		if(mapping.size() == 0) {
			set<uint> answer;
			return answer;
		}
		vector<uint> allcards;
		for(uint k = 0; k<mapping.size();++k) {
			allcards.push_back(mapping[k].size());
		}
		sort(allcards.rbegin(),allcards.rend());
		uint mythreshold = allcards[min(topcolumns-1,allcards.size()-1)];
		cout<<"# top "<<topcolumns<<" columns selected with cardinality "<<mythreshold<<endl;
		if(mythreshold < threshold) mythreshold = threshold;
		cout<<"# cardinality threshold "<<mythreshold<<endl;
		set<uint> answer;
		for(uint k = 0; k<mapping.size();++k) {
			if(mapping[k].size()<mythreshold) {
				answer.insert(k);
			}
		}
		return answer;
	}
	void clear() {
		mapping.clear();
	}
	void close() {
		in.close();
	}

	//uint getCardinalityOfColumn(uint k) {return mapping[k].size();}
	template<class C>
	bool nextRow(C & container) {
		if(mainreader.hasNext()) {
			const vector<string> & row = mainreader.nextRow();
			for(uint k = 0; k<row.size(); ++k) {
				container[k] = mapping[k][row[k]];
			}
			return true;
		} else return false;
	}

	void computeHisto(ifstream  & fsin,vector<umaptype > & histograms) {
		  NumberOfLines = 0;
		  CSVReader csvfile(&fsin);
		  if(csvfile.hasNext()) {
			  ++NumberOfLines;
			  const vector<string> & row = csvfile.nextRow();
			  histograms.resize(row.size());
			  for(uint k = 0; k<row.size(); ++k) {
				  histograms[k][row[k]]=1;
			  }
		  } else {
			  cerr<<"could open the file, but couldn't even read the first line"<<endl;
			  return;
		  }
		  while(csvfile.hasNext()) {
			  ++NumberOfLines;
			  const vector<string> & row = csvfile.nextRow();
			  for(uint k = 0; k<row.size(); ++k) {
				  histograms[k][row[k]]+=1;
			  }
		  }
	}
	// map the string values to integer per frequency
	void computeFreqNormalization(const char * filename) {
		  ifstream fsin(filename);
		  if(!fsin) {
			  cerr<<"can't open "<<filename<<endl;
			  return;
		  }

		  vector<umaptype > histograms;
		  computeHisto(fsin,histograms);

		  fsin.close();
		  mapping.resize(histograms.size());
		  // next we sort the values per frequency
		  for(uint k = 0; k<histograms.size(); ++k) {
			  vector<pair<uint,string> > tobesorted;
			  tobesorted.reserve(histograms[k].size());
			  for(umaptype::iterator i = histograms[k].begin(); i!= histograms[k].end();++i) {
				  tobesorted.push_back(pair<uint,string>(i->second,i->first));
			  }
			  histograms[k].clear();// free the memory
			  sort(tobesorted.rbegin(),tobesorted.rend());
			  maptype & thismap = mapping[k];
			  for(uint k = 0; k<tobesorted.size(); ++k)
				  thismap[tobesorted[k].second] = k;
		  }
	  }


	  // map the string values to integers in lexicographical order
 	  void computeDomainNormalization(const char * filename) {
		  ifstream fsin(filename);
		  if(!fsin) {
			  cerr<<"can't open "<<filename<<endl;
			  return;
		  }
		  NumberOfLines = 0;
		  CSVReader csvfile(&fsin);
		  vector<set<string> > histograms;
		  if(csvfile.hasNext()) {
			  ++ NumberOfLines;
			  const vector<string> & row = csvfile.nextRow();
			  histograms.resize(row.size());
			  for(uint k = 0; k<row.size(); ++k) {
				  histograms[k].insert(row[k]);
			  }
		  } else {
			  cerr<<"could open the file, but couldn't even read the first line of "<<filename<<endl;
			  return;
		  }
		  while(csvfile.hasNext()) {
			  ++NumberOfLines;
			  const vector<string> & row = csvfile.nextRow();
			  for(uint k = 0; k<row.size(); ++k) {
				  histograms[k].insert(row[k]);
			  }
		  }
		  fsin.close();
		  mapping.resize(histograms.size());
		  // the values are already sorted lexicographically
		  for(uint k = 0; k<histograms.size(); ++k) {
			  set<string> &  myvalues = histograms[k];
			  maptype & thismap = mapping[k];
			  uint counter = 0;
			  for(set<string>::iterator i = myvalues.begin(); i!= myvalues.end(); ++i) {
				  thismap[*i] = counter++;
			  }
			  myvalues.clear();
		  }
	  }

	  ifstream in;
	  CSVReader mainreader;
	  vector<maptype > mapping;
	  uint NumberOfLines;
};


// if you don't know what this is, you probably don't need it.
class LegacyBinaryFlatFile {
	public: 
	LegacyBinaryFlatFile(char * filename) : in(filename), version(), cookie(), column() {
	  	    in.read(reinterpret_cast<char *> (& cookie), sizeof (cookie));
	  	    endian_swap(cookie);
	  	    if(cookie != MAGIC) {
	  	    	cerr<<"========ERROR========"<<endl;
	  	    	cerr<<"Bad cookie. Is this a proper binary flat file? cookie: "<<cookie<<endl;
	  	    	cerr<<"Please check file "<<filename<<endl;
	  	    	cerr<<"It needs to be a custom made binary flat file."<<endl;
	  	    	cerr<<"========ERROR========"<<endl;
	  	    	return;
	  	    }
	  	    in.read(reinterpret_cast<char *> (& version), sizeof (version));
	  	    endian_swap(version);
	  	    if(version != VERSION) {
	  	    	cerr<<"========ERROR========"<<endl;
	  	    	cerr<<"version: "<<version<<endl;
	  	    	cerr<<" I was expecting "<<static_cast<int>(VERSION)<<endl;
	  	    	cerr<<"Please check file "<<filename<<endl;
	  	    	cerr<<"========ERROR========"<<endl;
	  	    }
	  	    in.read(reinterpret_cast<char *> (& column), sizeof (column));
	  	    endian_swap(column);
	  	    cout<<"# found "<<column<<" columns" <<endl;
	  }
	  
	  uint getNumberOfColumns() const {return column;}
	  
	  bool nextRow(vector<int> & container) {
	  	in.read(reinterpret_cast<char *> (& container[0]), sizeof (int) * column);
	  	for(int k = 0; k<column;++k) endian_swap(container[k]);
	  	return in;
	  }
	  
	  enum{MAGIC=0x76,VERSION=1};
	  
	  ifstream in;
	  int version, cookie, column;
};


#endif
