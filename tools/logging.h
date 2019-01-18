#ifndef LOGGING_H_
#define LOGGING_H_


#include <string>
#include <cstdio>
#include <sys/stat.h>

using namespace std;

class Logging {
private:
	string logging_filename;
	FILE *logfile;
public:
	Logging() : logging_filename("/tmp/gramfs.log") {}
	Logging(const char * path) : logging_filename(path) {}
	Logging(const string &path) : logging_filename(path) {}

	void Open();
	void LogMsg(const char *format, ...);
	virtual ~Logging();
}

#endif