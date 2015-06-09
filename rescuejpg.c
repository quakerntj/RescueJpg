/**
 * This is a tool for extracting missed JPEGs from a disk image.
 * AUTHOR Quaker Chung
**/

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

typedef union {
    unsigned int ii;
    unsigned char cc[4];
} SSSS;

static FILE * gfp = NULL;
static unsigned long long gStart = 0;
static unsigned long long gEnd = 0;
static unsigned long long byteCount = 0;
static unsigned long long gByteCount = 0;  // Image file bytes
static unsigned int gc = 0;
static char * gStorageFolder = NULL;
static unsigned int gFileIndex = 0;  // max is 99999
static char gJpgPrefix[] = "RESCUE";
static char gJpgSuffix[] = ".JPG";
static bool gDoExtract = false;


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

static char * getNextFileName(char * filename, unsigned int index) {
    sprintf(filename, "/%s%05u%s", gJpgPrefix, index, gJpgSuffix);
    return filename;
}

static void doExtract(unsigned long long _size) {
    // Reduce type scale.  We sure the size is enough.
    long size = (long) _size;

    // Rewind
    fseek(gfp, -size, SEEK_CUR);
    
    // Open new file
    char jpeg[PATH_MAX];
    char filename[256] = {0};
    strncpy(jpeg, gStorageFolder, PATH_MAX);
    strncat(jpeg, getNextFileName(filename, gFileIndex), PATH_MAX);
    FILE * fp = fopen(jpeg, "w");
    if (fp == NULL)
        return;
    gFileIndex++;

    // Read and Write
    unsigned char buffer[4096];
    long i;
    for (i = size; i > 0; i -= 4096) {
        long sizeprocess = 4096;
        if (i <= 4096)
            sizeprocess = i;
        fread(buffer, sizeprocess, 1, gfp);
        fwrite(buffer, sizeprocess, 1, fp);
    }
    fclose(fp);
    fp = NULL;
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

    if (gDoExtract)
        doExtract(byteCount);
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

void seek(unsigned long long target) {
    unsigned long seektargetindex = (unsigned long) (target >> 32);
    unsigned long i = 0;
    //fprintf(stderr, "seektargetindex = %lu", seektargetindex);
    for (;i < seektargetindex; i++) {
        fseek(gfp, 0x7FFFFFFFL, SEEK_CUR);
        fseek(gfp, 0x7FFFFFFFL, SEEK_CUR);
        fseek(gfp, 0x2L, SEEK_CUR);
    }
    unsigned long seektarget = (unsigned long) (target & 0xFFFFFFFFLLU);
    fseek(gfp, seektarget, SEEK_CUR);
}

int main(int argc, char ** argv) {
    unsigned long long target = 0;
    if (argc == 4) {
        target = strtoll(argv[3], NULL, 16);
        printf("# Seek to 0x%llx\n", target);
        printf("# Extracted JPGs will be saved to \"%s\"\n", argv[2]);
        gStorageFolder = strdup(argv[2]);
        gDoExtract = true;
    } if (argc == 3) {
        // storage folder is argv[2]
        printf("# Extracted JPGs will be saved to \"%s\"\n", argv[2]);
        gStorageFolder = strdup(argv[2]);
        gDoExtract = true;
    } else if (argc == 2) {
        // Only show address.  No extraction.
        puts("# Only show address.  No Extraction.\n");
    } else {
        puts("# rescue jpg from image file\n"
        "# rescuejpg <imagefile>   # see only jpg address in <imagefile>\n"
        "# rescuejpg <imagefile> <folder>   # save extracted jpg to <folder>\n"
        "# rescuejpg <imagefile> <folder> <address>  # start read image from <address>\n"
        "# Author: QuakerNTj\n"
        "# Site: https://github.com/quakerntj\n");
        return -1;
    }

    gfp = fopen(argv[1], "r");
    if (gfp == NULL)
        return -1;

    if (target != 0)
        seek(target);

    scan();

end:
    if (gfp != NULL) {
        fclose(gfp);
        gfp = NULL;
    }
    if (gDoExtract) {
        free(gStorageFolder);
        gStorageFolder = NULL;
    }
    return 0;
}

