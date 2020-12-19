#ifndef _LOG_H_
#define _LOG_H_

void dump_hex(const char * name, const void* vector, size_t length);

void dump_array_part(const char * array, size_t index, const char * name,
                     const uint8_t* vector, size_t length);

#endif /* _LOG_H_ */

