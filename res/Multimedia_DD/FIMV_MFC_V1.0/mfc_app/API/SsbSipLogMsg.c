#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "SsbSipLogMsg.h"


static const LOG_LEVEL log_level = LOG_TRACE;
static const char *modulename = MODULE_NAME;
static const char *level_str[] = {"TRACE", "WARNING", "ERROR"};

void LOG_MSG(LOG_LEVEL level, const char *func_name, const char *msg, ...)
{
	
	char buf[256];
	va_list argptr;

	if (level < log_level)
		return;

	sprintf(buf, "[%s: %s] %s: ", modulename, level_str[level], func_name);

	va_start(argptr, msg);
	vsprintf(buf + strlen(buf), msg, argptr);
	printf(buf);
	va_end(argptr);
}
