#ifndef _STREAM_BUFFER_H_
#define _STREAM_BUFFER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"
typedef struct
{
    void*  buf;
    uint   len;
    uint   index;
    ulong  counter;
    size_t itemSize;
} StreamBuffer;

StreamBuffer* stream_buffer_create (uint len, size_t itemSize);
int   stream_buffer_delete (StreamBuffer* bs);
int   stream_buffer_insert (StreamBuffer* bs, void * src);
int   stream_buffer_reset (StreamBuffer* bs);
int   stream_buffer_get (StreamBuffer* bs, void *buf, uint* len);
int   stream_buffer_counter_to_index (StreamBuffer* bs, ulong counter);
ulong stream_buffer_index_to_counter (StreamBuffer* bs, int index);

#ifdef __cplusplus
} /* end extern C */
#endif

#endif /* _STREAM_BUFFER_H_ */

