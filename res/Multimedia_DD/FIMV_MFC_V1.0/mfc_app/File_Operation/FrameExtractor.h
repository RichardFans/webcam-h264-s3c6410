#ifndef __SAMSUNG_SYSLSI_AP_FRAME_EXTRACTOR_H__
#define __SAMSUNG_SYSLSI_AP_FRAME_EXTRACTOR_H__


// 내부 QUEUE 버퍼의 크기
// delimiter의 길이는 이 QUEUE 버퍼 크기보다 작아야 한다.
#define QUEUE_CAPACITY		12

typedef void * FRAMEX_IN;

typedef enum
{
	FRAMEX_IN_TYPE_FILE,
	FRAMEX_IN_TYPE_MEM,

} FRAMEX_IN_TYPE;

// 다음은 FILE이 아닌 memory buffer의 stream 입력일 경우.. (FRAMEX_IN_TYPE_MEM 타입!)
typedef struct
{
	unsigned char *p_start;
	unsigned char *p_end;

	unsigned char *p_cur;
} FRAMEX_STRM_PTR;


typedef struct stFRAMEX_CTX
{
	FRAMEX_IN_TYPE   in_type;
	unsigned char   *delim_ptr;
	int              delim_leng;
	int              delim_insert;

	int              cont_offset;
} FRAMEX_CTX;



#ifdef __cplusplus
extern "C" {
#endif

FRAMEX_CTX *FrameExtractorInit(FRAMEX_IN_TYPE type, unsigned char delimiter[], int delim_leng, int delim_insert);
int FrameExtractorFirst(FRAMEX_CTX *pCTX, FRAMEX_IN in);
int FrameExtractorNext(FRAMEX_CTX *pCTX, FRAMEX_IN in, unsigned char outbuf[], int outbuf_size, int *n_fill);
int FrameExtractorPeek(FRAMEX_CTX *pCTX, FRAMEX_IN in, unsigned char peekbuf[], int peek_size, int *n_fill);
int FrameExtractorFinal(FRAMEX_CTX *pCTX);


#ifdef __cplusplus
}
#endif


#define FRAMEX_OK						0
#define FRAMEX_CONTINUE					(-1)
#define FRAMEX_ERR_NOTFOUND				(-2)
#define FRAMEX_ERR_EOS					(-3)
#define FRAMEX_ERR_BUFSIZE_TOO_SMALL	(-4)


#endif /* __SAMSUNG_SYSLSI_AP_FRAME_EXTRACTOR_H__ */
