#include "logging.h"

Logging::~Logging() 
{
	if (logfile != NULL)
	{
		fflush(logfile);
		fclose(logfile);
	}
}

void Logging::Open()
{
	if(logging_filename.size() > 0)
	{
		logfile = fopen(logging_filename.data(), "w");
	} else {
		logfile = NULL;
	}
}

void Logging::LogMsg(const char *format, ...)
{
	if (logfile != NULL)
	{
		va_list ap;
		va_start(ap, format);
		vfprintf(logfile, format, ap);
		fflush(logfile);
	}
}