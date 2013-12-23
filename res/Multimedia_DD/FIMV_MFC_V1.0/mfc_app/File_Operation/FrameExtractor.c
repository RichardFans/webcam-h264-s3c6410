#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FrameExtractor.h"

//
// FRAMEX_CTX *FrameExtractorInit(unsigned char delimiter[], int delim_leng, int delim_insert)
//
// Description
//		This function initializes the FRAMEX_CTX structure with the delimiter
// Return
//		FRAME_CTX* : FRAMEX_CTX structure. It should be released by FrameExtractorFinal() function.
// Parameters
//		delimiter    [IN]: pointer to the array that holds the delimiter octets.
//		delim_leng   [IN]: length of the delimiter octets
//		delim_insert [IN]: 0-delimiter를 outbuf에 넣지 않음. 1-delimiter를 outbuf에 넣음
//
FRAMEX_CTX *FrameExtractorInit(FRAMEX_IN_TYPE type, unsigned char delimiter[], int delim_leng, int delim_insert)
{
	FRAMEX_CTX *pCTX;

	// parameter checking
	if (delimiter == NULL || (delim_leng <= 0 || delim_leng > QUEUE_CAPACITY))
		return NULL;


	pCTX = (FRAMEX_CTX *) malloc(sizeof(FRAMEX_CTX));
	memset(pCTX, 0, sizeof(FRAMEX_CTX));

	pCTX->in_type    = type;
	pCTX->delim_ptr  = (unsigned char *) malloc(delim_leng);
	pCTX->delim_leng = delim_leng;
	memcpy(pCTX->delim_ptr, delimiter, delim_leng);

	pCTX->delim_insert = delim_insert;
	pCTX->cont_offset  = 0;

	return pCTX;
}


#define Q_PUSH(value, queue, qp_s, qp_e, q_size, q_capacity)	\
{	\
	queue[qp_s] = value;	\
	qp_s = (qp_s + 1) % q_capacity;	\
	q_size++;	\
}

#define Q_POP(value, queue, qp_s, qp_e, q_size, q_capacity)	\
{	\
	value = queue[qp_e];	\
	qp_e = (qp_e + 1) % q_capacity;	\
	q_size--;	\
}

#define Q_PEEK(value, queue, qp_s, qp_e)	\
{	\
	value = queue[qp_e];	\
}

// 다음의 delimiter를 찾는다.
static int next_delimiter(FRAMEX_CTX *pCTX, FILE *fpin, unsigned char *outbuf, const int outbuf_size, int *n_fill)
{
	int r = 0;
	int l;
	int nbytes_to_write;
	int qp_s, qp_e, q_size;
	unsigned char queue[QUEUE_CAPACITY], b, c;

	int offset;

	offset = 0;
	qp_s = qp_e = q_size = 0;
	nbytes_to_write = outbuf_size;

	// 리턴할 outbuf 버퍼의 맨 앞에 delimiter를 채워넣는다.
	if (outbuf != NULL) {
		if (pCTX->cont_offset == 0) {
			if (nbytes_to_write <= pCTX->delim_leng) {
				return FRAMEX_ERR_BUFSIZE_TOO_SMALL;
			}

			if (pCTX->delim_insert) {
				memcpy(outbuf, pCTX->delim_ptr, pCTX->delim_leng);
				outbuf += pCTX->delim_leng;
				nbytes_to_write -= pCTX->delim_leng;	// 가능한 크기를 delimiter 길이만큼 빼야함
				*n_fill = pCTX->delim_leng;
			}
			else
				*n_fill = 0;
		}
		else {
			// continue로 들어온 경우는 (pCTX->cont_offset != 0)
			offset = pCTX->cont_offset;
			outbuf += (pCTX->cont_offset - pCTX->delim_leng);

			for (l=0; l<pCTX->delim_leng; l++) {
				Q_PUSH(*outbuf, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
				outbuf++;
			}

		}
	}

	for (l=0; l<pCTX->delim_leng; l++) {
		if (q_size == 0 || l == q_size) {

			if (outbuf != NULL && offset >= nbytes_to_write) {
				if (pCTX->cont_offset == 0)
					pCTX->cont_offset = (pCTX->delim_leng + offset);
				else
					pCTX->cont_offset = offset;
				return FRAMEX_CONTINUE;
			}

			r = fread(&b, 1, 1, fpin);
			if (r != 1)
				break;
			offset++;
			if (outbuf != NULL) {
				*outbuf = b; outbuf++; (*n_fill)++;
			}
		}
		else {
			r = -1;
			Q_PEEK(b, queue, qp_s, qp_e)
		}


		if (b == pCTX->delim_ptr[l]) {
			if (r == 1)	// 새로 읽은 경우에만 PUSH한다.
				Q_PUSH(b, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
		}
		else {
			if (r != 1)
				// 새로 읽은 경우가 아니라면, 하나 빼버린다.
				Q_POP(c, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
			else if (l > 0) {
				// 새로 읽은 경우 중에서, delimiter의 첫 octet(l=0)을 비교하는 경우는
				// QUEUE에 넣지 않고 바로 넘어간다.
				// 2번째 octet이상(l>0)을 비교하는 경우는,
				// 맨 앞에 하나를 빼고 맨 뒤에 새로 하나를 넣는다.

				Q_POP(c, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
				Q_PUSH(b, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
			}

			l = -1;
		}

	}

	if (r == 1) {
		if (outbuf != NULL) {
			*n_fill -= pCTX->delim_leng;
			pCTX->cont_offset = 0;
		}

		return FRAMEX_OK;
	}
	else if (r == 0)
		return FRAMEX_ERR_EOS;

	return FRAMEX_ERR_NOTFOUND;
}


// 다음의 frame에서 원하는 수의 octets 값을 확인만 한다.
static int next_frame_peek(FRAMEX_CTX *pCTX, FILE *fpin, unsigned char *peekbuf, const int peek_size, int *n_fill)
{
	int r = 0;
	int l;
	int nbytes_to_peek;
	int qp_s, qp_e, q_size;
	unsigned char queue[QUEUE_CAPACITY], b, c;

	int offset;

	offset = 0;
	qp_s = qp_e = q_size = 0;
	nbytes_to_peek = peek_size;

	// 리턴할 outbuf 버퍼의 맨 앞에 delimiter를 채워넣는다.
	if (peekbuf != NULL) {
		if (pCTX->cont_offset == 0) {
			if (nbytes_to_peek <= pCTX->delim_leng) {
				return FRAMEX_ERR_BUFSIZE_TOO_SMALL;
			}

			if (pCTX->delim_insert) {
				memcpy(peekbuf, pCTX->delim_ptr, pCTX->delim_leng);
				peekbuf += pCTX->delim_leng;
				nbytes_to_peek -= pCTX->delim_leng;	// 가능한 크기를 delimiter 길이만큼 빼야함
				*n_fill = pCTX->delim_leng;
			}
			else
				*n_fill = 0;
		}
		else {
			// continue로 들어온 경우는 ERROR다.

		}
	}

	for (l=0; l<pCTX->delim_leng; l++) {
		if (q_size == 0 || l == q_size) {

			r = fread(&b, 1, 1, fpin);
			if (r != 1)
				break;
			offset++;
			if (peekbuf != NULL) {
				*peekbuf = b; peekbuf++; (*n_fill)++;
			}

			// fread 개수가 peek_size가 되면, 탈출한다.
			if (offset >= nbytes_to_peek)
				break;
		}
		else {
			r = -1;
			Q_PEEK(b, queue, qp_s, qp_e)
		}


		if (b == pCTX->delim_ptr[l]) {
			if (r == 1)	// 새로 읽은 경우에만 PUSH한다.
				Q_PUSH(b, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
		}
		else {
			if (r != 1)
				// 새로 읽은 경우가 아니라면, 하나 빼버린다.
				Q_POP(c, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
			else if (l > 0) {
				// 새로 읽은 경우 중에서, delimiter의 첫 octet(l=0)을 비교하는 경우는
				// QUEUE에 넣지 않고 바로 넘어간다.
				// 2번째 octet이상(l>0)을 비교하는 경우는,
				// 맨 앞에 하나를 빼고 맨 뒤에 새로 하나를 넣는다.

				Q_POP(c, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
				Q_PUSH(b, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
			}

			l = -1;
		}

	}

	if (r == 1) {
		if (peekbuf != NULL) {
			*n_fill -= pCTX->delim_leng;
			pCTX->cont_offset = 0;
		}

		fseek(fpin, -(peek_size-pCTX->delim_leng), SEEK_CUR);

		return FRAMEX_OK;
	}
	else if (r == 0)
		return FRAMEX_ERR_EOS;

	return FRAMEX_ERR_NOTFOUND;
}


// 다음의 delimiter를 찾는다.
static int next_delimiter_mem(FRAMEX_CTX *pCTX, FRAMEX_STRM_PTR *strm_ptr, unsigned char *outbuf, const int outbuf_size, int *n_fill)
{
	int r = 0;
	int l;
	int nbytes_to_write;
	int qp_s, qp_e, q_size;
	unsigned char queue[QUEUE_CAPACITY], b, c;

	int offset;

	offset = 0;
	qp_s = qp_e = q_size = 0;
	nbytes_to_write = outbuf_size;

	// 리턴할 outbuf 버퍼의 맨 앞에 delimiter를 채워넣는다.
	if (outbuf != NULL) {
		if (pCTX->cont_offset == 0) {
			if (nbytes_to_write <= pCTX->delim_leng) {
				return FRAMEX_ERR_BUFSIZE_TOO_SMALL;
			}

			if (pCTX->delim_insert) {
				memcpy(outbuf, pCTX->delim_ptr, pCTX->delim_leng);
				outbuf += pCTX->delim_leng;
				nbytes_to_write -= pCTX->delim_leng;	// 가능한 크기를 delimiter 길이만큼 빼야함
				*n_fill = pCTX->delim_leng;
			}
			else
				*n_fill = 0;
		}
		else {
			// continue로 들어온 경우는 (pCTX->cont_offset != 0)
			offset = pCTX->cont_offset;
			outbuf += (pCTX->cont_offset - pCTX->delim_leng);

			for (l=0; l<pCTX->delim_leng; l++) {
				Q_PUSH(*outbuf, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
				outbuf++;
			}

		}
	}

	for (l=0; l<pCTX->delim_leng; l++) {
		if (q_size == 0 || l == q_size) {

			if (outbuf != NULL && offset >= nbytes_to_write) {
				if (pCTX->cont_offset == 0)
					pCTX->cont_offset = (pCTX->delim_leng + offset);
				else
					pCTX->cont_offset = offset;
				return FRAMEX_CONTINUE;
			}

			if ((int) strm_ptr->p_cur > (int) strm_ptr->p_end)
				break;
			b = *(strm_ptr->p_cur); strm_ptr->p_cur++; r = 1;
			offset++;
			if (outbuf != NULL) {
				*outbuf = b; outbuf++; (*n_fill)++;
			}
		}
		else {
			r = -1;
			Q_PEEK(b, queue, qp_s, qp_e)
		}


		if (b == pCTX->delim_ptr[l]) {
			if (r == 1)	// 새로 읽은 경우에만 PUSH한다.
				Q_PUSH(b, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
		}
		else {
			if (r != 1)
				// 새로 읽은 경우가 아니라면, 하나 빼버린다.
				Q_POP(c, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
			else if (l > 0) {
				// 새로 읽은 경우 중에서, delimiter의 첫 octet(l=0)을 비교하는 경우는
				// QUEUE에 넣지 않고 바로 넘어간다.
				// 2번째 octet이상(l>0)을 비교하는 경우는,
				// 맨 앞에 하나를 빼고 맨 뒤에 새로 하나를 넣는다.

				Q_POP(c, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
				Q_PUSH(b, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
			}

			l = -1;
		}

	}

	if (r == 1) {
		if (outbuf != NULL) {
			*n_fill -= pCTX->delim_leng;
			pCTX->cont_offset = 0;
		}

		return FRAMEX_OK;
	}
	else if (r == 0)
		return FRAMEX_ERR_EOS;

	return FRAMEX_ERR_NOTFOUND;
}


// 다음의 frame에서 원하는 수의 octets 값을 확인만 한다.
static int next_frame_peek_mem(FRAMEX_CTX *pCTX, FRAMEX_STRM_PTR *strm_ptr, unsigned char *peekbuf, const int peek_size, int *n_fill)
{
	int r = 0;
	int l;
	int nbytes_to_peek;
	int qp_s, qp_e, q_size;
	unsigned char queue[QUEUE_CAPACITY], b, c;

	int offset;

	offset = 0;
	qp_s = qp_e = q_size = 0;
	nbytes_to_peek = peek_size;

	// 리턴할 outbuf 버퍼의 맨 앞에 delimiter를 채워넣는다.
	if (peekbuf != NULL) {
		if (pCTX->cont_offset == 0) {
			if (nbytes_to_peek <= pCTX->delim_leng) {
				return FRAMEX_ERR_BUFSIZE_TOO_SMALL;
			}

			if (pCTX->delim_insert) {
				memcpy(peekbuf, pCTX->delim_ptr, pCTX->delim_leng);
				peekbuf += pCTX->delim_leng;
				nbytes_to_peek -= pCTX->delim_leng;	// 가능한 크기를 delimiter 길이만큼 빼야함
				*n_fill = pCTX->delim_leng;
			}
			else
				*n_fill = 0;
		}
		else {
			// continue로 들어온 경우는 ERROR다.

		}
	}

	for (l=0; l<pCTX->delim_leng; l++) {
		if (q_size == 0 || l == q_size) {

			if ((int) strm_ptr->p_cur > (int) strm_ptr->p_end)
				break;
			b = *(strm_ptr->p_cur); strm_ptr->p_cur++; r = 1;
			offset++;
			if (peekbuf != NULL) {
				*peekbuf = b; peekbuf++; (*n_fill)++;
			}

			// fread 개수가 peek_size가 되면, 탈출한다.
			if (offset >= nbytes_to_peek)
				break;
		}
		else {
			r = -1;
			Q_PEEK(b, queue, qp_s, qp_e)
		}


		if (b == pCTX->delim_ptr[l]) {
			if (r == 1)	// 새로 읽은 경우에만 PUSH한다.
				Q_PUSH(b, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
		}
		else {
			if (r != 1)
				// 새로 읽은 경우가 아니라면, 하나 빼버린다.
				Q_POP(c, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
			else if (l > 0) {
				// 새로 읽은 경우 중에서, delimiter의 첫 octet(l=0)을 비교하는 경우는
				// QUEUE에 넣지 않고 바로 넘어간다.
				// 2번째 octet이상(l>0)을 비교하는 경우는,
				// 맨 앞에 하나를 빼고 맨 뒤에 새로 하나를 넣는다.

				Q_POP(c, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
				Q_PUSH(b, queue, qp_s, qp_e, q_size, QUEUE_CAPACITY)
			}

			l = -1;
		}

	}

	if (r == 1) {
		if (peekbuf != NULL) {
			*n_fill -= pCTX->delim_leng;
			pCTX->cont_offset = 0;
		}

//		fseek(fpin, -(peek_size-pCTX->delim_leng), SEEK_CUR);
		strm_ptr->p_cur = (strm_ptr->p_cur  -  (peek_size-pCTX->delim_leng));

		return FRAMEX_OK;
	}
	else if (r == 0)
		return FRAMEX_ERR_EOS;

	return FRAMEX_ERR_NOTFOUND;
}


//
// int FrameExtractorFirst(FRAMEX_CTX *pCTX, FILE *fpin)
//
// Description
//		제일 처음의 delimiter가 나오는 것을 찾는다.
// Return
//
// Parameters
//
int FrameExtractorFirst(FRAMEX_CTX *pCTX, FRAMEX_IN in)
{
	if (pCTX->in_type == FRAMEX_IN_TYPE_FILE)
		return next_delimiter(pCTX, (FILE *) in, NULL, 0, NULL);
	else
		return next_delimiter_mem(pCTX, (FRAMEX_STRM_PTR *) in, NULL, 0, NULL);
}

//
// int FrameExtractorNext(FRAMEX_CTX *pCTX, FILE *fpin, unsigned char outbuf[], int outbuf_size, int *n_fill)
//
// Description
//		FrameExtractorFirst() 함수를 호출하여 첫 frame의 delimiter를 찾은 후부터 호출한다.
//		다음 frame의 delimiter가 나올때까지의 frame을 추출하여, outbuf에 복사한다.
// Return
//
// Parameters
//		outbuf     [OUT]: pointer to the array that will be filled with the frame data
//		outbuf_size[IN] : length of the outbuf size
//		n_fill     [OUT]: number of octets filled in the out_buf
//
int FrameExtractorNext(FRAMEX_CTX *pCTX, FRAMEX_IN in, unsigned char outbuf[], int outbuf_size, int *n_fill)
{
	if (pCTX->in_type == FRAMEX_IN_TYPE_FILE)
		return next_delimiter(pCTX, (FILE *) in, outbuf, outbuf_size, n_fill);
	else
		return next_delimiter_mem(pCTX, (FRAMEX_STRM_PTR *) in, outbuf, outbuf_size, n_fill);
}


//
// int FrameExtractorPeek(FRAMEX_CTX *pCTX, FILE *fpin, unsigned char peekbuf[], int peek_size, int *n_fill)
//
// Description
//		FrameExtractorFirst() 함수를 호출하여 첫 frame의 delimiter를 찾은 후부터 호출한다.
//		다음 frame의 값을 peek_size만큼 peekbuf에 복사한다.
//		이 때, Buffer를 PEEK만 하는 것이므로, frame을 추출하는 것은 아니다.
// Return
//
// Parameters
//		peekbuf    [OUT]: pointer to the array that will be filled with the frame data
//		peek_size  [IN] : length of the outbuf size
//		n_fill     [OUT]: number of octets filled in the out_buf
//
int FrameExtractorPeek(FRAMEX_CTX *pCTX, FRAMEX_IN in, unsigned char peekbuf[], int peek_size, int *n_fill)
{
	if (pCTX->in_type == FRAMEX_IN_TYPE_FILE)
		return next_frame_peek(pCTX, (FILE *) in, peekbuf, peek_size, n_fill);
	else
		return next_frame_peek_mem(pCTX, (FRAMEX_STRM_PTR *) in, peekbuf, peek_size, n_fill);
}



//
// int FrameExtractorFinal(FRAMEX_CTX *pCTX)
//
// Description
//		FRAMEX_CTX 구조체 데이터를 해제한다.
// Return
//
// Parameters
//
int FrameExtractorFinal(FRAMEX_CTX *pCTX)
{
	free(pCTX->delim_ptr);
	free(pCTX);

	return 0;
}
