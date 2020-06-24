/*
 * Simple trace backend
 *
 * Copyright IBM, Corp. 2010
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

#ifndef TRACE_SIMPLE_H
#define TRACE_SIMPLE_H

// START: Kaifeng
#include <zlib.h>
#define CHUNK 0x4000
// END: Kaifeng

void st_print_trace_file_status(void);
bool st_set_trace_file_enabled(bool enable);
void st_set_trace_file(const char *file);
bool st_init(void);
void st_flush_trace_buffer(void);

typedef struct {
    unsigned int tbuf_idx;
    unsigned int rec_off;
} TraceBufferRecord;

// START: Kaifeng
/* The following macro calls a zlib routine and checks the return
   value. If the return value ("status") is not OK, it prints an error
   message and exits the program. Zlib's error statuses are all less
   than zero. */
#define CALL_ZLIB(x) {                                                  \
            int status;                                                 \
            status = x;                                                 \
            if (status < 0) {                                           \
                fprintf (stderr,                                        \
                         "%s:%d: %s returned a bad status of %d.\n",    \
                         __FILE__, __LINE__, #x, status);               \
                exit (EXIT_FAILURE);                                    \
            }                                                           \
        }

/* These are parameters to deflateInit2. See
   http://zlib.net/manual.html for the exact meanings. */
#define windowBits 15
#define GZIP_ENCODING 16

// Add compressed simple backend
typedef struct {
    z_stream gz_stream;
    int is_compressed;
    unsigned char out[CHUNK];
    unsigned char message[CHUNK];
} CompressedTraceBuffer;

/* Private global variable, don't use */
extern CompressedTraceBuffer *compressed_trace_buffer;

/* Clean up/flush Compressed Trace Buffer buffers on exit */
void compressed_simple_trace_buffer_cleanup(void);
// END: Kaifeng

/* Note for hackers: Make sure MAX_TRACE_LEN < sizeof(uint32_t) */
#define MAX_TRACE_STRLEN 512
/**
 * Initialize a trace record and claim space for it in the buffer
 *
 * @arglen  number of bytes required for arguments
 */
int trace_record_start(TraceBufferRecord *rec, uint32_t id, size_t arglen);

/**
 * Append a 64-bit argument to a trace record
 */
void trace_record_write_u64(TraceBufferRecord *rec, uint64_t val);

/**
 * Append a string argument to a trace record
 */
void trace_record_write_str(TraceBufferRecord *rec, const char *s, uint32_t slen);

/**
 * Mark a trace record completed
 *
 * Don't append any more arguments to the trace record after calling this.
 */
void trace_record_finish(TraceBufferRecord *rec);

#endif /* TRACE_SIMPLE_H */
