/**
 * This is a tool for extracting missed JPEGs from a disk image.
 * AUTHOR Quaker Chung
**/

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>

typedef union {
    unsigned int ii;
    unsigned char cc[4];
} SSSS;

static FILE * gfp = NULL;
static unsigned long long gStart = 0;
static unsigned long long gEnd = 0;
static unsigned long long byteCount = 0;
static unsigned long long gByteCount = 0;
static unsigned int gc = 0;

#define DEBUG 0
#define MB(x) x * 1000000LL
#define MAX_FILE_SIZE MB(20) // 20mb
#define FILE_SIZE_FILTER MB(1)

static bool isStandalone(int c) {
    if (c >= 0xD0 && c <= 0xD9)
        return true;
    else if (c == 0x01)
        return true;
    else
        return false;
}

unsigned char getNext() {
    int c = fgetc(gfp);
    if (c == EOF) {
        fclose(gfp);
        gfp = NULL;
        exit(0);
        return 0;
    }
    gc = 0xFF & c;
    
    byteCount++;
    gByteCount++;
    if (DEBUG > 1) printf("c = %02X\n", gc);
    return gc;
}

static unsigned int readSSSS() {
    SSSS ssss;
    unsigned char c = getNext();
    ssss.cc[0] = getNext();
    ssss.cc[1] = c;
    ssss.cc[2] = 0;
    ssss.cc[3] = 0;
    return ssss.ii;
}

static void skip(unsigned int datasize) {
    int i;
    for (i = 0; i < datasize; i++) {
        getNext();
    }
    return;
}

static bool skipHeader() {
    unsigned int datasize = readSSSS();
    if (datasize < 4) {
        // Not a jpeg
        if (DEBUG) printf("Not a jpeg 2\n");
        return false;
    }
    if (DEBUG) printf("Mark size %u\n", datasize);
    skip(datasize - 2);
    return true;
}

static bool skipToEnd() {
    if (DEBUG) printf("Skip to end\n");
    do {
        getNext();
        if (gc == 0xFF) {
            getNext();
            if (gc == 0xD9)
                return true;
        }
    } while (byteCount <= MAX_FILE_SIZE);  // 20MB
    return false;
}

static bool scanJpeg() {
    gStart = gByteCount - 2;
    byteCount = 2;
    do {
        // Check marks
        getNext();
        if (gc != 0xFF)
            goto notajpeg;
        getNext();
        if (isStandalone(gc)) {
            if (gc == 0xD8) {
                break;
            } else if (gc == 0xD9) {
                // jpeg end
                goto hasjpeg;
            } else {
                continue;
            }
        } else if (gc == 0xDA) {
            // Start of Scan
            if (DEBUG) printf("SOS begin\n");
            if (skipToEnd()) {
                if (DEBUG) printf("JPEG end\n");
                goto hasjpeg;
            } else {
                break;
            }
        } else {
            if (!skipHeader())
                break;
        }
    } while (byteCount <= MAX_FILE_SIZE);
    goto notajpeg;

hasjpeg:
    if (byteCount < 200)
        goto notajpeg;
    // Filter
    if (byteCount < FILE_SIZE_FILTER)
        goto notajpeg;
    gEnd = gByteCount;
    unsigned int B = byteCount & 0x3FF;
    unsigned int K = 0x3FF & (byteCount >> 10);
    unsigned int M = byteCount >> 20;
    printf("Start %016llX End %016llX diff %llu\n", gStart, gEnd,  gEnd - gStart);
    printf("byteCount = %llu (%uM %uK %uBytes)\n", byteCount, M, K, B);
    return true;

notajpeg:
    if (DEBUG) printf("Corrupted JPEG\n");
    return false;
}

static bool scan() {
    bool ret = false;
    bool begin = false;
    puts("");
    do {
        getNext();
        if (gc == 0xFF) {
            getNext();
            if (gc == 0xD8)
                begin = true;
        }
        if (begin) {
            ret = scanJpeg();
            if (ret) {
                puts("");
                byteCount = 0;
            }
            begin = false;
        }
        if (byteCount > MB(2)) {
            byteCount = 0;
            printf("# %04lluG ", 0xFFFF & (gByteCount >> 30));
            printf("%03lluM\r", 0x3FF & (gByteCount >> 20));
        }
        // Keep scan
    } while(1);
}

int main(int argc, char ** argv) {
    if (argc != 2)
        return -1;
    gfp = fopen(argv[1], "r");
    if (gfp == NULL)
        return -1;

    scan();

end:
    if (gfp != NULL)
        fclose(gfp);
    return 0;
}

