#include <stdio.h>
#include <string.h>

#include "FileRead.h"

/*
static void *FileOpen(LPCTSTR filename)
{
	return (void *) fopen(filename, "rb");
}

static void FileRewind(void *p)
{
	fseek((FILE *) p, 0, SEEK_SET);
}

static int FileRead(void *p, unsigned char *buf, unsigned int num_read, unsigned int *num_written)
{
	if (feof((FILE *) p) != 0)
		return SSB_FILE_EOF;

	num_read = fread(buf, 1, num_read, (FILE *) p);
	if (num_written)
		*num_written = num_read;

	return num_read;
}

static void FileClose(void *p)
{
	fclose((FILE *) p);
}


static int FileGetPos(void *p)
{
	return ftell((FILE *) p);
}


static int FileSetPos(void *p, int offset, int origin)
{
	switch (origin) {
	case SSB_FILE_POS_BEGIN:
		origin = SEEK_SET;
		break;

	case SSB_FILE_POS_CURRENT:
		origin = SEEK_CUR;
		break;

	case SSB_FILE_POS_END:
		origin = SEEK_END;
		break;

	default:
		return -1;
	}

	return fseek((FILE *) p, offset, origin);
}
*/

////-----------------------------------------------

// Look! It is supporting one file open only.
//static HANDLE  hFile, hFileMap;
/*
static void *MapFileOpen(LPCTSTR filename)
{
	DWORD   dwMaximumSizeHigh;
	DWORD   dwMaximumSizeLow;

	MMAP_STRM_PTR  *strm_ptr;


	hFile = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFile == INVALID_HANDLE_VALUE) {
		printf("[Error] %s file not found\n", filename);
		return NULL;
	}
	dwMaximumSizeLow = GetFileSize(hFile, &dwMaximumSizeHigh);
	hFileMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, dwMaximumSizeHigh, dwMaximumSizeLow, NULL);
	if (hFileMap == INVALID_HANDLE_VALUE) {
		printf("[Error] %s file not found\n", filename);
		return NULL;
	}

	strm_ptr = (MMAP_STRM_PTR *) malloc(sizeof(MMAP_STRM_PTR));

	strm_ptr->p_start = (unsigned char *) MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, dwMaximumSizeLow);
	strm_ptr->p_cur   = strm_ptr->p_start;
	strm_ptr->p_end   = strm_ptr->p_start  +  dwMaximumSizeLow;

	return (void *) strm_ptr;
}

static void MapFileRewind(void *p)
{
	MMAP_STRM_PTR  *strm_ptr;

	strm_ptr = (MMAP_STRM_PTR *) p;

	strm_ptr->p_cur = strm_ptr->p_start;
}
*/
static int MapFileRead(void *p, unsigned char *buf, unsigned int num_read, unsigned int *num_written)
{
	MMAP_STRM_PTR  *strm_ptr;

	strm_ptr = (MMAP_STRM_PTR *) p;

	if (strm_ptr->p_end == strm_ptr->p_cur)
		return SSB_FILE_EOF;

	if ((int) num_read > (int) (strm_ptr->p_end - strm_ptr->p_cur))
		num_read = (unsigned int) (strm_ptr->p_end - strm_ptr->p_cur);

	memcpy(buf, strm_ptr->p_cur, num_read);

	strm_ptr->p_cur += num_read;

	if (num_written)
		*num_written = num_read;

	return num_read;
}

/*
static void MapFileClose(void *p)
{
	MMAP_STRM_PTR  *strm_ptr;

	strm_ptr = (MMAP_STRM_PTR *) p;

	UnmapViewOfFile(strm_ptr->p_start);
	CloseHandle(hFileMap);
	CloseHandle(hFile);

	free(p);
}

static int MapFileGetPos(void *p)
{
	MMAP_STRM_PTR  *strm_ptr;

	strm_ptr = (MMAP_STRM_PTR *) p;

	return (strm_ptr->p_cur - strm_ptr->p_start);
}

static int MapFileSetPos(void *p, int offset, int origin)
{
	MMAP_STRM_PTR  *strm_ptr;
	unsigned char  *p_cur_new;

	strm_ptr = (MMAP_STRM_PTR *) p;

	switch (origin) {
	case SSB_FILE_POS_BEGIN:
		if (offset < 0)
			return -1;
		p_cur_new = strm_ptr->p_start + offset;
		break;

	case SSB_FILE_POS_CURRENT:
		p_cur_new = strm_ptr->p_cur + offset;
		if (((int) p_cur_new - (int) strm_ptr->p_start) < 0)
			return -1;
		if (((int) p_cur_new - (int) strm_ptr->p_end) > 0)
			return -1;
		break;

	case SSB_FILE_POS_END:
		if (offset > 0)
			return -1;
		p_cur_new = strm_ptr->p_end + offset;
		break;

	default:
		return -1;
	}

	strm_ptr->p_cur = p_cur_new;

	return ((int) strm_ptr->p_cur - (int) strm_ptr->p_start);
}
*/
////----------------------------------------------------------------------------------------------------


// Function name: SSB_FILE_OPEN
//
// Remark: It opens a file for reading only.
//         According to the definition, it opens a file with fopen or memory-mapped file.
// Parameters:
//      filename[IN] : filename to open
// Return value:
//      void pointer (context pointer)

/*
void *SSB_FILE_OPEN(LPCTSTR filename)
{
#if (DATAREAD_TYPE == DATA_FILE)
	return FileOpen(filename);
#else
	return MapFileOpen(filename);
#endif
}
*/

// Function name: SSB_FILE_REWIND
//
// Remark: It moves the current file position to the beginning.
// Parameters:
//      p[IN] : context pointer
// Return value:
//      void

/*
void SSB_FILE_REWIND(void *p)
{
#if (DATAREAD_TYPE == DATA_FILE)
	FileRewind(p);
#else
	MapFileRewind(p);
#endif
}
*/


// Function name: SSB_FILE_READ
//
// Remark: It reads the data and copies them into 'buf'.
//         The file position will be moved by the amount of 'num_written'
// Parameters:
//      p          [IN]  : context pointer
//      buf        [IN]  : memory buffer to be filled with data.
//      num_read   [IN]  : number of data to read
//      num_written[OUT] : number of data to be read (it can be NULL.)
// Return value:
//      num_written in the buffer or error code
int SSB_FILE_READ(void *p, unsigned char *buf, unsigned int num_read, unsigned int *num_written)
{
	int  ret;

	ret = MapFileRead(p, buf, num_read, num_written);

	return ret;
}


// Function name: SSB_FILE_CLOSE
//
// Remark: It closes the current file.
// Parameters:
//      p[IN] : context pointer
// Return value:
//      void
/*
void SSB_FILE_CLOSE(void *p)
{
#if (DATAREAD_TYPE == DATA_FILE)
	FileClose(p);
#else
	MapFileClose(p);
#endif
}
*/

/*
int   SSB_FILE_GET_POS(void *p)
{
#if (DATAREAD_TYPE == DATA_FILE)
	return FileGetPos(p);
#else
	return MapFileGetPos(p);
#endif
}

int   SSB_FILE_SET_POS(void *p, int offset, int origin)
{
#if (DATAREAD_TYPE == DATA_FILE)
	return FileSetPos(p, offset, origin);
#else
	return MapFileSetPos(p, offset, origin);
#endif
}
*/

