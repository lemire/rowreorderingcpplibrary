/**
 * (c) Daniel Lemire 2010-2012
 * Apache License 2.0
 */

#ifndef EXTERNALVECTOR_H_
#define EXTERNALVECTOR_H_

#include <sys/mman.h>
#include <queue>
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <cassert>
#include <errno.h>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
using namespace std;

typedef unsigned int uint;
typedef unsigned long long uint64;

template<class DataType, class CMP>
class BinaryFileBuffer {
public:
	enum {
		BUFFERSIZE = 512
	};
	FILE *fd;
	vector<DataType>* buf;
	uint64 currentpointer, mEnd;
	uint localpointer;
	bool valid;
	CMP mCmp;

	BinaryFileBuffer(const BinaryFileBuffer & o) :
		fd(o.fd), buf(o.buf), currentpointer(o.currentpointer), mEnd(o.mEnd),
				localpointer(o.localpointer), valid(o.valid), mCmp(o.mCmp) {
	}
	BinaryFileBuffer() :
		fd(NULL), buf(NULL), currentpointer(0), mEnd(0), localpointer(0),
				valid(false), mCmp(CMP()) {
	}

	BinaryFileBuffer<DataType, CMP> & operator=(
			const BinaryFileBuffer<DataType, CMP> &o) {
		fd = o.fd;
		buf = o.buf;
		currentpointer = o.currentpointer;
		mEnd = o.mEnd;
		localpointer = o.localpointer;
		valid = o.valid;
		mCmp = o.mCmp;
		return *this;
	}
	BinaryFileBuffer(FILE * f, uint64 start, uint64 end, CMP cmp,
			vector<DataType>& Buf) :
		fd(f), buf(&Buf), currentpointer(start), mEnd(end), localpointer(0),
				valid(true), mCmp(cmp) {
		buf->reserve(BUFFERSIZE);
		reload();
	}

	void reset(uint64 start) {
		currentpointer = start;
		localpointer = 0;
		valid = true;
		reload();
	}

	uint64 size() {
		return mEnd - currentpointer;
	}

	void reload() {
		assert(currentpointer<=mEnd);
		uint howmanycanIread = BUFFERSIZE;
		if (howmanycanIread > mEnd - currentpointer)
			howmanycanIread = mEnd - currentpointer;
		if (howmanycanIread == 0) {
			buf->resize(0);
			valid = false;
			return;
		}
		buf->resize(howmanycanIread);

		int result = fseek(fd, currentpointer * sizeof(DataType), SEEK_SET);
		if (result != 0) {
			cerr << "failed to seek" << endl;
			throw runtime_error("failed to seek");
		}
		size_t howmany = fread(&((*buf)[0]), sizeof(DataType), howmanycanIread,
				fd);
		if (howmany != howmanycanIread) {
			cerr << " Expected to read " << howmanycanIread << " but found "
					<< howmany << endl;
			throw runtime_error("failed to read the number expected");
		}

		localpointer = 0;
	}



	bool hasMore() {
		return valid;
	}
	bool empty() {
		return !valid;
	}

	const DataType & peek() const {
		return (*buf)[localpointer];
	}

	bool operator<(const BinaryFileBuffer<DataType, CMP> & bfb) const {
		return mCmp(this->peek(), bfb.peek());
	}

	bool operator>(const BinaryFileBuffer<DataType, CMP> & bfb) const {
		return mCmp(bfb.peek(), this->peek());
	}
	void pop(DataType & container) {
		++currentpointer;
		container = (*buf)[localpointer++];
		assert(localpointer<=buf->size());
		if (localpointer == buf->size())
			reload();
	}
private:

};

template<class DataType>
class externalvector {

public:

	enum {
		verbose = false, vverbose = false
	};

	typedef DataType value_type;
	typedef DataType* iterator;
	typedef const DataType* const_iterator;
	typedef DataType& reference;
	typedef const DataType& const_reference;
	externalvector() :
		fd(NULL),N(0), mFileName(NULL) {
	}
	~externalvector() {
	}
	externalvector<DataType> top(uint64 number, const uint64 BLOCKSIZE =DEFAULTBLOCKSIZE) const {
		if(size()<number) number = size();
		externalvector<DataType> ans;
		ans.open();
		vector<DataType> buffer;
		for (uint64 k = 0; k < number; k += BLOCKSIZE) {
			if (k + BLOCKSIZE < number) {
				loadACopy(buffer,k,k+BLOCKSIZE);
				ans.append(buffer);
			} else {
				loadACopy(buffer,k,number);
				ans.append(buffer);
			}
		}
		return ans;

	}

	externalvector<DataType> buildSample(const uint number) {
		externalvector<DataType> ans;
		ans.open();

		int result =fseek(fd, 0L, SEEK_END);
		if(result != 0) {
			cerr<<"could not determine file size"<<endl;
		}
		//cout<< "file size= "<<ftell(fd)<< endl;
		DataType buffer;
		while(ans.size()<number) {
			const uint64 pos =rand()%N;
			//cout<<"reading at pos "<<pos<<endl;
			//int result =
			fseek(fd, pos * sizeof(DataType), SEEK_SET);
			//if (result != 0) {
			//			cerr << "could not seek to " << pos << endl;
			//			throw runtime_error("bad seek");
			//}
			//size_t howmany =
			fread(&buffer, sizeof(DataType), 1,fd);
			//if (howmany !=  1) {
			//			cerr << "Error reading from file " << endl;
			//			cerr << strerror(errno) << endl;
			//			throw runtime_error("bad read");
			//}
			ans.append(buffer);
		}
		return ans;

	}

	externalvector(const externalvector<DataType> & other) :
		fd(NULL),  N(0),  mFileName(NULL) {
		if ((other.fd != NULL) or (fd != NULL)) {
			cerr << "please don't use copy constructor for non-trivial things"
					<< endl;
			throw runtime_error("you are abusing copy constructor");
		}
		assert(other.fd==NULL);
		assert(other.N==0);
	}
	externalvector<DataType> & operator=(const externalvector<DataType> & other) {
		if ((other.fd != NULL) or (fd != NULL)) {
			cerr << "please don't use assignment for non-trivial things"
					<< endl;
			throw runtime_error("you are abusing assignment operator");
		}
		assert(other.fd==NULL);
		assert(other.N==0);
		fd = NULL;
		N = 0;
		mFileName = NULL;
		return *this;
	}

	void swap(externalvector<DataType> & o) {
		FILE * tmpfd = fd;
		uint64 tmpN = N;
		char * tmpFileName = mFileName;
		//
		fd = o.fd;
		N = o.N;
		mFileName = o.mFileName;
		//
		o.fd = tmpfd;
		o.N = tmpN;
		o.mFileName = tmpFileName;
	}
	off_t getFileSize(char * filename) {
		struct stat s;
		lstat(filename, &s);
		return s.st_size;
	}

	off_t getFileSize() {
		struct stat s;
		lstat(mFileName, &s);
		return s.st_size;
	}

	enum{DEFAULTBLOCKSIZE=4194304};

	// this is not a true shuffle, it shuffles by block,
	// blocks are *not* merged. For my purposes, I did not
	// need a true shuffle.
	void shuffle(const uint64 BLOCKSIZE =DEFAULTBLOCKSIZE) {
		vector<DataType> buffer;
		for (uint64 k = 0; k < size(); k += BLOCKSIZE) {
			if (k + BLOCKSIZE < size()) {
				loadACopy(buffer,k,k+BLOCKSIZE);
				random_shuffle(buffer.begin(), buffer.end());
				copyAt(buffer,k);
			} else {
				loadACopy(buffer,k,size());
				random_shuffle(buffer.begin(), buffer.end());
				copyAt(buffer,k);
			}
			cout << "#completed block " << (k) / BLOCKSIZE << " out of "
					<< (size() / BLOCKSIZE
							+ (size() % BLOCKSIZE == 0 ? 0 : 1))
					<< " blocks" << endl;
		}

	}





	template<class CMP>
	void sort(CMP & comparator,const uint64 BLOCKSIZE =DEFAULTBLOCKSIZE) {
		 //;//1024*10;
		// first you sort blocks
		//
		// we use a "reverse" priority queue which
		// returns the "lesser" item each time.
		//
		priority_queue<BinaryFileBuffer<DataType, CMP> , vector<
				BinaryFileBuffer<DataType, CMP> > , greater<BinaryFileBuffer<
				DataType, CMP> > > pq;
		/////priority_queue<BinaryFileBuffer<DataType,CMP> > pq;

		cout << "# sorting by blocks " << endl;
		const uint howmanybuffers = N / BLOCKSIZE
				+ (N % BLOCKSIZE == 0 ? 0 : 1);

		vector<vector<DataType> > buffers(howmanybuffers);// + one if for luck
		uint buffercounter = 0;


		vector<DataType> buffer;
		buffer.reserve(BLOCKSIZE);
		for (uint64 rowindex = 0; rowindex < size(); rowindex += BLOCKSIZE) {

			cout << "# block " << (rowindex / BLOCKSIZE + 1) << " out of "
					<< (N / BLOCKSIZE + (N % BLOCKSIZE == 0 ? 0 : 1)) << endl;
			uint64 end = size();
			if (rowindex + BLOCKSIZE < size())
				end = rowindex + BLOCKSIZE;
			loadACopy(buffer,rowindex,end);
			std::sort(buffer.begin(), buffer.end(),comparator);
			copyAt(buffer, rowindex);
			BinaryFileBuffer<DataType, CMP> bfb(fd, rowindex, end, comparator,
					buffers[buffercounter++]);

			pq.push(bfb);
		}
		if(buffercounter!=buffers.size())
			throw runtime_error("This should never happen.");
		if(howmanybuffers==1)
			return;// we are done
		// we must merge which requires a new file
		char * newFileName;
		if (getenv("TMPDIR") != NULL) {
			newFileName = tempnam(getenv("TMPDIR"), NULL);
			if (verbose)
				cout << "# creating temp file " << newFileName << endl;
		} else {
			newFileName = tmpnam(NULL);
			if (verbose)
				cout << "# TMPDIR is null, creating temp file " << newFileName
						<< endl;
		}
		FILE * newfd = ::fopen(newFileName, "w+b");
		if (newfd == NULL) {
			cerr << "Can't open " << newFileName << endl;
			cerr << "Check your TMPDIR variable" << endl;
			cerr << strerror(errno) << endl;
			throw runtime_error("could not open temp file");
		}

		DataType container;
		const bool bfbparanoid = false;
		DataType lastentry;
		bool first = true;

		cout << "#sorted all the blocks" << endl;
		uint64 counter = 0;
		BinaryFileBuffer<DataType, CMP> bfb;
		while (!pq.empty()) {
			bfb = pq.top();
			pq.pop();

			bfb.pop(container);
			if (bfbparanoid) {
				if (!first) {
					if (comparator(container, lastentry))
						cout << " we have a problem at counter = " << counter
								<< endl;
					assert(!comparator(container, lastentry));
					lastentry = container;
				} else {
					first = false;
					lastentry = container;
				}
			}

			size_t result = fwrite(&container, sizeof(container), 1, newfd);
			if (result != 1) {
				cerr << "Error appending to the file " << newFileName << endl;
				cerr << strerror(errno) << endl;
				break;
			}
			++counter;
			if (!bfb.empty()) {
				pq.push(bfb); // add it back
			}
		}
		::fclose(fd);
		fd = newfd;
		mFileName = newFileName;
	}

	bool append(const DataType & d) {
		int results = fseek(fd, 0, SEEK_END);
		if (results != 0) {
			cerr << "could not seek to end of file" << endl;
			throw runtime_error("bad seek");
		}
		size_t result = fwrite(&d, sizeof(d), 1, fd);
		if (result != 1) {
			cerr << "Error appending to the file " << mFileName << endl;
			cerr << strerror(errno) << endl;
			return false;
		}
		++N;
		return true;
	}

	// caller is responsible for calling close
	void open() {
		if (vverbose)
			cout << "opening..." << endl;
		externalvector<DataType>::NumberOfCallsToOpen += 1;
		if (externalvector<DataType>::NumberOfCallsToOpen > 100)
			cout << "# lots of temporary files : "
					<< externalvector<DataType>::NumberOfCallsToOpen << endl;


		if (getenv("TMPDIR") != NULL) {
			mFileName = tempnam(getenv("TMPDIR"), NULL);
			if (verbose)
				cout << "# creating temp file " << mFileName << endl;
		} else {
			mFileName = tmpnam(NULL);
			if (verbose)
				cout << "# TMPDIR not specified, creating temp file "
						<< mFileName << endl;
		}
		fd = ::fopen(mFileName, "w+b");
		if (fd == NULL) {
			cerr << "Can't open " << mFileName << endl;
			cerr << "Check your TMPDIR variable" << endl;
			cerr << strerror(errno) << endl;
			throw runtime_error("could not open temp file");
		}
		if (vverbose)
			cout << "File " << mFileName << " is opened" << endl;
	}

	void close() {

		if (fd != NULL) {
			if (vverbose)
				cout << "closing " << mFileName << endl;
			::fclose(fd);
			::unlink(mFileName);
			fd = NULL;
			N = 0;
			assert(externalvector<DataType>::NumberOfCallsToOpen>0);
			externalvector<DataType>::NumberOfCallsToOpen -= 1;
			mFileName = NULL;
		}
	}

	void loadACopy(vector<DataType> & buffer, uint64 begin, uint64 end) const {
		buffer.resize(end - begin);
		int result = fseek(fd, begin * sizeof(DataType), SEEK_SET);
		if (result != 0) {
			cerr << "could not seek to " << begin << endl;
			throw runtime_error("bad seek");
		}
		size_t result2 = fread(&(buffer[0]), sizeof(DataType), buffer.size(), fd);
		if (result2 != (end-begin)) {
			cerr << "Error reading from file " << endl;
			cerr << strerror(errno) << endl;
			throw runtime_error("bad read");
		}

	}

	DataType get(const uint64 pos) {

		    if (fd == NULL) {
		    	cerr<<"no file to read from! Open the file first!"<<endl;
				throw runtime_error("file not open");
		    }
			DataType ans;
			int result =
			fseek(fd, pos * sizeof(DataType), SEEK_SET);
			if (result != 0) {
						cerr << "could not seek to " << pos << endl;
						throw runtime_error("bad seek");
			}
			size_t howmany =
			fread(&ans, sizeof(DataType), 1,fd);
			if (howmany !=  sizeof(DataType)) {
						cerr << "Error read from file " << endl;
						cerr << strerror(errno) << endl;
						throw runtime_error("bad read");
			}
			return ans;
	}
	void append(const vector<DataType> & buffer) {
		int result = fseek(fd, 0, SEEK_END);
		if (result != 0) {
			cerr << "could not seek to end of file" << endl;
			throw runtime_error("bad seek");
		}
		size_t result2 = fwrite(&(buffer[0]), sizeof(DataType), buffer.size(), fd);
		if (result2 != buffer.size()) {
			cerr << "Error write from file " << endl;
			cerr << strerror(errno) << endl;
			throw runtime_error("bad read");
		}
		N+=buffer.size();

	}

	void copyAt(const vector<DataType> & buffer, uint64 begin) {
		int result = fseek(fd, begin * sizeof(DataType), SEEK_SET);
		if (result != 0) {
			cerr << "could not seek to " << begin << endl;
			throw runtime_error("bad seek");
		}
		size_t result2 = fwrite(&(buffer[0]), sizeof(DataType), buffer.size(), fd);
		if (result2 != buffer.size()) {
			cerr << "Error write from file " << endl;
			cerr << strerror(errno) << endl;
			throw runtime_error("bad read");
		}

	}

	uint64 size() const {
		return N;
	}

private:

	FILE * fd; //file descriptor
	uint64 N;
	static uint NumberOfCallsToOpen;
	char * mFileName;
};

template<class DataType>
uint externalvector<DataType>::NumberOfCallsToOpen = 0;


#endif /* EXTERNALVECTOR_H_ */
