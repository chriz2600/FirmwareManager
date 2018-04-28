/*
  6PACK - file compressor using FastLZ (lightning-fast compression library)

  Copyright (C) 2007 Ariya Hidayat (ariya@kde.org)

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fastlz.h"
#include "firmware-utils.h"

size_t bytes_in_result = 0;
unsigned char *buffer = NULL;
unsigned char *result = NULL;
unsigned char *result_start = NULL;
FILE* in;
FILE* out;

void initBuffer(uint8_t *buffer, int len) {
    for (int i = 0 ; i < len ; i++) { 
        buffer[i] = 0xff; 
    }
}

int simulateEvent() {
    int bytes_write = 0;
    int chunk_size = 0;

    if (bytes_in_result == 0) {
        // reset the result buffer pointer
        result = result_start;
        // step 1: read chunk header
        if (fread(buffer, 1, 2, in) == 0) {
            // no more chunks
            return -1;
        }
        chunk_size = buffer[0] + (buffer[1] << 8);
        // step 2: read chunk length from file
        fread(buffer, 1, chunk_size, in);
        // step 3: decompress
        bytes_in_result = fastlz_decompress(buffer, chunk_size, result, BLOCK_SIZE);
        // step 4: reverseBitOrder
        reverseBitOrder(result, bytes_in_result);
        return 0; /* no bytes written yet */
    }
    
    bytes_write = bytes_in_result > 256 ? 256 : bytes_in_result;
    //fwrite(result, 1, 256, out);
    fwrite(result, 1, bytes_write, out);
    // cleanup buffer afterwards
    initBuffer(result, 256);
    bytes_in_result -= bytes_write;
    result += bytes_write; /* advance the char* by written bytes */
    return bytes_write; /* 256 bytes written */
}

int main(int argc, char** argv) {
    char* input_file = 0;
    char* output_file = 0;
    unsigned int i = 0;
    unsigned long fsize = 0;
    unsigned char chunk_header[4];
    unsigned long total_read = 0;
    unsigned long total_uncompressed = 0;
    unsigned long file_size = 0;
    int bytes_written = 0;

    for(i = 1; i <= argc; i++) {
        char* argument = argv[i];
        if(!input_file) {
            input_file = argument;
            continue;
        }
        if (!output_file) {
            output_file = argument;
            continue;
        }
    }

    in = fopen(input_file, "rb");
    if (!in) {
        printf("Error: could not open %s\n", input_file);
        return -1;
    }

    out = fopen(output_file, "wb");
    if(!out) {
        printf("Error: could not create %s. Aborted.\n\n", output_file);
        return -1;
    }

    /* find size of the file */
    fseek(in, 0, SEEK_END);
    fsize = ftell(in);
    fseek(in, 0, SEEK_SET);

    /* initialize memory */
    buffer = malloc(BLOCK_SIZE);
    result = malloc(BLOCK_SIZE);
    result_start = result;
    initBuffer(result, BLOCK_SIZE);

    fread(buffer, 1, 4, in);
    file_size = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);

    // unpack it!
    for(;;) {
        bytes_written = simulateEvent();
        if (bytes_written == -1) {
            break;
        }
        total_uncompressed += bytes_written;
    }

    free(buffer);
    free(result);

    fclose(in);
    fclose(out);

    printf("%s: %lu / %lu / %lu\n", input_file, file_size, total_uncompressed, fsize);
    return 0;
}
