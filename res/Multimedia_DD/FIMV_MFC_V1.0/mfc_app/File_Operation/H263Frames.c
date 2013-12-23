/////
///   MPEG4Frames.c
///
///   Written by Simon Chun (simon.chun@samsung.com)
///   2007/03/23
///

#include <stdio.h>
#include "FrameExtractor.h"
#include "H263Frames.h"


typedef struct tagH263_PICTURE_INFO
{
	unsigned int width;
	unsigned int height;
	unsigned int picture_coding_type;
} H263_PICTURE_INFO;


static unsigned int read_bits(unsigned char bytes[], int num_read, int *bit_offset)
{
	unsigned int   bits;
	unsigned int   utmp;

	int            i;
	int            bit_shift;

	int   byte_offset, bit_offset_in_byte;
	int   num_bytes_copy;


	if (num_read > 24)	// Max 24 bits까지만 지원된다.
		return 0xFFFFFFFF;
	if (num_read == 0)
		return 0;


	// byte_offset과
	// 그 byte 내에서 bit_offset을 구한다.
	byte_offset = (*bit_offset) >> 3;	// byte_offset = (*bit_offset) / 8
	bit_offset_in_byte = (*bit_offset) - (byte_offset << 3);


	num_bytes_copy = ((*bit_offset + num_read) >> 3) - (*bit_offset >> 3) + 1;
	bits = 0;
	for (i=0; i<num_bytes_copy; i++) {
		utmp = bytes[byte_offset + i];
		bits = (bits << 8) | (utmp);
	}

	bit_shift = (num_bytes_copy << 3) - (bit_offset_in_byte + num_read);
	bits >>= bit_shift;
	bits &= (0xFFFFFFFF >> (32 - num_read));

	*bit_offset += num_read;

	return bits;
}


static void get_h263_picture_info(unsigned char picture_header[], H263_PICTURE_INFO *h263_picture_info)
{
	int            bit_offset;
	unsigned int   utmp;


	bit_offset = 22;
	utmp = read_bits(picture_header, 8, &bit_offset);	// Temporal Reference
	bit_offset += 5;						// Type Information (Bit 1 ~ 5)
	utmp = read_bits(picture_header, 3, &bit_offset);	// Type Information (Bit 6 ~ 8) :  Source Format
	switch (utmp) {
	case 0x1:	// sub-QCIF
		h263_picture_info->width  = 88;
		h263_picture_info->height = 72;
		break;
	case 0x2:	// QCIF
		h263_picture_info->width  = 176;
		h263_picture_info->height = 144;
		break;
	case 0x3:	// CIF
		h263_picture_info->width  = 352;
		h263_picture_info->height = 288;
		break;
	case 0x4:	// 4CIF
		h263_picture_info->width  = 704;
		h263_picture_info->height = 576;
		break;
	case 0x5:	// 16CIF
		h263_picture_info->width  = 1408;
		h263_picture_info->height = 1152;
		break;
	case 0x0:	// forbidden
	case 0x6:	// reserved
	case 0x7:	// extended PTYPE
		h263_picture_info->width  = 0;
		h263_picture_info->height = 0;
		break;
	}


	// Type Information (Bit 9) :  0(I-pic), 1(P-pic)
	h263_picture_info->picture_coding_type = read_bits(picture_header, 1, &bit_offset);
}


int NextFrameH263(FRAMEX_CTX  *pFrameExCtx, void *fp, unsigned char buf[], int buf_size, unsigned int *coding_type)
{
	int                i;
	int                ret;
	int                nFrameSize;

	H263_PICTURE_INFO  h263_picture_info;


	for (i=0; i<5; i++) {
		ret = FrameExtractorNext(pFrameExCtx, fp, buf, buf_size, (int *)&nFrameSize);
		if (ret != FRAMEX_OK)
			return 0;

		if (coding_type) {
			get_h263_picture_info(buf, &h263_picture_info);
			*coding_type = h263_picture_info.picture_coding_type;
		}

		break;
	}

	return nFrameSize;
}


int ExtractConfigStreamH263(FRAMEX_CTX  *pFrameExCtx,
                            void *fp,
                            unsigned char buf[],
                            int buf_size,
                            H263_CONFIG_DATA *conf_data)
{
	int                nStreamSize;

	H263_PICTURE_INFO  h263_picture_info;


	nStreamSize = NextFrameH263(pFrameExCtx, fp, buf, buf_size, NULL);

	if (conf_data) {
		get_h263_picture_info(buf, &h263_picture_info);
		conf_data->width  = h263_picture_info.width;
		conf_data->height = h263_picture_info.height;
	}

	return nStreamSize;
}

/*
int ExtractConfigStreamH263(FRAMEX_CTX  *pFrameExCtx, void *fp, unsigned char buf[], int buf_size)
{
	int                i;
	int                ret;
	unsigned char      frame_type[4];
	int                nStreamSize, nFrameSize;


	for (i=0, nStreamSize=0; i<20; i++) {

		ret = FrameExtractorPeek(pFrameExCtx, fp, frame_type, sizeof(frame_type), (int *)&nFrameSize);
		if (frame_type[3] == 0xB6) {
			break;
		}

		ret = FrameExtractorNext(pFrameExCtx, fp, buf + nStreamSize, buf_size - nStreamSize, (int *)&nFrameSize);
		if (ret != FRAMEX_OK)
			break;

		nStreamSize += nFrameSize;
	}

	return nStreamSize;
}
*/
