/////
///   MPEG4Frames.c
///
///   Written by Simon Chun (simon.chun@samsung.com)
///   2007/03/23
///

#include <stdio.h>
#include <string.h>

#include "FrameExtractor.h"
#include "H263Frames.h"
#include "FileRead.h"


typedef struct tagH263_PICTURE_INFO
{
	unsigned int width;
	unsigned int height;
	unsigned int picture_coding_type;
} H263_PICTURE_INFO;


static unsigned char h263_delimiter[3];


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


static int first_h263_frame(void *fp, unsigned char buf[])
{
	int             iRet;
	int             nOffset;
	unsigned int    nread;
	unsigned int    delimiter;
	unsigned char  *p_delim = (unsigned char *) &delimiter;


	// Find first frame delimiter
	iRet = SSB_FILE_READ(fp, p_delim+1, 3, &nread);

	if (iRet != 3)
		return 0;
	nOffset = 3;

	do {

		// Little endian 경우
		if ((delimiter & 0xFCFFFF00) == 0x80000000)
			break;
		delimiter >>= 8;
/*	// Big endian 경우
		if ((delimiter & 0x00FFFFFC) == 0x00000080)
			break;
		delimiter <<= 8;
*/
		iRet = SSB_FILE_READ(fp, p_delim+3, 1, &nread);
		if (iRet != 1)
			return 0;
		nOffset++;
	} while (1);


	// copy delimiter
	memcpy(h263_delimiter, p_delim+1, 3);


	return nOffset;
}


static int next_h263_frame(void *fp,
                           unsigned char buf[],
                           int buf_size)
{
	int             iRet;

	int             nStreamSize;

	unsigned int    nread;
	unsigned int    delimiter;
	unsigned char  *p_delim = (unsigned char *) &delimiter;

	unsigned char  *p_buf;



	// Find next frame delimiter
	iRet = SSB_FILE_READ(fp, p_delim+1, 3, &nread);
	if (iRet == SSB_FILE_EOF)
		return 0;

	memcpy(buf, p_delim+1, 3);
	p_buf = buf + 3;
	do {
		if ((delimiter & 0xFCFFFF00) == 0x80000000) {
			nStreamSize = (int) p_buf - (int) buf - 3;
			memcpy(h263_delimiter, p_delim+1, 3);
			break;
		}
		delimiter >>= 8;

		iRet = SSB_FILE_READ(fp, p_delim+3, 1, &nread);
		if (iRet != 1) {
			nStreamSize = (int) p_buf - (int) buf;
			break;
		}
		*p_buf = *(p_delim+3); p_buf++;
	} while (1);

	return nStreamSize;
}


int NextFrameH263(void *fp, unsigned char buf[], int buf_size, unsigned int *coding_type)
{
	int                nFrameSize;

	H263_PICTURE_INFO  h263_picture_info;


	// delimiter copy
	memcpy(buf, h263_delimiter, 3);

	nFrameSize = next_h263_frame(fp, buf+3, buf_size);
	if (nFrameSize == 0)
		return 0;

	if (coding_type) {
		get_h263_picture_info(buf, &h263_picture_info);
		*coding_type = h263_picture_info.picture_coding_type;
	}

	return (nFrameSize + 3);
}


int ExtractConfigStreamH263(void *fp,
                            unsigned char buf[],
                            int buf_size,
                            H263_CONFIG_DATA *conf_data)
{
	int                	nFrameSize=0;
	int             	iRet;
	H263_PICTURE_INFO  	h263_picture_info;


	// Find the first h.263 delimiter
	iRet = first_h263_frame(fp, buf);

	// delimiter copy
	memcpy(buf, h263_delimiter, 3);

	// Find the next h.263 delimiter & copy the first h.263 frame into buf
	nFrameSize = next_h263_frame(fp, buf + 3, buf_size - 3);

	if (conf_data) {
		get_h263_picture_info(buf, &h263_picture_info);
		conf_data->width  = h263_picture_info.width;
		conf_data->height = h263_picture_info.height;
	}

	return (nFrameSize + 3);
}

