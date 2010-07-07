#include <stdarg.h>
#include <stdio.h>
#include "log.h"

void Log(LOG_LEVEL severity, const char* fmt...)
{
	va_list valist;
	va_start(valist, fmt);
	vprintf(fmt, valist);
	printf("\n");
	fflush(stdout);
	va_end(valist);
}
