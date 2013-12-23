#ifndef __SAMSUNG_SYSLSI_APDEV_MFCLIB_SSBSIPLOGMSG_H__
#define __SAMSUNG_SYSLSI_APDEV_MFCLIB_SSBSIPLOGMSG_H__


#define MODULE_NAME	"MFC_APP"

typedef enum
{
	LOG_TRACE   = 0,
	LOG_WARNING = 1,
	LOG_ERROR   = 2
} LOG_LEVEL;

void LOG_MSG(LOG_LEVEL level, const char *func_name, const char *msg, ...);


#endif /* __SAMSUNG_SYSLSI_APDEV_MFCLIB_SSBSIPLOGMSG_H__ */

