#ifndef __tealtree__pipestream__
#define __tealtree__pipestream__

#include <errno.h>
#include <memory>
#include <stdio.h>
#include <stdint.h>
#include <string>

#include "util.h"

#define BUFFER_MAX_SIZE 65536

class LineReader
{
protected:
    FILE *fp;
    char* buffer;
    size_t buffer_ptr, buffer_size;
    bool buffer_done;
public:
    LineReader(FILE *fp);
    std::unique_ptr<std::string> next_line();
    virtual ~LineReader();
private:
    std::unique_ptr<std::string> next_line_internal();
};

class StdInReader : public LineReader
{
public:
    StdInReader() : LineReader(stdin) {};
    virtual ~StdInReader() {};
};

class PipeReader : public LineReader
{
public:
    PipeReader(const char * cmd_line)
#ifdef _WIN32
        : LineReader(_popen(cmd_line, "r"))
#else
		: LineReader(popen(cmd_line, "r"))
#endif
    {
        int errsv = errno;
//        if (this->fp == NULL) {
        if (errsv != 0) {
            //int errsv = errno;
            throw std::runtime_error(std::string("Calling popen failed: ") + std_strerror(errsv));
        }
    };
    virtual ~PipeReader()
    {
#ifdef _WIN32
        _pclose(this->fp);
#else
        pclose(this->fp);
#endif
    };
};

class FileReader : public LineReader
{
public:
    FileReader(const char * file_name)
        : LineReader(fopen(file_name, "r"))
    {
        int errsv = errno;
        if (errsv != 0) {
            throw std::runtime_error(std::string("Opening file failed: ") + std_strerror(errsv));
        }
    };
    virtual ~FileReader()
    {
        fclose(this->fp);
    };
};


#endif /* defined(__tealtree__pipestream__) */
