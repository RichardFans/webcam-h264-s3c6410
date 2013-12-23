/////
///   MPEG4Frames.c
///
///   Written by Simon Chun (simon.chun@samsung.com)
///   2007/03/23
///

#include <stdio.h>
#include "FrameExtractor.h"
#include "MPEG4Frames.h"


static int find_one(unsigned int bits8)
{
	if (bits8 >= 8)
		return 4;
	else if (bits8 >= 4)
		return 3;
	else if (bits8 >= 2)
		return 2;
	else if (bits8 == 1)
		return 1;

	return 0;
}

static int num_bits(unsigned int bits)
{
	if (bits & 0xFFFF0000) {
		if (bits & 0xF0000000) {
			return (find_one(bits >> 28) + 28);
		}
		else if (bits & 0x0F000000) {
			return (find_one(bits >> 24) + 24);
		}
		else if (bits & 0x00F00000) {
			return (find_one(bits >> 20) + 20);
		}
		else {
			return (find_one(bits) + 16);
		}
	}
	else if (bits & 0x0000FFFF) {
		if (bits & 0x0000F000) {
			return (find_one(bits >> 12) + 12);
		}
		else if (bits & 0x00000F00) {
			return (find_one(bits >> 8) + 8);
		}
		else if (bits & 0x000000F0) {
			return (find_one(bits >> 4) + 4);
		}
		else {
			return find_one(bits);
		}
	}

	return 0;
}

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


int ExtractConfigStreamMpeg4(FRAMEX_CTX  *pFrameExCtx,
                             void *fp,
                             unsigned char buf[],
                             int buf_size,
                             MPEG4_CONFIG_DATA *conf_data)
{
	int                i;
	int                ret;
	unsigned char      frame_type[4];
	int                nStreamSize, nFrameSize;
	int                includeIframe=1;
	unsigned int       coding_type;

	int                bit_offset;
	int                nbits;
	unsigned char      utmp, *ptmp;
	unsigned int       video_object_layer_shape, vop_time_increment_resolution;


	for (i=0, nStreamSize=0; i<30; i++) {

		ret = FrameExtractorPeek(pFrameExCtx, fp, frame_type, sizeof(frame_type), (int *)&nFrameSize);
		if (frame_type[3] == 0xB6) {
			break;
		}

		ret = FrameExtractorNext(pFrameExCtx, fp, buf + nStreamSize, buf_size - nStreamSize, (int *)&nFrameSize);
		if (ret != FRAMEX_OK)
			break;

		if ((buf[nStreamSize + 3] & 0xF0) == 0x20) {	// Video Object Layer
			ptmp = buf + nStreamSize + 4;
			bit_offset = 0;

			if (conf_data != NULL) {

				utmp = read_bits(ptmp, 1, &bit_offset);		// random_accessible_vol
				utmp = read_bits(ptmp, 8, &bit_offset);		// video_object_type_indication
				utmp = read_bits(ptmp, 1, &bit_offset);		// is_object_layer_identifier
				if (utmp) {
					utmp = read_bits(ptmp, 4, &bit_offset);		// video_object_layer_verid
					utmp = read_bits(ptmp, 3, &bit_offset);		// video_object_layer_priority
				}
				utmp = read_bits(ptmp, 4, &bit_offset);		// aspect_ratio_info
				utmp = read_bits(ptmp, 1, &bit_offset);		// vol_control_parameters
				if (utmp) {
					bit_offset += 3;
					utmp = read_bits(ptmp, 1, &bit_offset);		// vbv_parameters
					if (utmp) {
						bit_offset += 63;
					}
				}

				video_object_layer_shape = read_bits(ptmp, 2, &bit_offset);		// video_object_layer_shape


				utmp = read_bits(ptmp, 1, &bit_offset);		// marker_bit
				vop_time_increment_resolution = read_bits(ptmp, 16, &bit_offset);	// vop_time_increment_resolution
				utmp = read_bits(ptmp, 1, &bit_offset);		// marker_bit
				utmp = read_bits(ptmp, 1, &bit_offset);		// fixed_vop_rate
				if (utmp) {
					nbits = num_bits(vop_time_increment_resolution);
					utmp  = read_bits(ptmp, nbits, &bit_offset);	// fixed_vop_time_increment
				}

				if (video_object_layer_shape == 0) {
					utmp = read_bits(ptmp, 1, &bit_offset);		// marker_bit
					conf_data->width   = read_bits(ptmp, 13, &bit_offset);	// video_object_layer_width
					utmp = read_bits(ptmp, 1, &bit_offset);		// marker_bit
					conf_data->height  = read_bits(ptmp, 13, &bit_offset);	// video_object_layer_height
					utmp = read_bits(ptmp, 1, &bit_offset);		// marker_bit
				}
				utmp = read_bits(ptmp, 1, &bit_offset);		// interlaced
			}
		}


		nStreamSize += nFrameSize;
	}


	// includeIframe 가 1인 경우는
	// 리턴되는 config stream에 I-frame을 포함시킬지 결정한다.
	if (includeIframe == 1) {
		nFrameSize = NextFrameMpeg4(pFrameExCtx, fp, buf + nStreamSize, buf_size - nStreamSize, &coding_type);

		nStreamSize += nFrameSize;
	}


	return nStreamSize;
}



int NextFrameMpeg4(FRAMEX_CTX  *pFrameExCtx, void *fp, unsigned char buf[], int buf_size, unsigned int *coding_type)
{
	int                i;
	int                ret;
	int                nStreamSize, nFrameSize;

	int                bit_offset;



	nStreamSize=0;

	for (i=0; i<20; i++) {
		ret = FrameExtractorNext(pFrameExCtx, fp, buf + nStreamSize, buf_size - nStreamSize, (int *)&nFrameSize);
		if (ret != FRAMEX_OK)
			return 0;

		if (buf[nStreamSize + 3] == 0xB6) {	// VIDEO OBJECT인 경우에 리턴
			if (coding_type) {
				bit_offset = 0;
				*coding_type = read_bits(buf + nStreamSize + 4, 2, &bit_offset);		// vop_coding_type
			}

			nStreamSize += nFrameSize;
			break;
		}

		nStreamSize += nFrameSize;
	}

	return nStreamSize;
}
