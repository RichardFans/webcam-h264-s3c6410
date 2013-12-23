/////
///   VC1Frames.c
///
///   Written by Simon Chun (simon.chun@samsung.com)
///   2007/06/18
///	  2007/10/07
///

#include <stdio.h>
#include <string.h>

#include "FrameExtractor.h"
#include "VC1Frames.h"
#include "FileRead.h"


typedef struct
{
	unsigned int   num_frames : 24;
	unsigned int   const_C5   : 8;
	unsigned int   const_04;
	unsigned int   struct_c;
	unsigned int   struct_a_vert;
	unsigned int   struct_a_horz;
	unsigned int   const_0C;
	unsigned int   struct_b_1;
	unsigned int   struct_b_2;
	unsigned int   struct_b_3;
} VC1_SEQ_LAYER;

typedef struct
{
	unsigned int   frame_size : 24;
	unsigned int   res        : 7;
	unsigned int   key        : 1;
	unsigned int   timestamp;
} VC1_FRM_LAYER;


int NextFrameVC1(void *fp, unsigned char buf[], int buf_size, unsigned int *coding_type)
{
	VC1_FRM_LAYER  vc1_frm_hdr;
	unsigned int   nread;
	int            nFrameSize;
	int            iRet;


	iRet = SSB_FILE_READ(fp, (unsigned char *) &vc1_frm_hdr, sizeof(VC1_FRM_LAYER), &nread);
	if ((iRet == SSB_FILE_EOF) || (nread != sizeof(VC1_FRM_LAYER)))
		return 0;

	memcpy(buf, &vc1_frm_hdr, nread);

	iRet = SSB_FILE_READ(fp, buf + nread, vc1_frm_hdr.frame_size, &nread);
	if ((iRet == SSB_FILE_EOF) || (nread != vc1_frm_hdr.frame_size))
		return 0;

	nFrameSize = sizeof(VC1_FRM_LAYER) + nread;

	if (coding_type)
		*coding_type = -1;	// Not implemented yet

	return nFrameSize;
}

int ExtractConfigStreamVC1(void *fp, unsigned char buf[], int buf_size, VC1_CONFIG_DATA *conf_data)
{
	VC1_SEQ_LAYER  vc1_seq_hdr;
	unsigned int   nread;
	int            iRet;


	iRet = SSB_FILE_READ(fp, (unsigned char *) &vc1_seq_hdr, sizeof(VC1_SEQ_LAYER), &nread);
	if ((iRet == SSB_FILE_EOF) || (nread != sizeof(VC1_SEQ_LAYER)))
		return 0;

	if (vc1_seq_hdr.const_C5 != 0xC5) {
		return 0;
	}

	memcpy(buf, &vc1_seq_hdr, sizeof(VC1_SEQ_LAYER));

	if (conf_data) {
		conf_data->width  = vc1_seq_hdr.struct_a_horz;
		conf_data->height = vc1_seq_hdr.struct_a_vert;
	}

	nread = NextFrameVC1(fp, buf + sizeof(VC1_SEQ_LAYER), buf_size - sizeof(VC1_SEQ_LAYER), NULL);

	return (sizeof(VC1_SEQ_LAYER) + nread);
}

