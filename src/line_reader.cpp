#include "line_reader.h"

#include <errno.h>
#include <stdlib.h>
#include <string>


LineReader::LineReader(FILE* fp)
{
    this->fp = fp;
    this->buffer = reinterpret_cast<char*>(malloc(BUFFER_MAX_SIZE * sizeof(this->buffer[0])));
    this->buffer_ptr = 0;
    this->buffer_size = 0;
    this->buffer_done = false;
}

std::unique_ptr<std::string> LineReader::next_line()
{
    while(true) {
        std::unique_ptr<std::string> result = this->next_line_internal();
        if (result.get() == NULL) {
            return result;
        }
        size_t result_len = result.get()->length();
        if (result_len == 0) {
            // and try again
        } else {
            return result;
        }
    }
}

std::unique_ptr<std::string> LineReader::next_line_internal()
{
    if (this->buffer_done) {
        return std::unique_ptr<std::string>();
    }
    char* result = NULL;
    size_t result_len = 0;
    while (true) {
        if (this->buffer_ptr >= this->buffer_size) {
            if (fgets(this->buffer, BUFFER_MAX_SIZE, this->fp) == NULL) {
                this->buffer_done = true;
                break;
            } else {
                this->buffer_size = (uint32_t)strlen(this->buffer);
                this->buffer_ptr = 0;
            }
        }
        
        bool found_newline = false;
        size_t ptr2;
        for (ptr2 = this->buffer_ptr; ptr2 < this->buffer_size; ptr2++) {
            if ((this->buffer[ptr2] == '\n') || (this->buffer[ptr2] == '\r')) {
                found_newline = true;
                break;
            }
        }
        size_t chunk_length = ptr2 - this->buffer_ptr;
        if (result_len == 0) {
            result = reinterpret_cast<char*>(malloc((chunk_length + 1) * sizeof(this->buffer[0])));
            result_len = 1;
        } else {
            result = reinterpret_cast<char*>(realloc(result, result_len + chunk_length));
        }
        memcpy(result + result_len - 1, this->buffer + this->buffer_ptr, chunk_length * sizeof(this->buffer[0]));
        result_len += chunk_length;
        this->buffer_ptr = ptr2;
        if (found_newline) {
            this->buffer_ptr ++;
            break;
        }
    }
    
    if (result_len > 0) {
        result[result_len-1] = (char)0;
        std::unique_ptr<std::string> result2(new std::string(result));
        free(result);
        return result2;
    } else {
        return std::unique_ptr<std::string>();
    }
}

LineReader::~LineReader()
{
    free(this->buffer);
}
