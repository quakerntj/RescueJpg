#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char ** argv) {
    FILE * fp = fopen("/media/w1/16G.image", "r");
    if (fp == NULL)
        return -1;

    unsigned long long target = 0x00000003671F0000LLU;
    unsigned long size = 5132454;

    unsigned long seektargetindex = (unsigned long) (target >> 32);
    unsigned long i = 0;
    //fprintf(stderr, "seektargetindex = %lu", seektargetindex);
    for (;i < seektargetindex; i++) {
        fseek(fp, 0x7FFFFFFFL, SEEK_CUR);
        fseek(fp, 0x7FFFFFFFL, SEEK_CUR);
        fseek(fp, 0x2L, SEEK_CUR);
    }
    unsigned long seektarget = (unsigned long) (target & 0xFFFFFFFFLLU);
    fseek(fp, seektarget, SEEK_CUR);
        
    unsigned long j = 0;
    for (j = 0; j < size; j++)
        putc(fgetc(fp), stdout);

    if (fp != NULL)
        fclose(fp);
    return 0;
}

