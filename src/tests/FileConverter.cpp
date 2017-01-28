#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include "Buffer/ProcessorSelector.h"
#include "Message/MessageConverter.h"
#include "Core/ProcessTrivial.h"    //only for random inputs

const int MAX_POS_ARGS = 2;
const int MAX_ARG_LEN = 1<<12;
FILE * const LOG_FILE = stderr;

void logprintf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(LOG_FILE, format, args);
    va_end(args);
}

void strtolower(char *s) {
    for (int i = 0; s[i]; i++)
        s[i] = tolower(s[i]);
}

void Check(bool condition, const char *format, ...) {
    if (condition)
        return;
    va_list args;
    va_start(args, format);
    vfprintf(LOG_FILE, format, args);
    va_end(args);
    exit(17);
}

int GetFormatOfEncoding(const char *encoding) {
    if (strcmp(encoding, "utf8") == 0 || strcmp(encoding, "utf-8") == 0)
        return dfUtf8;
    if (strcmp(encoding, "utf16") == 0 || strcmp(encoding, "utf-16") == 0)
        return dfUtf16;
    if (strcmp(encoding, "utf32") == 0 || strcmp(encoding, "utf-32") == 0)
        return dfUtf32;
    Check(false, "Unknown encoding: %s\n", encoding);
    return -1;
}
const char *GetFormatStr(int format) {
    if (format == dfUtf8 ) return "UTF-8 ";
    if (format == dfUtf16) return "UTF-16";
    if (format == dfUtf32) return "UTF-32";
    return 0;
}

struct Config {
    int srcFormat;              // -s=%s
    int dstFormat;              // -d=%s
    int maxBytesFast;           // -b=%d
    bool smallConverter;        // --small
    bool errorCorrection;       // -ec
    bool fileToFile;            // --file
    int numberOfRuns;           // -k=%d
    char srcPath[MAX_ARG_LEN];
    char dstPath[MAX_ARG_LEN];
    int srcRandomLen;           // input: [rnd%c%c%c%c:%d]
    bool srcRandomChars[4];     // -- | --
    bool dstPrintHash;          // output: [hash]
    
    Config() {
        srcFormat = dfUtf8;
        dstFormat = dfUtf16;
        maxBytesFast = 3;
        smallConverter = false;
        errorCorrection = false;
        fileToFile = false;
        numberOfRuns = 1;
        srcPath[0] = 0;
        dstPath[0] = 0;
        srcRandomLen = -1;
        dstPrintHash = false;
    }
    void Init() {
        Check(numberOfRuns >= 0, "Cannot run negative number of times: %d\n", numberOfRuns);
        Check(srcFormat != dstFormat, "Source and destination encoding must be different (%d)\n", srcFormat);
        Check(srcFormat == dfUtf8 || dstFormat == dfUtf8, "Either source of destination encoding must be UTF-8\n");
        Check(!fileToFile || (srcPath[0] && dstPath[0]), "Both input and output must be file paths when using file-to-file mode\n");
        Check(maxBytesFast >= 1 && maxBytesFast <= 3, "Fast path can process up to 1-byte, 2-byte, or 3-byte code points (%d)\n", maxBytesFast);
        if (errorCorrection && !smallConverter) {
            smallConverter = true;
            logprintf("Note: Error correction forces 'small' mode of processor\n");
        }

        logprintf("Starting the following conversion");
        if (numberOfRuns != 1)
            logprintf(" (%d times)", numberOfRuns);
        logprintf(":\n");
        char randomSrcMessage[256] = {0};
        if (srcRandomLen >= 0) {
            char temp[16], tl = 0;
            for (int t = 0; t < 4; t++) if (srcRandomChars[t]) temp[tl++] = '1' + t;
            temp[tl] = 0;
            sprintf(randomSrcMessage, "[random: %d chars of [%s]]", srcRandomLen, temp);
        }
        logprintf("  Input (source) in %s: %s\n", GetFormatStr(srcFormat), (srcRandomLen >= 0 ? randomSrcMessage : srcPath));
        logprintf("  Output (dest.) in %s: %s\n", GetFormatStr(dstFormat), (dstPrintHash ? "[print hash]" : dstPath));
        if (fileToFile) 
            logprintf("  Direct file-to-file conversion\n");

        logprintf("Processor settings:\n");
        logprintf("  Fast path supports code points with up to %d bytes in UTF-8\n", maxBytesFast);
        if (smallConverter)
            logprintf("  Using small processor, i.e. single stream / no unrolling\n");
        else
            logprintf("  Using big converter, i.e. four streams / 4x unrolling\n");
        if (errorCorrection)
            logprintf("  Error correction enabled\n");
        else
            logprintf("  Stops on any incorrect input\n");
    }
};


//linked from AllProcessors.cpp
BaseBufferProcessor* GenerateProcessor(int srcFormat, int dstFormat, int maxBytes, int checkMode, int multiplier, int *errorCounter);

void ReadFileContents(const char *filename, char *&buffer, long long &size) {
    assert(!buffer);
    FILE *fi = fopen(filename, "rb");
    Check(fi, "Cannot open file: %s\n", filename);
    fseek(fi, 0, SEEK_END);
    size = ftell(fi);
    fseek(fi, 0, SEEK_SET);
    buffer = new char[size];
    fread(buffer, 1, size, fi);
    fclose(fi);
}

void WriteFileContents(const char *filename, char *buffer, long long size) {
    FILE *fo = fopen(filename, "wb");
    Check(fo, "Cannot open file: %s\n", filename);
    fwrite(buffer, 1, size, fo);
    fclose(fo);
}

int MaxCodeOf(int bytes) {
    if (bytes == 0) return -1;
    if (bytes == 1) return 0x0000007F;
    if (bytes == 2) return 0x000007FF;
    if (bytes == 3) return 0x0000FFFF;
    return 0x10FFFF;
}
int MyRandom() { return (rand() << 15) ^ rand(); }
void GenerateRandomSource(char *&buffer, long long &size, int format, int cnt, bool allowedLens[4]) {
    assert(!buffer);
    int *buff32 = new int[cnt];
    for (int i = 0; i < cnt; i++) {
        int b;
        do { b = rand() & 3; } while (!allowedLens[b]);
        b++;
        int minV = MaxCodeOf(b-1) + 1, maxV = MaxCodeOf(b);
        int code;
        do {
            code = minV + MyRandom() % (maxV - minV + 1);
        } while (code >= 0xD800 && code < 0xE000);
        buff32[i] = code;
    }
    if (format == dfUtf32) {
        buffer = (char*)(void*)buff32;
        size = 4*cnt;
        return;
    }
    char *buff8 = new char[4*cnt];
    char *buff8end;
    {
        const char *ptr = (char*)buff32;
        buff8end = buff8;
        EncodeTrivial<4>(ptr, (char*)(buff32 + cnt), buff8end);
        delete[] buff32;
    }
    if (format == dfUtf8) {
        buffer = buff8;
        size = buff8end - buff8;
        return;
    }
    char *buff16 = new char[4*cnt];
    char *buff16end;
    {
        const char *ptr = buff8;
        buff16end = buff16;
        DecodeTrivial<2>(ptr, buff8end, buff16end);
        delete[] buff8;
    }
    buffer = buff16;
    size = buff16end - buff16;
}


bool IsSameResult (const ConversionResult &a, const ConversionResult &b) {
    return a.status == b.status && a.inputSize == b.inputSize && a.outputSize == b.outputSize;
}
void PrintResult(const ConversionResult &res) {
    logprintf("Conversion result: %s;   converted %" PRId64 " bytes -> %" PRId64 " bytes\n",
        res.status == csSuccess ? "success" :
        res.status == csOverflowPossible ? "OVERFLOW" :
        res.status == csIncompleteData ? "incomplete data" :
        res.status == csIncorrectData ? "INCORRECT" :
        res.status == csInputOutputNoAccess ? "ACCESS DENIED" :
        "!!!unknown!!!",
        res.inputSize, res.outputSize
    );
}
unsigned int GetHashOfBuffer(const char *buffer, long long size) {
    unsigned int hash = 0;
    for (long long i = 0; i < size; i++)
        hash = hash * 31 + buffer[i];
    return hash;
}


int main(int argc, char **argv) {
    Config cfg;

    int posArgsCnt = 0;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        Check(strlen(arg) < MAX_ARG_LEN, "Argument too long: %s\n", arg);

        char larg[MAX_ARG_LEN];
        strcpy(larg, arg);
        strtolower(larg);

        int num;
        char str[MAX_ARG_LEN];

        if (0);
        else if (strcmp(larg, "-h") == 0 || strcmp(larg, "--help") == 0) {
            logprintf(
                "TODO!\n"
            );
            exit(1);
        }
        else if (sscanf(larg, "-s=%s", &str) == 1)
            cfg.srcFormat = GetFormatOfEncoding(str);
        else if (sscanf(larg, "-d=%s", &str) == 1)
            cfg.dstFormat = GetFormatOfEncoding(str);
        else if (sscanf(larg, "-b=%d", &num) == 1)
            cfg.maxBytesFast = num;
        else if (strcmp(larg, "--small") == 0)
            cfg.smallConverter = true;
        else if (strcmp(larg, "--file") == 0)
            cfg.fileToFile = true;
        else if (strcmp(larg, "-ec") == 0)
            cfg.errorCorrection = true;
        else if (sscanf(larg, "-k=%d", &num) == 1)
            cfg.numberOfRuns = num;
        else if (larg[0] != '-') {
            Check(posArgsCnt < MAX_POS_ARGS, "No more positional arguments allowed: %s\n", arg);
            if (0 == posArgsCnt) {
                if (sscanf(larg, "[rnd%c%c%c%c:%d]", str+0, str+1, str+2, str+3, &num) == 5) {
                    cfg.srcRandomLen = num;
                    for (int t = 0; t < 4; t++)
                        cfg.srcRandomChars[t] = strchr("1tTyY+", str[t]) != 0;
                }
                else
                    strcpy(cfg.srcPath, arg);
            }
            if (1 == posArgsCnt) {
                if (strcmp(larg, "[hash]") == 0)
                    cfg.dstPrintHash = true;
                else
                    strcpy(cfg.dstPath, arg);
            }
            posArgsCnt++;
        }
        else
            logprintf("Ignored command line parameter: %s\n", arg);
    }

    Check(posArgsCnt == 2, "Must specify input and output files as two positional arguments\n");
    cfg.Init();
    logprintf("\n");

    int errorCounter = 0;
    BaseBufferProcessor *processor = GenerateProcessor(
        cfg.srcFormat,
        cfg.dstFormat,
        cfg.maxBytesFast,
        cmValidate,
        cfg.smallConverter ? 1 : 4,
        cfg.errorCorrection ? &errorCounter : 0
    );
    Check(processor, "Cannot generate processor with specified parameters!\n");
    logprintf("Generated processor for conversion\n");

    ConversionResult allResult;
    clock_t startTime = clock();
    if (cfg.fileToFile) {
        for (int r = 0; r < cfg.numberOfRuns; r++) {
            ConversionResult convres = ConvertFiles(*processor, cfg.srcPath, cfg.dstPath);
            if (r && !IsSameResult(allResult, convres))
                logprintf("Consecutive conversion runs produce different results!\n");
            allResult = convres;
        }
    }
    else {
        char *inputData = 0;
        long long inputSize = 0;
        if (cfg.srcRandomLen >= 0) {
            GenerateRandomSource(inputData, inputSize, cfg.srcFormat, cfg.srcRandomLen, cfg.srcRandomChars);
            logprintf("Generated random input buffer\n");
        }
        else {
            ReadFileContents(cfg.srcPath, inputData, inputSize);
            logprintf("Read input buffer from file\n");
        }

        char *outputData = 0;
        long long outputSize = ConvertInMemorySize(*processor, inputSize, &outputData);

        for (int r = 0; r < cfg.numberOfRuns; r++) {
            ConversionResult convres = ConvertInMemory(*processor, inputData, inputSize, outputData, outputSize);
            if (r && !IsSameResult(allResult, convres))
                logprintf("Consecutive conversion runs produce different results!\n");
            allResult = convres;
        }
        logprintf("Conversion%s complete\n", (cfg.numberOfRuns == 1 ? "" : " (all runs)"));

        if (cfg.dstPrintHash) {
            unsigned int hash = GetHashOfBuffer(outputData, outputSize);
            logprintf("Computed hash value of output: %08X\n", hash);
        }
        else {
            WriteFileContents(cfg.dstPath, outputData, outputSize);
            logprintf("Wrote output buffer to file\n");
        }

        delete[] inputData;
        delete[] outputData;
    }
    clock_t endTime = clock();
    double elapsedTime = double(endTime - startTime) / CLOCKS_PER_SEC;

    logprintf("The task was finished in %0.3lf seconds\n", elapsedTime);
    PrintResult(allResult);

    delete processor;
    processor = 0;
    logprintf("Destroyed processor\n");

    logprintf("\n");

#ifdef TIMING
    logprintf("Internal timings:\n");
    TimingPrintAll(LOG_FILE);
#endif

    return 0;
}
