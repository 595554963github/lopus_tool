#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <opus.h>
#include <intrin.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef int s32;
typedef short s16;
typedef char s8;

#ifdef _WIN32
typedef long long s64;
typedef unsigned long long u64;
#else
typedef long s64;
typedef unsigned long u64;
#endif

#define EXTRACT_BITS(value, pos, width) ( ((value) >> (pos)) & ((1u << (width)) - 1u) )
#define ALIGN_DOWN_4(value) ( (value) & ~(4 - 1) )
#define ALIGN_UP_4(value)   ( ((value) + 4 - 1) & ~(4 - 1) )
#define ALIGN_DOWN_16(value) ( (value) & ~(16 - 1) )
#define ALIGN_UP_16(value)   ( ((value) + 16 - 1) & ~(16 - 1) )
#define ALIGN_DOWN_32(value) ( (value) & ~(32 - 1) )
#define ALIGN_UP_32(value)   ( ((value) + 32 - 1) & ~(32 - 1) )
#define ALIGN_DOWN_64(value) ( (value) & ~(64 - 1) )
#define ALIGN_UP_64(value)   ( ((value) + 64 - 1) & ~(64 - 1) )
#define IDENTIFIER_TO_U32(char1, char2, char3, char4) ( ((u32)(char4) << 24) | ((u32)(char3) << 16) | ((u32)(char2) <<  8) | ((u32)(char1) <<  0) )
#define IDENTIFIER_TO_U16(char1, char2) ( ((u16)(char2) << 8) | ((u16)(char1) << 0) )
#define MAX(a,b) ( (a) > (b) ? a : b )
#define MIN(a,b) ( (a) < (b) ? a : b )

static u32 byteswap32(u32 x) {
    return _byteswap_ulong(x);
}

static void panic(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    printf("\nPANIC: %s\n\n", buffer);
    va_end(args);
    exit(1);
}

static void warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    printf("\nWARN: %s\n\n", buffer);
    va_end(args);
}

typedef struct {
    u8* data_u8;
    u64 size;
} MemoryFile;

static const char* FileBasePath = "";

static MemoryFile MemoryFileCreate(const char* path) {
    MemoryFile hndl = { 0 };

    if (path == NULL) {
        panic("MemoryFileCreate: path is NULL");
        return hndl;
    }

    char fpath[512];
    if (path[0] != '/')
        snprintf(fpath, sizeof(fpath), "%s%s", FileBasePath, path);
    else
        snprintf(fpath, sizeof(fpath), "%s", path);

    FILE* fp = fopen(fpath, "rb");
    if (fp == NULL) {
        panic("MemoryFileCreate: fopen failed (path : %s)", fpath);
        return hndl;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        panic("MemoryFileCreate: fseek failed (path : %s)", fpath);
        return hndl;
    }

    long size = ftell(fp);
    if (size == -1L) {
        fclose(fp);
        panic("MemoryFileCreate: ftell failed (path : %s)", fpath);
        return hndl;
    }

    hndl.size = size;
    rewind(fp);

    hndl.data_u8 = (u8*)malloc((size_t)hndl.size);
    if (hndl.data_u8 == NULL) {
        fclose(fp);
        panic("MemoryFileCreate: malloc failed");
        return hndl;
    }

    size_t bytesCopied = fread(hndl.data_u8, 1, (size_t)hndl.size, fp);
    if (bytesCopied < hndl.size) {
        fclose(fp);
        free(hndl.data_u8);
        hndl.data_u8 = NULL;
        panic("MemoryFileCreate: fread failed (path : %s)", fpath);
        return hndl;
    }

    fclose(fp);
    return hndl;
}

static void MemoryFileDestroy(MemoryFile* file) {
    if (file->data_u8) {
        free(file->data_u8);
        file->data_u8 = NULL;
    }
    file->size = 0;
}

static int MemoryFileWrite(MemoryFile* file, const char* path) {
    if (path == NULL) {
        warn("MemoryFileWrite: path is NULL");
        return 1;
    }
    if (file->size > 0 && file->data_u8 == NULL) {
        warn("MemoryFileWrite: data is NULL");
        return 1;
    }
    char fpath[512];
    if (path[0] != '/')
        snprintf(fpath, sizeof(fpath), "%s%s", FileBasePath, path);
    else
        snprintf(fpath, sizeof(fpath), "%s", path);
    FILE* fp = fopen(fpath, "wb");
    if (fp == NULL) {
        warn("MemoryFileWrite: fopen failed (path : %s)", fpath);
        return 1;
    }
    if (file->size > 0) {
        size_t bytesCopied = fwrite(file->data_u8, 1, (size_t)file->size, fp);
        if (bytesCopied < file->size) {
            fclose(fp);
            warn("MemoryFileWrite: fwrite error (path: %s)", fpath);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

typedef struct {
    void* data;
    u64 elementCount;
    u32 elementSize;
    u64 _capacity;
} ListData;

static void ListInit(ListData* list, u32 elementSize, u64 initialCapacity) {
    if (initialCapacity == 0) initialCapacity = 1;
    list->data = malloc((size_t)(initialCapacity * elementSize));
    if (!list->data) panic("ListInit: malloc fail");
    list->elementSize = elementSize;
    list->elementCount = 0;
    list->_capacity = initialCapacity;
}

static void* ListGet(ListData* list, u64 index) {
    if (index >= list->elementCount) {
        warn("ListGet: index out of bounds");
        return NULL;
    }
    return (u8*)list->data + list->elementSize * (size_t)index;
}

static void ListAdd(ListData* list, void* element) {
    if (list->elementCount == list->_capacity) {
        u64 newCapacity = list->_capacity * 2;
        if (newCapacity == 0) newCapacity = 1;
        void* newData = realloc(list->data, (size_t)(newCapacity * list->elementSize));
        if (!newData) panic("ListAdd: realloc fail");
        list->data = newData;
        list->_capacity = newCapacity;
    }

    if (list->data) {
        u8* dst = (u8*)list->data + list->elementSize * (size_t)list->elementCount;
        memcpy(dst, element, list->elementSize);
        list->elementCount++;
    }
}

static void ListRemove(ListData* list, u64 index) {
    if (index >= list->elementCount) {
        warn("ListRemove: index out of bounds");
        return;
    }
    u8* dst = (u8*)list->data + list->elementSize * (size_t)index;
    u8* src = dst + list->elementSize;
    u64 bytesToMove = (list->elementCount - index - 1) * list->elementSize;
    if (bytesToMove > 0) {
        memmove(dst, src, (size_t)bytesToMove);
    }
    list->elementCount--;
    if (list->elementCount < list->_capacity / 4 && list->_capacity > 4) {
        list->_capacity /= 2;
        void* newData = realloc(list->data, (size_t)(list->_capacity * list->elementSize));
        if (newData) list->data = newData;
    }
}

static void ListAddRange(ListData* list, void* elements, u64 count) {
    if (count == 0 || !elements) return;

    if (list->elementCount + count > list->_capacity) {
        u64 newCapacity = list->_capacity;
        while (list->elementCount + count > newCapacity) {
            newCapacity *= 2;
            if (newCapacity == 0) newCapacity = 1;
        }
        void* newData = realloc(list->data, (size_t)(newCapacity * list->elementSize));
        if (!newData) panic("ListAddRange: realloc fail");
        list->data = newData;
        list->_capacity = newCapacity;
    }

    if (list->data) {
        u8* dst = (u8*)list->data + list->elementSize * (size_t)list->elementCount;
        memcpy(dst, elements, (size_t)(count * list->elementSize));
        list->elementCount += count;
    }
}

static void ListDestroy(ListData* list) {
    if (list->data) {
        free(list->data);
        list->data = NULL;
    }
    list->elementCount = 0;
    list->_capacity = 0;
}

#define CHUNK_HEADER_ID (0x80000001)
#define CHUNK_DATA_ID (0x80000004)
#define OPUS_VERSION (0)
#define OGG_OPUS_ID IDENTIFIER_TO_U32('O','g','g','S')

#pragma pack(push, 1)
typedef struct {
    u32 chunkId;
    u32 chunkSize;
    u8 version;
    u8 channelCount;
    u16 frameSize;
    u32 sampleRate;
    u32 dataOffset;
    u32 _unk14;
    u32 contextOffset;
    u16 preSkipSamples;
    u16 _pad16;
} OpusFileHeader;

typedef struct {
    u32 chunkId;
    u32 chunkSize;
    u8 data[1];
} OpusDataChunk;

typedef struct {
    u32 packetSize;
    u32 finalRange;
    u8 packet[1];
} OpusPacketHeader;
#pragma pack(pop)

static void OpusPreprocess(u8* opusData) {
    if (!opusData) {
        panic("OpusPreprocess: data is NULL");
        return;
    }

    OpusFileHeader* fileHeader = (OpusFileHeader*)opusData;
    if (!fileHeader) {
        panic("OpusPreprocess: invalid file header");
        return;
    }

    if (fileHeader->chunkId == OGG_OPUS_ID) {
        panic("A Ogg Opus file was passed; please pass in a Nintendo Opus file.");
        return;
    }

    if (fileHeader->chunkId != CHUNK_HEADER_ID) {
        panic("OPUS file header ID is nonmatching");
        return;
    }

    if (fileHeader->contextOffset != 0) {
        warn("OPUS context is present but will be ignored");
    }

    if (fileHeader->sampleRate != 48000 && fileHeader->sampleRate != 24000 &&
        fileHeader->sampleRate != 16000 && fileHeader->sampleRate != 12000 &&
        fileHeader->sampleRate != 8000) {
        panic("Invalid OPUS sample rate (%uhz)", fileHeader->sampleRate);
        return;
    }

    if (fileHeader->channelCount != 1 && fileHeader->channelCount != 2) {
        panic("Invalid OPUS channel count (%u)", fileHeader->channelCount);
        return;
    }

    OpusDataChunk* dataChunk = (OpusDataChunk*)(opusData + fileHeader->dataOffset);
    if (!dataChunk) {
        panic("OPUS data chunk is NULL");
        return;
    }

    if (dataChunk->chunkId != CHUNK_DATA_ID) {
        panic("OPUS data chunk ID is nonmatching");
        return;
    }
}

static u32 OpusGetChannelCount(u8* opusData) {
    return ((OpusFileHeader*)opusData)->channelCount;
}

static u32 OpusGetSampleRate(u8* opusData) {
    return ((OpusFileHeader*)opusData)->sampleRate;
}

static ListData OpusDecode(u8* opusData) {
    if (!opusData) panic("OpusDecode: data is NULL");
    OpusFileHeader* fileHeader = (OpusFileHeader*)opusData;
    OpusDataChunk* dataChunk = (OpusDataChunk*)(opusData + fileHeader->dataOffset);
    int error;
    OpusDecoder* decoder = opus_decoder_create(fileHeader->sampleRate, fileHeader->channelCount, &error);
    if (error != OPUS_OK) panic("OpusDecode: opus_decoder_create fail: %s", opus_strerror(error));
    u32 coFrameSize = fileHeader->frameSize != 0 ? fileHeader->frameSize : (fileHeader->sampleRate / 50);
    s16* tempSamples = (s16*)malloc(coFrameSize * sizeof(s16) * fileHeader->channelCount);
    if (!tempSamples) panic("OpusDecode: failed to alloc temp buffer");
    ListData samples;
    ListInit(&samples, sizeof(s16), 1024);
    u32 offset = 0;
    int samplesLeftToSkip = fileHeader->preSkipSamples;
    while (offset < dataChunk->chunkSize) {
        OpusPacketHeader* packetHeader = (OpusPacketHeader*)(dataChunk->data + offset);
        u32 packetSize = byteswap32(packetHeader->packetSize);
        offset += sizeof(OpusPacketHeader) - 1 + packetSize;
        int samplesDecoded = opus_decode(decoder, packetHeader->packet, packetSize, tempSamples, coFrameSize, 0);
        if (samplesDecoded < 0) panic("OpusProcess: opus_decode fail: %s", opus_strerror(samplesDecoded));
        if (samplesLeftToSkip > 0) {
            if (samplesDecoded <= samplesLeftToSkip) {
                samplesLeftToSkip -= samplesDecoded;
            }
            else {
                int remainingSamples = samplesDecoded - samplesLeftToSkip;
                ListAddRange(&samples, tempSamples + samplesLeftToSkip * fileHeader->channelCount, remainingSamples * fileHeader->channelCount);
                samplesLeftToSkip = 0;
            }
        }
        else {
            ListAddRange(&samples, tempSamples, samplesDecoded * fileHeader->channelCount);
        }
    }
    opus_decoder_destroy(decoder);
    free(tempSamples);
    return samples;
}

#define OPUS_PACKETSIZE_MAX (1275)
#define OPUS_DEFAULT_BITRATE (96000)

typedef struct OpusBuildPacket {
    struct OpusBuildPacket* next;
    u32 packetLen;
    u32 finalRange;
    u8 packetData[1];
} OpusBuildPacket;

static MemoryFile OpusBuild(s16* samples, u32 sampleCount, u32 sampleRate, u32 channelCount) {
    MemoryFile mfResult = { 0 };

    if (!samples || sampleCount == 0) {
        panic("OpusBuild: invalid samples input");
        return mfResult;
    }

    if (channelCount != 1 && channelCount != 2) {
        panic("OpusBuild: Invalid channel count (%u)\nOnly one or two channels are allowed.", channelCount);
        return mfResult;
    }

    s16* processedSamples = samples;
    u32 processedSampleCount = sampleCount;
    u32 processedSampleRate = sampleRate;
    s16* resampledBuffer = NULL;

    if (sampleRate != 48000 && sampleRate != 24000 && sampleRate != 16000 &&
        sampleRate != 12000 && sampleRate != 8000) {

        printf("Resampling from %u Hz to 48000 Hz\n", sampleRate);

        processedSampleRate = 48000;
        double ratio = (double)processedSampleRate / sampleRate;
        u32 inputFrames = sampleCount / channelCount;
        u32 targetFrameCount = (u32)(inputFrames * ratio);
        processedSampleCount = targetFrameCount * channelCount;

        resampledBuffer = (s16*)malloc(processedSampleCount * sizeof(s16));
        if (!resampledBuffer) {
            panic("OpusBuild: Failed to allocate resample buffer");
            return mfResult;
        }

        for (u32 i = 0; i < targetFrameCount; i++) {
            double srcPos = i / ratio;
            u32 srcIdx = (u32)srcPos;
            double frac = srcPos - srcIdx;

            for (u32 ch = 0; ch < channelCount; ch++) {
                if (srcIdx + 1 < inputFrames) {
                    s16 sample0 = samples[srcIdx * channelCount + ch];
                    s16 sample1 = samples[(srcIdx + 1) * channelCount + ch];
                    resampledBuffer[i * channelCount + ch] = (s16)(sample0 * (1 - frac) + sample1 * frac);
                }
                else {
                    resampledBuffer[i * channelCount + ch] = samples[srcIdx * channelCount + ch];
                }
            }
        }

        processedSamples = resampledBuffer;
    }

    int opusError;
    OpusEncoder* encoder = opus_encoder_create(processedSampleRate, (int)channelCount, OPUS_APPLICATION_AUDIO, &opusError);
    if (opusError < 0 || !encoder) {
        if (resampledBuffer) free(resampledBuffer);
        panic("OpusBuild: opus_encoder_create failed: %s", opus_strerror(opusError));
        return mfResult;
    }

    opusError = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(OPUS_DEFAULT_BITRATE));
    if (opusError < 0) {
        opus_encoder_destroy(encoder);
        if (resampledBuffer) free(resampledBuffer);
        panic("OpusBuild: failed to set Opus bitrate to %u", OPUS_DEFAULT_BITRATE);
        return mfResult;
    }

    opusError = opus_encoder_ctl(encoder, OPUS_SET_VBR(1));
    if (opusError < 0) {
        opus_encoder_destroy(encoder);
        if (resampledBuffer) free(resampledBuffer);
        panic("OpusBuild: failed to enable Opus VBR");
        return mfResult;
    }

    opusError = opus_encoder_ctl(encoder, OPUS_SET_VBR_CONSTRAINT(0));
    if (opusError < 0) {
        opus_encoder_destroy(encoder);
        if (resampledBuffer) free(resampledBuffer);
        panic("OpusBuild: failed to disable Opus VBR constraint");
        return mfResult;
    }

    u32 frameDurationMs = 20;
    u32 frameSize = (processedSampleRate / 1000) * frameDurationMs;
    u32 samplesPerFrame = frameSize * channelCount;

    int preSkipSamples = 0;
    opusError = opus_encoder_ctl(encoder, OPUS_GET_LOOKAHEAD(&preSkipSamples));
    if (opusError < 0) {
        opus_encoder_destroy(encoder);
        if (resampledBuffer) free(resampledBuffer);
        panic("OpusBuild: failed to get pre-skip sample count");
        return mfResult;
    }

    OpusBuildPacket* rootPacket = (OpusBuildPacket*)malloc(sizeof(OpusBuildPacket));
    if (!rootPacket) {
        opus_encoder_destroy(encoder);
        if (resampledBuffer) free(resampledBuffer);
        panic("OpusBuild: failed to allocate root packet");
        return mfResult;
    }

    memset(rootPacket, 0, sizeof(OpusBuildPacket));
    rootPacket->next = NULL;
    rootPacket->packetLen = 0;
    rootPacket->finalRange = 0;

    OpusBuildPacket* currentPacket = rootPacket;
    u8 buffer[OPUS_PACKETSIZE_MAX];
    u32 packetCount = 0;

    for (u32 i = 0; i + samplesPerFrame <= processedSampleCount; i += samplesPerFrame) {
        int nbBytes = opus_encode(encoder, processedSamples + i, (int)frameSize, buffer, sizeof(buffer));
        if (nbBytes < 0) {
            opus_encoder_destroy(encoder);
            if (resampledBuffer) free(resampledBuffer);

            OpusBuildPacket* temp = rootPacket->next;
            while (temp) {
                OpusBuildPacket* next = temp->next;
                free(temp);
                temp = next;
            }
            free(rootPacket);

            panic("OpusBuild: opus_encode failed: %s", opus_strerror(nbBytes));
            return mfResult;
        }

        OpusBuildPacket* packet = (OpusBuildPacket*)malloc(sizeof(OpusBuildPacket) + nbBytes - 1);
        if (!packet) {
            opus_encoder_destroy(encoder);
            if (resampledBuffer) free(resampledBuffer);

            OpusBuildPacket* temp = rootPacket->next;
            while (temp) {
                OpusBuildPacket* next = temp->next;
                free(temp);
                temp = next;
            }
            free(rootPacket);

            panic("OpusBuild: failed to allocate packet");
            return mfResult;
        }

        packet->next = NULL;
        packet->packetLen = (u32)nbBytes;
        memcpy(packet->packetData, buffer, (size_t)nbBytes);

        opusError = opus_encoder_ctl(encoder, OPUS_GET_FINAL_RANGE(&packet->finalRange));
        if (opusError < 0) {
            opus_encoder_destroy(encoder);
            if (resampledBuffer) free(resampledBuffer);
            free(packet);

            OpusBuildPacket* temp = rootPacket->next;
            while (temp) {
                OpusBuildPacket* next = temp->next;
                free(temp);
                temp = next;
            }
            free(rootPacket);

            panic("OpusBuild: failed to get encoder final range");
            return mfResult;
        }

        currentPacket->next = packet;
        currentPacket = packet;
        packetCount++;
    }

    opus_encoder_destroy(encoder);

    mfResult.size = sizeof(OpusFileHeader) + sizeof(OpusDataChunk) - 1;

    currentPacket = rootPacket->next;
    while (currentPacket) {
        mfResult.size += sizeof(OpusPacketHeader) - 1 + currentPacket->packetLen;
        currentPacket = currentPacket->next;
    }

    mfResult.data_u8 = (u8*)malloc((size_t)mfResult.size);
    if (!mfResult.data_u8) {
        if (resampledBuffer) free(resampledBuffer);

        OpusBuildPacket* temp = rootPacket->next;
        while (temp) {
            OpusBuildPacket* next = temp->next;
            free(temp);
            temp = next;
        }
        free(rootPacket);

        panic("OpusBuild: failed to allocate file buffer");
        return mfResult;
    }

    memset(mfResult.data_u8, 0, (size_t)mfResult.size);

    OpusFileHeader* fileHeader = (OpusFileHeader*)mfResult.data_u8;
    if (!fileHeader) {
        free(mfResult.data_u8);
        mfResult.data_u8 = NULL;

        if (resampledBuffer) free(resampledBuffer);

        OpusBuildPacket* temp = rootPacket->next;
        while (temp) {
            OpusBuildPacket* next = temp->next;
            free(temp);
            temp = next;
        }
        free(rootPacket);

        panic("OpusBuild: invalid file header");
        return mfResult;
    }

    fileHeader->chunkId = CHUNK_HEADER_ID;
    fileHeader->chunkSize = (u32)(sizeof(OpusFileHeader) - 8);
    fileHeader->version = OPUS_VERSION;
    fileHeader->channelCount = (u8)channelCount;
    fileHeader->frameSize = 0;
    fileHeader->sampleRate = processedSampleRate;
    fileHeader->dataOffset = sizeof(OpusFileHeader);
    fileHeader->_unk14 = 0;
    fileHeader->contextOffset = 0;
    fileHeader->preSkipSamples = (u16)preSkipSamples;
    fileHeader->_pad16 = 0;

    OpusDataChunk* dataChunk = (OpusDataChunk*)(fileHeader + 1);
    if (!dataChunk) {
        free(mfResult.data_u8);
        mfResult.data_u8 = NULL;

        if (resampledBuffer) free(resampledBuffer);

        OpusBuildPacket* temp = rootPacket->next;
        while (temp) {
            OpusBuildPacket* next = temp->next;
            free(temp);
            temp = next;
        }
        free(rootPacket);

        panic("OpusBuild: invalid data chunk");
        return mfResult;
    }

    dataChunk->chunkId = CHUNK_DATA_ID;
    dataChunk->chunkSize = (u32)(mfResult.size - sizeof(OpusFileHeader) - (sizeof(OpusDataChunk) - 1));

    OpusPacketHeader* currentPacketHeader = (OpusPacketHeader*)dataChunk->data;
    currentPacket = rootPacket->next;

    while (currentPacket) {
        if (!currentPacketHeader) {
            free(mfResult.data_u8);
            mfResult.data_u8 = NULL;

            if (resampledBuffer) free(resampledBuffer);

            OpusBuildPacket* temp = rootPacket->next;
            while (temp) {
                OpusBuildPacket* next = temp->next;
                free(temp);
                temp = next;
            }
            free(rootPacket);

            panic("OpusBuild: invalid packet header");
            return mfResult;
        }

        currentPacketHeader->packetSize = byteswap32(currentPacket->packetLen);
        currentPacketHeader->finalRange = byteswap32(currentPacket->finalRange);
        memcpy(currentPacketHeader->packet, currentPacket->packetData, (size_t)currentPacket->packetLen);

        currentPacketHeader = (OpusPacketHeader*)((u8*)currentPacketHeader + sizeof(OpusPacketHeader) - 1 + currentPacket->packetLen);
        currentPacket = currentPacket->next;
    }

    currentPacket = rootPacket->next;
    while (currentPacket) {
        OpusBuildPacket* nextPacket = currentPacket->next;
        free(currentPacket);
        currentPacket = nextPacket;
    }
    free(rootPacket);

    if (resampledBuffer) {
        free(resampledBuffer);
    }

    return mfResult;
}

#define RIFF_MAGIC IDENTIFIER_TO_U32('R', 'I', 'F', 'F')
#define WAVE_MAGIC IDENTIFIER_TO_U32('W', 'A', 'V', 'E')
#define FMT__MAGIC IDENTIFIER_TO_U32('f', 'm', 't', ' ')
#define DATA_MAGIC IDENTIFIER_TO_U32('d', 'a', 't', 'a')
#define FMT_FORMAT_PCM (0x0001)
#define FMT_FORMAT_FLOAT (0x0003)

#pragma pack(push, 1)
typedef struct {
    u32 riffMagic;
    u32 fileSize;
    u32 waveMagic;
} WavFileHeader;

typedef struct {
    u32 magic;
    u32 chunkSize;
    u16 format;
    u16 channelCount;
    u32 sampleRate;
    u32 dataRate;
    u16 blockSize;
    u16 bitsPerSample;
} WavFmtChunk;

typedef struct {
    u32 magic;
    u32 chunkSize;
    u8 data[1];
} WavDataChunk;

typedef struct {
    u32 magic;
    u32 chunkSize;
} WavChunkHeader;
#pragma pack(pop)

static const u8* _WavFindChunk(const u8* chunksStart, u32 chunksSize, u32 targetMagic) {
    const u8* ptr = chunksStart;
    const u8* end = chunksStart + chunksSize;
    while (ptr + sizeof(WavChunkHeader) <= end) {
        const WavChunkHeader* chunk = (const WavChunkHeader*)ptr;
        if (chunk->magic == targetMagic) return ptr;
        ptr += sizeof(WavChunkHeader) + chunk->chunkSize;
        if (chunk->chunkSize % 2 != 0) ptr++;
    }
    char magicChars[4] = { (char)(targetMagic & 0xFF), (char)((targetMagic >> 8) & 0xFF), (char)((targetMagic >> 16) & 0xFF), (char)((targetMagic >> 24) & 0xFF) };
    panic("WAV '%c%c%c%c' chunk not found", magicChars[0], magicChars[1], magicChars[2], magicChars[3]);
}

static void WavPreprocess(const u8* wavData, u32 wavDataSize) {
    if (!wavData) {
        panic("WAV data is NULL");
        return;
    }

    if (wavDataSize < sizeof(WavFileHeader)) {
        panic("WAV data too small");
        return;
    }

    const WavFileHeader* fileHeader = (const WavFileHeader*)wavData;
    if (!fileHeader) {
        panic("WAV file header is NULL");
        return;
    }

    if (fileHeader->riffMagic != RIFF_MAGIC) {
        panic("WAV RIFF magic is nonmatching");
        return;
    }

    if (fileHeader->waveMagic != WAVE_MAGIC) {
        panic("WAV WAVE magic is nonmatching");
        return;
    }

    const u8* chunksStart = wavData + sizeof(WavFileHeader);
    u32 chunksSize = wavDataSize - sizeof(WavFileHeader);

    const WavFmtChunk* fmtChunk = (const WavFmtChunk*)_WavFindChunk(chunksStart, chunksSize, FMT__MAGIC);
    if (!fmtChunk) {
        panic("WAV fmt chunk not found");
        return;
    }

    if (fmtChunk->format != FMT_FORMAT_PCM && fmtChunk->format != FMT_FORMAT_FLOAT) {
        panic("WAV format is unsupported: %u", fmtChunk->format);
        return;
    }

    if (fmtChunk->format == FMT_FORMAT_PCM) {
        if (fmtChunk->bitsPerSample != 16 && fmtChunk->bitsPerSample != 24) {
            panic("%u-bit PCM isn't supported", (unsigned)fmtChunk->bitsPerSample);
            return;
        }
    }
    else if (fmtChunk->format == FMT_FORMAT_FLOAT) {
        if (fmtChunk->bitsPerSample != 32) {
            panic("%u-bit FLOAT isn't supported", (unsigned)fmtChunk->bitsPerSample);
            return;
        }
    }
}

static u32 WavGetSampleRate(const u8* wavData, u32 wavDataSize) {
    const WavFmtChunk* fmtChunk = (const WavFmtChunk*)_WavFindChunk(wavData + sizeof(WavFileHeader), wavDataSize - sizeof(WavFileHeader), FMT__MAGIC);
    return fmtChunk->sampleRate;
}

static u16 WavGetChannelCount(const u8* wavData, u32 wavDataSize) {
    const WavFmtChunk* fmtChunk = (const WavFmtChunk*)_WavFindChunk(wavData + sizeof(WavFileHeader), wavDataSize - sizeof(WavFileHeader), FMT__MAGIC);
    return fmtChunk->channelCount;
}

static u32 WavGetSampleCount(const u8* wavData, u32 wavDataSize) {
    const u8* chunksStart = wavData + sizeof(WavFileHeader);
    u32 chunksSize = wavDataSize - sizeof(WavFileHeader);
    const WavFmtChunk* fmtChunk = (const WavFmtChunk*)_WavFindChunk(chunksStart, chunksSize, FMT__MAGIC);
    const WavDataChunk* dataChunk = (const WavDataChunk*)_WavFindChunk(chunksStart, chunksSize, DATA_MAGIC);
    return dataChunk->chunkSize / (fmtChunk->bitsPerSample / 8);
}

static s16* WavGetPCM16(const u8* wavData, u32 wavDataSize) {
    if (!wavData || wavDataSize < sizeof(WavFileHeader)) {
        panic("WavGetPCM16: invalid wav data");
        return NULL;
    }

    const u8* chunksStart = wavData + sizeof(WavFileHeader);
    u32 chunksSize = wavDataSize - sizeof(WavFileHeader);

    const WavFmtChunk* fmtChunk = (const WavFmtChunk*)_WavFindChunk(chunksStart, chunksSize, FMT__MAGIC);
    if (!fmtChunk) {
        panic("WavGetPCM16: fmt chunk not found");
        return NULL;
    }

    const WavDataChunk* dataChunk = (const WavDataChunk*)_WavFindChunk(chunksStart, chunksSize, DATA_MAGIC);
    if (!dataChunk) {
        panic("WavGetPCM16: data chunk not found");
        return NULL;
    }

    u32 bytesPerSample = fmtChunk->bitsPerSample / 8;
    if (bytesPerSample == 0) {
        panic("WavGetPCM16: invalid bits per sample");
        return NULL;
    }

    u32 sampleCount = dataChunk->chunkSize / bytesPerSample;
    if (sampleCount == 0) {
        panic("WavGetPCM16: no samples");
        return NULL;
    }

    s16* dstSamples = (s16*)malloc(sizeof(s16) * sampleCount);
    if (!dstSamples) {
        panic("WavGetPCM16: failed to allocate result buf");
        return NULL;
    }

    if (fmtChunk->format == FMT_FORMAT_FLOAT) {
        if (!dataChunk->data) {
            free(dstSamples);
            panic("WavGetPCM16: invalid float data");
            return NULL;
        }
        const float* srcSamples = (const float*)dataChunk->data;
        for (u32 i = 0; i < sampleCount; i++) {
            float sample = srcSamples[i] * 32768.0f;
            if (sample > 32767.0f) sample = 32767.0f;
            if (sample < -32768.0f) sample = -32768.0f;
            dstSamples[i] = (s16)sample;
        }
    }
    else if (fmtChunk->format == FMT_FORMAT_PCM && fmtChunk->bitsPerSample == 16) {
        if (!dataChunk->data) {
            free(dstSamples);
            panic("WavGetPCM16: invalid 16-bit data");
            return NULL;
        }
        memcpy(dstSamples, dataChunk->data, sizeof(s16) * sampleCount);
    }
    else if (fmtChunk->format == FMT_FORMAT_PCM && fmtChunk->bitsPerSample == 24) {
        if (!dataChunk->data) {
            free(dstSamples);
            panic("WavGetPCM16: invalid 24-bit data");
            return NULL;
        }
        const u8* srcSamples = (const u8*)dataChunk->data;
        for (u32 i = 0; i < sampleCount; i++) {
            s32 sample = ((s32)(srcSamples[i * 3 + 0]) << 8) |
                ((s32)(srcSamples[i * 3 + 1]) << 16) |
                ((s32)(srcSamples[i * 3 + 2]) << 24);
            s32 normalizedSample = sample >> 8;
            if (normalizedSample > 32767) normalizedSample = 32767;
            if (normalizedSample < -32768) normalizedSample = -32768;
            dstSamples[i] = (s16)normalizedSample;
        }
    }
    else {
        free(dstSamples);
        panic("WavGetPCM16: no convert condition met");
        return NULL;
    }

    return dstSamples;
}

static MemoryFile WavBuild(s16* samples, u32 sampleCount, u32 sampleRate, u16 channelCount) {
    MemoryFile mfResult = { 0 };

    if (!samples || sampleCount == 0) {
        panic("WavBuild: invalid samples input");
        return mfResult;
    }

    u32 dataSize = sampleCount * sizeof(s16);
    u32 fileSize = sizeof(WavFileHeader) + sizeof(WavFmtChunk) + sizeof(WavDataChunk) - 1 + dataSize;

    mfResult.data_u8 = (u8*)malloc(fileSize);
    if (!mfResult.data_u8) {
        panic("WavBuild: failed to allocate memfile");
        return mfResult;
    }

    mfResult.size = fileSize;
    memset(mfResult.data_u8, 0, fileSize);

    WavFileHeader* wavFileHeader = (WavFileHeader*)mfResult.data_u8;
    if (!wavFileHeader) {
        free(mfResult.data_u8);
        mfResult.data_u8 = NULL;
        panic("WavBuild: invalid file header");
        return mfResult;
    }

    WavFmtChunk* wavFmtChunk = (WavFmtChunk*)(wavFileHeader + 1);
    if (!wavFmtChunk) {
        free(mfResult.data_u8);
        mfResult.data_u8 = NULL;
        panic("WavBuild: invalid fmt chunk");
        return mfResult;
    }

    WavDataChunk* wavDataChunk = (WavDataChunk*)(wavFmtChunk + 1);
    if (!wavDataChunk) {
        free(mfResult.data_u8);
        mfResult.data_u8 = NULL;
        panic("WavBuild: invalid data chunk");
        return mfResult;
    }

    s16* dataStart = (s16*)wavDataChunk->data;
    if (!dataStart) {
        free(mfResult.data_u8);
        mfResult.data_u8 = NULL;
        panic("WavBuild: invalid data start");
        return mfResult;
    }

    wavFileHeader->riffMagic = RIFF_MAGIC;
    wavFileHeader->waveMagic = WAVE_MAGIC;
    wavFileHeader->fileSize = fileSize - 8;

    wavFmtChunk->magic = FMT__MAGIC;
    wavFmtChunk->chunkSize = 16;
    wavFmtChunk->format = FMT_FORMAT_PCM;
    wavFmtChunk->channelCount = channelCount;
    wavFmtChunk->sampleRate = sampleRate;
    wavFmtChunk->dataRate = sampleRate * sizeof(s16) * channelCount;
    wavFmtChunk->blockSize = (u16)(sizeof(s16) * channelCount);
    wavFmtChunk->bitsPerSample = 8 * sizeof(s16);

    wavDataChunk->magic = DATA_MAGIC;
    wavDataChunk->chunkSize = dataSize;

    memcpy(dataStart, samples, dataSize);

    return mfResult;
}

int main(int argc, char** argv) {
    printf("Nintendo OPUS <-> WAV converter tool");

    if (argc < 3) {
        printf("usage: %s -e <wav>  (encode wav to lopus)\n", argv[0]);
        printf("       %s -d <lopus>  (decode lopus to wav)\n", argv[0]);
        return 1;
    }

    char* inputPath = argv[2];
    char outputPath[512] = { 0 };
    int isEncode = (strcmp(argv[1], "-e") == 0);
    int isDecode = (strcmp(argv[1], "-d") == 0);

    if (!isEncode && !isDecode) {
        printf("Invalid option: %s\n", argv[1]);
        printf("usage: %s -e <wav>  (encode wav to lopus)\n", argv[0]);
        printf("       %s -d <lopus>  (decode lopus to wav)\n", argv[0]);
        return 1;
    }

    char* ext = strrchr(inputPath, '.');
    if (ext) {
        size_t len = ext - inputPath;
        strncpy(outputPath, inputPath, len);
        outputPath[len] = '\0';
    }
    else {
        strcpy(outputPath, inputPath);
    }

    if (isEncode) {
        strcat(outputPath, ".lopus");
        printf("- Converting wav at path \"%s\" to lopus at path \"%s\"..\n\n", inputPath, outputPath);

        MemoryFile mfWav = MemoryFileCreate(inputPath);
        WavPreprocess(mfWav.data_u8, (u32)mfWav.size);
        u32 channelCount = WavGetChannelCount(mfWav.data_u8, (u32)mfWav.size);
        u32 sampleRate = WavGetSampleRate(mfWav.data_u8, (u32)mfWav.size);
        s16* samples = WavGetPCM16(mfWav.data_u8, (u32)mfWav.size);
        u32 sampleCount = WavGetSampleCount(mfWav.data_u8, (u32)mfWav.size);

        printf("Encoding..");
        fflush(stdout);
        MemoryFile mfOpus = OpusBuild(samples, sampleCount, sampleRate, channelCount);
        printf(" OK\n");

        free(samples);
        MemoryFileDestroy(&mfWav);

        printf("Writing lopus..");
        fflush(stdout);
        MemoryFileWrite(&mfOpus, outputPath);
        MemoryFileDestroy(&mfOpus);
        printf(" OK\n");
    }
    else {
        strcat(outputPath, ".wav");
        printf("- Converting lopus at path \"%s\" to wav at path \"%s\"..\n\n", inputPath, outputPath);

        MemoryFile mfOpus = MemoryFileCreate(inputPath);
        OpusPreprocess(mfOpus.data_u8);
        u32 channelCount = OpusGetChannelCount(mfOpus.data_u8);
        u32 sampleRate = OpusGetSampleRate(mfOpus.data_u8);

        printf("Decoding..");
        fflush(stdout);
        ListData samples = OpusDecode(mfOpus.data_u8);
        printf(" OK\n");

        MemoryFileDestroy(&mfOpus);

        printf("Creating wav..");
        fflush(stdout);
        MemoryFile mfWav = WavBuild((s16*)samples.data, (u32)samples.elementCount, sampleRate, (u16)channelCount);
        ListDestroy(&samples);
        MemoryFileWrite(&mfWav, outputPath);
        MemoryFileDestroy(&mfWav);
        printf(" OK\n");
    }

    printf("\nAll done.\n");
    return 0;
}
