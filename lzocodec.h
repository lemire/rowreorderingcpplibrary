/**
 * (c) Daniel Lemire 2010-2012
 * Apache License 2.0
 */


#ifndef LZOCODEC
#define LZOCODEC

#include "columncodecs.h"
#include "columnwidecodecs.h"

/// you need to install the LZO library. Should be painless: http://www.oberhumer.com/opensource/lzo/
#include "lzo/lzoconf.h"
#include "lzo/lzo1x.h"


class lzo : public SimpleCODEC {
	public:
		lzo() : wrkmem(LZO1X_1_MEM_COMPRESS){
			if (lzo_init() != LZO_E_OK) cerr<<"The LZO library is not ok. Something will fail."<<endl;
		}
		
		string name() const {
		        ostringstream convert;
		        convert <<"lzo(v."<<LZO_VERSION_STRING<<")";
		        return convert.str();
		}

		void compress(const columntype & in, columntype& out) {
			out.resize(in.size()+in.size()/16+ 64 + 3 + 1);// allocating enough space
			lzo_uint in_len = in.size() * sizeof(uint);
		    lzo_uint out_len (0);
			out[0] = in.size();
			int r = lzo1x_1_compress(reinterpret_cast<const unsigned char*>(&in[0]),in_len,reinterpret_cast<unsigned char*>(&out[2]),&out_len,&wrkmem[0]);
			if(r != LZO_E_OK) cerr<<"Something went wrong during the compression."<<endl;
			out[1] = out_len;
			out.resize(2+ceildiv(out_len,sizeof(uint)));
		}

		void uncompress(const columntype & in, columntype & out){
			const uint size = in[0];
			lzo_uint in_len = in[1];
			out.resize(size);
		    lzo_uint out_len(0);
			int r = lzo1x_decompress(reinterpret_cast<const unsigned char*>(&in[2]),in_len,reinterpret_cast<unsigned char*>(&out[0]),&out_len,NULL);
			if(r != LZO_E_OK) cerr<<"Something went wrong during the decompression."<<endl;
		}
		
		vector<unsigned char> wrkmem;
};

template<uint BLOCKSIZE>
class BlockedLZO : public CODEC {
	public:
		BlockedLZO() : wrkmem(LZO1X_1_MEM_COMPRESS),bytesforcounter(4),blockbuffer(BLOCKSIZE){
			if (lzo_init() != LZO_E_OK) cerr<<"The LZO library is not ok. Something will fail."<<endl;
			if(BLOCKSIZE<=(1U<<8))
				bytesforcounter = 1;
			else if(BLOCKSIZE<=(1U<<16))
				bytesforcounter = 2;
		}

		string name() const {
		        ostringstream convert;
		        convert <<"BlockedLZO<"<<BLOCKSIZE<<">(v."<<LZO_VERSION_STRING<<")";
		        return convert.str();
		}

	    uint blocksize() const {return BLOCKSIZE;}

		void compress(const columntype & in, columntype& out,columntype & blockpointers) {
			out.resize(in.size()+in.size()/16+ 64 + 3 + 1 );// allocating enough space
			out[0] = in.size();
			unsigned char* pout = reinterpret_cast<unsigned char*>(& out[1]);
	        for(columntype::const_iterator i = in.begin(); i < in.end(); i+=BLOCKSIZE) {
	        	blockpointers.push_back(reinterpret_cast<unsigned int*>(pout)-&out.front());//;&out.back() + 1);
	        	if(i+BLOCKSIZE<=in.end())
	        		pout=compressBlock(i,i+BLOCKSIZE,pout);
	        	else
	        		pout=compressBlock(i,in.end(),pout);
	        }
	        assert(static_cast<uint>(ceildiv(  pout-reinterpret_cast<unsigned char*>(& out[0]) , sizeof(uint) )) <= out.size());
	        out.resize(ceildiv(  pout-reinterpret_cast<unsigned char*>(& out[0]) , sizeof(uint) ));
		}

		unsigned char* compressBlock(const columntype::const_iterator & begin, const columntype::const_iterator & end, unsigned char* pout) {

			const unsigned char* pin = reinterpret_cast<const unsigned char*>(& *begin);
			lzo_uint out_len (0);
#ifndef NDEBUG
			int r = lzo1x_1_compress(pin,(end-begin)*sizeof(uint),pout+sizeof(uint),&out_len,&wrkmem[0]);
			assert(r == LZO_E_OK);
#else
			lzo1x_1_compress(pin,(end-begin)*sizeof(uint),pout+sizeof(uint),&out_len,&wrkmem[0]);
#endif
			*reinterpret_cast<uint*>(pout) = static_cast<uint>(out_len);
			return pout + sizeof(uint) + ceildiv(reinterpret_cast<uint*>(pout)[0],sizeof(uint))*sizeof(uint);
		}

		// random access in a compressed array
		uint getValueAt(const columntype & in, const columntype & blockpointers, const uint index) {
			const uint whichblock = index/blocksize();
			const uint * startofblock = (& in.front()) + blockpointers[whichblock];
			assert(startofblock<=&in.back());
			assert(startofblock>=&in.front());
			// we do it the hard way:
			const unsigned char* pin = reinterpret_cast<const unsigned char*>(startofblock);
			unsigned char* pout = reinterpret_cast<unsigned char*>(& blockbuffer.front());
			uncompressBlock(pin, pout);
			return blockbuffer.at(index % blocksize());
		}

		void uncompress(const columntype & in, columntype & out){
			out.resize(in[0]);
			unsigned char* pout = reinterpret_cast<unsigned char*>(& out[0]);
			unsigned char* const lastpout = reinterpret_cast<unsigned char*>(&out[out.size()]);
			const unsigned char* pin = reinterpret_cast<const unsigned char*>(& in[1]);
			while(pout != lastpout) {
				pin = uncompressBlock(pin, pout);
			}
		}

		const unsigned char* uncompressBlock(const unsigned char* pin, unsigned char*& pout) {
			lzo_uint out_len(0);
#ifndef NDEBUG
			int r = lzo1x_decompress(pin+sizeof(uint),reinterpret_cast<const uint*>(pin)[0],pout,&out_len,NULL);
			assert(r == LZO_E_OK);
#else
			lzo1x_decompress(pin+sizeof(uint),reinterpret_cast<const uint*>(pin)[0],pout,&out_len,NULL);
#endif
			pout+=out_len;
			return pin+sizeof(uint)+ceildiv(reinterpret_cast<const uint*>(pin)[0],sizeof(uint))*sizeof(uint);
		}


		vector<unsigned char> wrkmem;
		uint bytesforcounter;

		vector<uint> blockbuffer;
};




#endif
