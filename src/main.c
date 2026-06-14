#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define FLAG_DEBUG 1

uint8_t flags;

bool Compress(char* buf, size_t bufLen, const char* filePath)
{
    if (!buf || !bufLen || !filePath) return false;

    size_t filePathLen = strlen(filePath);

    char* compressedBuffer = calloc(bufLen, 2); /*at least of size len * 2. On the worst case it will be like this.*/;
    if (!compressedBuffer) {printf("Failed to allocate %d bytes\n", bufLen); return false;}

    /*Compress and write to new buffer*/
    /*
    Procedure: Read byte by byte, if there is a sequence of the same byte, save the amount and write it in a [value][number] format in the compresse file.
    If the sequence is bigger than 256, then simply write another pair of [value][number] with the same value and the remaining number.
    Do this even if number is 1.
    */

    uint8_t amount = 0; /*0 cannot exist, treat 0 as 1 so we can have up to 256 value long sequences*/
    char value;
    size_t compressedBufferLength = 0;
    for (size_t i = 0; i < bufLen+1; i++)
    {
        if (i == bufLen)
        {
            value = buf[i - 1];
            compressedBuffer[compressedBufferLength++] = value;
            if (bufLen != 1) {if (value == buf[i-2]) amount++;}
            compressedBuffer[compressedBufferLength++] = amount;
            break;
        }
        if (i == 0) {value = buf[i]; continue;}
        if (value == buf[i])
        {
            if (amount + 1 == 256)
            {
                /*Write out to the compressed buffer*/
                compressedBuffer[compressedBufferLength++] = value;
                compressedBuffer[compressedBufferLength++] = amount;
            }

            amount++; /*Also resets to zero if overflows*/
            continue;
        }

        compressedBuffer[compressedBufferLength++] = value;
        compressedBuffer[compressedBufferLength++] = amount;
        amount = 0;
        value = buf[i];
    }

    char newFilePath[filePathLen + 1 + 4];
    strncpy(newFilePath, filePath, filePathLen);
    strncpy(newFilePath + filePathLen, ".cmp", 4);
    newFilePath[filePathLen + 1 + 3] = 0;

    FILE* file = fopen(newFilePath, "wb");
    if (!file) {printf("Something went wrong when opening the file to write, at path: %s\n", newFilePath); free(compressedBuffer); return false;}

    size_t result = fwrite(compressedBuffer, 1, compressedBufferLength, file);
    if (result < compressedBufferLength) {printf("Something went wrong when writing the compressed data to the file\n"); fclose(file); free(compressedBuffer); return false;}

    if (flags & FLAG_DEBUG) printf("DEBUG - Compressed file size: %zu\n", result);

    fclose(file);
    free(compressedBuffer);

    return true;
}

bool Extract(char* buf, size_t bufLen, const char* filePath)
{
    if (!buf || !bufLen || !filePath) return false;

    size_t filePathLen = strlen(filePath);

    char* extractedBuffer = malloc(bufLen);
    if (!extractedBuffer)
    {
        printf("Failed to allocate extracted buffer\n");
        return false;
    }

    size_t allocationSize = bufLen;
    size_t currentExtractedSize = 0;
    for (size_t i = 0; i < bufLen; i+=2)
    {
        if (i == bufLen) break;;
        char value = buf[i];
        char count = buf[i+1];

        if (currentExtractedSize + count + 1 > allocationSize)
        {
            char* newBuf = realloc(extractedBuffer, allocationSize * 2);
            if (!newBuf)
            {
                free(extractedBuffer);
                printf("Failed to reallocate extracted buffer\n");
                return false;
            }

            extractedBuffer = newBuf;
            allocationSize *= 2;
        }

        memset(extractedBuffer + currentExtractedSize, value, count + 1);
        currentExtractedSize += count + 1 /*Count is stored from 0 to 255, where 0 means 1 so everything is upped by 1.*/;
    }

    char newFilePath[filePathLen];
    strncpy(newFilePath, filePath, filePathLen - 4);
    newFilePath[filePathLen - 4] = 0;

    FILE* file = fopen(newFilePath, "wb");
    if (!file)
    {
        printf("Could not open or create the file at path: %s\n", newFilePath);
        free(extractedBuffer);
        return false;
    }

    size_t result = fwrite(extractedBuffer, 1, currentExtractedSize, file);
    if (result < currentExtractedSize)
    {
        printf("Something went wrong when writing to the uncompressed file\n");
        fclose(file);
        free(extractedBuffer);
        return false;
    }

    fclose(file);
    free(extractedBuffer);

    return true;
}

void PrintHelpMenu()
{
    printf(
    "HELP MENU\n"
    "Command list:\n"
        "\t-d DEBUG INFO\n"
        "\t-e EXTRACT/DECOMPRESS\n");
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("Wrong usage: compress <filepath> -<flag>... \n");
        return 1;
    }

    /*Fetch flags*/
    for (int i = 2; i < argc; i++)
    {
        if (argv[i][0] != '-' || strlen(argv[i]) == 1)
        {printf("%s is not a recognized argument\n", argv[i]); continue; /*Not a flag*/}

        switch (argv[i][1])
        {
        case 'd': flags |= FLAG_DEBUG; break;
        case 'h': PrintHelpMenu(); return 0;
        default: printf("Flag %c not recognized\n", argv[i][1]); break;
        }
    }

    FILE* file = fopen(argv[1], "rb");
    if (!file) {printf("Failed to open file at path: %s\n", argv[1]); return 1;}

    fseek(file, 0, SEEK_END);
    size_t len = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (flags & FLAG_DEBUG) printf("DEBUG - Chosen file size: %zu\n", len);

    char* ogBuffer = malloc(len);
    if (!ogBuffer)
    {printf("Failed to allocate %d bytes\n", len); fclose(file); return 1;}
    {
        size_t result = fread(ogBuffer, 1, len, file);
        fclose(file);
        if (result < len) {printf("Something went wrong when reading the file.\n"); if (flags & FLAG_DEBUG) printf("DEBUG - bytes read: %zu\n", result); free(ogBuffer); return 1;}
    }

    size_t filePathLen = strlen(argv[1]);

    if (strcmp(argv[1] + filePathLen - 4, ".cmp") != 0)
    {
        Compress(ogBuffer, len, argv[1]);
    }

    else
    {
        Extract(ogBuffer, len, argv[1]);
    }

    free(ogBuffer);

    return 0;
}