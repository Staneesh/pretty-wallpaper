#include "utils.h"

#pragma pack(push, 1)
struct BitmapHeader
{
    unsigned short fileType;
    unsigned fileSize;
    unsigned short reserved1;
    unsigned short reserved2;
    unsigned bitmapOffset;
    unsigned size;
    int width;
    int height;
    unsigned short planes;
    unsigned short bitsPerPixel;
    unsigned compression;
    unsigned sizeOfBitmap;
    int horzResolution;
    int vertResolution;
    unsigned colorsUsed;
    unsigned colorsImportant;
};
#pragma pack(pop)

void writeImage(unsigned width, unsigned height, const unsigned* pixels, const char* filename)
{
    unsigned outputPixelsSize = sizeof(unsigned) * width * height;
    struct BitmapHeader header = {};
    header.fileType = 0x4D42;
    header.fileSize = sizeof(header) + outputPixelsSize;
    header.bitmapOffset = sizeof(header);
    header.size = sizeof(header) - 14;
    header.width = width;
    header.height = height;
    header.planes = 1;
    header.bitsPerPixel = 32;
    header.compression = 0;
    header.sizeOfBitmap = outputPixelsSize;
    header.horzResolution = 0;
    header.vertResolution = 0;
    header.colorsUsed = 0;
    header.colorsImportant = 0;

    FILE* outFile = fopen(filename, "wb");
    if (outFile)
    {
        fwrite(&header, sizeof(header), 1, outFile);
        fwrite(pixels, outputPixelsSize, 1, outFile);
        fclose(outFile);
    }
    else
    {
        printf("[ERROR]: Unable to write output file %s!\n", filename);
    }
}
