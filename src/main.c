#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <raylib/raylib.h>

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
        if (i == bufLen - 1) break;
        char value = buf[i];
        uint8_t count = buf[i+1];

        if (currentExtractedSize + count + 1 > allocationSize)
        {
            char* newBuf = realloc(extractedBuffer, (allocationSize + count + 1) * 2);
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

bool WindowProcedure()
{
    InitWindow(100, 100, "Compressor");

    int monitor = GetCurrentMonitor();

    int screenWidth = GetMonitorWidth(monitor) / 2;
    int screenHeight = GetMonitorHeight(monitor) / 2;

    SetWindowSize(screenWidth, screenHeight);

    int windowX = (GetMonitorWidth(monitor) - screenWidth) / 2;
    int windowY = (GetMonitorHeight(monitor) - screenHeight) / 2;
    SetWindowPosition(windowX, windowY);

    SetTargetFPS(60);

    Font font = LoadFont("../resources/font.otf");
    if (!IsFontValid(font)) {font = GetFontDefault();}

    Vector2 textLength1 = MeasureTextEx(font, "Drag & Drop", 32, 0.1f);
    Vector2 textLength2 = MeasureTextEx(font, "your files here", 32, 0.1f);

    while (!WindowShouldClose())
    {
        if (IsFileDropped())
        {
            FilePathList droppedFiles = LoadDroppedFiles();

            for (int i = 0; i < droppedFiles.count; i++)
            {
                const char* currentFilePath = droppedFiles.paths[i];

                FILE* file = fopen(currentFilePath, "rb");
                if (!file) {printf("Failed to open file at path: %s\n", currentFilePath); continue;}

                fseek(file, 0, SEEK_END);
                size_t len = ftell(file);
                fseek(file, 0, SEEK_SET);

                if (flags & FLAG_DEBUG) printf("DEBUG - Chosen file size: %zu\n", len);

                char* ogBuffer = malloc(len);
                if (!ogBuffer)
                {printf("Failed to allocate %d bytes\n", len); fclose(file); continue;}
                {
                    size_t result = fread(ogBuffer, 1, len, file);
                    fclose(file);
                    if (result < len) {printf("Something went wrong when reading the file.\n"); if (flags & FLAG_DEBUG) printf("DEBUG - bytes read: %zu\n", result); free(ogBuffer); continue;}
                }

                size_t filePathLen = strlen(currentFilePath);
                if (filePathLen < 4) {printf("File path length is too short %s", currentFilePath); free(ogBuffer); return false;}
                if (strcmp(currentFilePath + strlen(currentFilePath) - 4, ".cmp") != 0)
                {
                    if (Compress(ogBuffer, len, currentFilePath)) printf("Successfully compressed file %s\n", currentFilePath);
                }

                else
                {
                    if (Extract(ogBuffer, len, currentFilePath)) printf("Successfully extracted file %s\n", currentFilePath);
                }

                free(ogBuffer);
            }

            UnloadDroppedFiles(droppedFiles);
        }

        BeginDrawing();

        ClearBackground(DARKGRAY);

        //DrawTextEx(Font font, const char *text, Vector2 position, float fontSize, float spacing, Color tint)
        DrawTextEx(font, "Drag & Drop", (Vector2){screenWidth / 2 - textLength1.x / 2, screenHeight / 2 - textLength1.y / 2 - 16}, 32, 0.1f, BLACK);
        DrawTextEx(font, "your files here", (Vector2){screenWidth / 2 - textLength2.x / 2, screenHeight / 2 - textLength2.y / 2 + 16}, 32, 0.1f, BLACK);

        EndDrawing();
    }

    UnloadFont(font);
    CloseWindow();
    return true;
}

bool ConsoleProcedure(int argc, const char** argv)
{
    if (!argv || !(*argv) || argc < 2) return false;

    /*Fetch flags*/
    for (int i = 2; i < argc; i++)
    {
        if (argv[i][0] != '-' || strlen(argv[i]) == 1)
        {printf("%s is not a recognized argument\n", argv[i]); continue; /*Not a flag*/}

        switch (argv[i][1])
        {
        case 'd': flags |= FLAG_DEBUG; break;
        case 'h': PrintHelpMenu(); return true;
        default: printf("Flag %c not recognized\n", argv[i][1]); break;
        }
    }

    FILE* file = fopen(argv[1], "rb");
    if (!file) {printf("Failed to open file at path: %s\n", argv[1]); return false;}

    fseek(file, 0, SEEK_END);
    size_t len = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (flags & FLAG_DEBUG) printf("DEBUG - Chosen file size: %zu\n", len);

    char* ogBuffer = malloc(len);
    if (!ogBuffer)
    {printf("Failed to allocate %d bytes\n", len); fclose(file); return false;}
    {
        size_t result = fread(ogBuffer, 1, len, file);
        fclose(file);
        if (result < len) {printf("Something went wrong when reading the file.\n"); if (flags & FLAG_DEBUG) printf("DEBUG - bytes read: %zu\n", result); free(ogBuffer); return false;}
    }

    size_t filePathLen = strlen(argv[1]);
    if (filePathLen < 4) {printf("File path length is too short %s", argv[1]); free(ogBuffer); return false;}

    if (strcmp(argv[1] + filePathLen - 4, ".cmp") != 0)
    {
        Compress(ogBuffer, len, argv[1]);
    }

    else
    {
        Extract(ogBuffer, len, argv[1]);
    }

    free(ogBuffer);

    return true;
}

int main(int argc, char** argv)
{
    bool result = argc == 1 ? WindowProcedure() : ConsoleProcedure(argc, (const char**)argv);
    return result;
}