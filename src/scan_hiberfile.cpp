#include "bulk_extractor.h"
#include "image_process.h"
#include "pyxpress.h"


#include <stdlib.h>
#include <string.h>


#define ZLIB_CONST
#ifdef HAVE_DIAGNOSTIC_UNDEF
#  pragma GCC diagnostic ignored "-Wundef"
#endif
#ifdef HAVE_DIAGNOSTIC_CAST_QUAL
#  pragma GCC diagnostic ignored "-Wcast-qual"
#endif
#include <zlib.h>

#include <iostream>
#include <iomanip>
#include <cassert>

using namespace std;

/**
 * scan_hiberfile:
 * Look for elements of the hibernation file and decompress them.
 */

extern "C"
void scan_hiberfile(const class scanner_params &sp,const recursion_control_block &rcb)
{
    assert(sp.sp_version==scanner_params::CURRENT_SP_VERSION);
    if(sp.phase==scanner_params::startup){
        assert(sp.info->si_version==scanner_info::CURRENT_SI_VERSION);
	sp.info->name  = "hiber";
        sp.info->author         = "Simson Garfinkel and Matthieu Suiche";
        sp.info->description    = "Scans for Microsoft-XPress compressed data";
        sp.info->scanner_version= "1.0";
	return; /* no features */
    }
    if(sp.phase==scanner_params::shutdown) return;
    if(sp.phase==scanner_params::scan){

	/* Do not scan for hibernation decompression if we are already
	 * inside a hibernation file decompression.
	 * Right now this is a hack; it should be done by the system with some kind of flag.
	 */
	const sbuf_t &sbuf = sp.sbuf;
	const pos0_t &pos0 = sp.sbuf.pos0;

	if(pos0.path.find("HIBER")!=string::npos){
	    return;
	}


	for(const unsigned char *cc=sbuf.buf ;
	    cc < sbuf.buf + sbuf.pagesize && cc<sbuf.buf+sbuf.bufsize-38 ;
	    cc++){

	    /**
	     * http://www.pyflag.net/pyflag/src/lib/pyxpress.c
	     */
	    if(cc[0]==0x81 && cc[1]==0x81 && cc[2]==0x78 && cc[3]==0x70 &&
	       cc[4]==0x72 && cc[5]==0x65 && cc[6]==0x73 && cc[7]==0x73){

		u_int compressed_length = (((cc[9]<<8) + (cc[10] << 16) + (cc[11]<<24)) >> 10) + 1;
		const u_char *compressed_buf = cc+32;		 // "the header contains 32 bytes"
		u_int  remaining_size = sbuf.bufsize - (compressed_buf-sbuf.buf); // up to the end of the buffer
		size_t compr_size = compressed_length < remaining_size ? compressed_length : remaining_size;
		size_t max_uncompr_size_ = compr_size * 10; // hope that's good enough
		if(max_uncompr_size_<4096) max_uncompr_size_=4096; // it should at least be this large!

		managed_malloc<u_char>decomp(max_uncompr_size_);


		int decompress_size = Xpress_Decompress(compressed_buf,compr_size,
							decomp.buf,max_uncompr_size_);

		if(decompress_size>0){
		    const ssize_t pos = cc-sbuf.buf;
		    const pos0_t pos0_hiber = (pos0 + pos) + rcb.partName;
		    const sbuf_t sbuf_new(pos0_hiber,decomp.buf,decompress_size,decompress_size,false);
		    (*rcb.callback)(scanner_params(sp,sbuf_new)); // recurse
		    if(rcb.returnAfterFound){
			return;
		    }
		}
	    }
	}
    }
}
