#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include "Buffer/ProcessorSelector.h"
#include "Message/MessageConverter.h"

const int MAX_POS_ARGS = 2;
const int MAX_ARG_LEN = 1<<12;


void strtolower(char *s) {
    for (int i = 0; s[i]; i++)
        s[i] = tolower(s[i]);
}

void Check(bool condition, const char *format, ...) {
    if (condition)
        return;
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
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
    int srcRandomMask;          // input: [rnd%c%c%c%c]
    bool dstCheckSum;           // output: [sum]
    
    Config() {
        srcFormat = 1;
        dstFormat = 2;
        maxBytesFast = 3;
        smallConverter = false;
        errorCorrection = false;
        fileToFile = false;
        numberOfRuns = 1;
        srcPath[0] = 0;
        dstPath[0] = 0;
        srcRandomMask = -1;
        dstCheckSum = false;
    }
    void Init() {
        Check(numberOfRuns >= 0, "Cannot run negative number of times: %d\n", numberOfRuns);
        Check(srcFormat != dstFormat, "Source and destination encoding must be different (%d)\n", srcFormat);
        Check(srcFormat == 1 || dstFormat == 1, "Either source of destination encoding must be UTF-8\n");
        Check(!fileToFile || (srcPath[0] && dstPath[0]), "Both input and output must be file paths when using file-to-file mode\n");
        Check(maxBytesFast >= 1 && maxBytesFast <= 3, "Fast path can process up to 1-byte, 2-byte, or 3-byte code points (%d)\n", maxBytesFast);
        Check(!errorCorrection || smallConverter, "Error correction is only supported in 'small' mode\n");

        //TODO: print header to stderr
    }
};


//linked from AllProcessors.cpp
BaseBufferProcessor* GenerateProcessor(int srcFormat, int dstFormat, int maxBytes, int checkMode, int multiplier, int *errorCounter);

void ReadFileContents(const char *filename, char *&buffer, long long &size) {
    assert(!buffer);
    FILE *fi = fopen(filename, "rb");
    fseek(fi, 0, SEEK_END);
    size = ftell(fi);
    fseek(fi, 0, SEEK_SET);
    buffer = new char[size];
    fread(buffer, 1, size, fi);
    fclose(fi);
}

void WriteFileContents(const char *filename, char *buffer, long long size) {
    FILE *fo = fopen(filename, "wb");
    fwrite(buffer, 1, size, fo);
    fclose(fo);
}


bool IsSameResult (const ConversionResult &a, const ConversionResult &b) {
    return a.status == b.status && a.inputSize == b.inputSize && a.outputSize == b.outputSize;
}
void PrintResult(const ConversionResult &res) {
    fprintf(stderr, "Conversion result: %s;   converted %" PRId64 " bytes -> %" PRId64 " bytes\n",
        res.status == csSuccess ? "success" :
        res.status == csOverflowPossible ? "OVERFLOW" :
        res.status == csIncompleteData ? "incomplete data" :
        res.status == csIncorrectData ? "INCORRECT" :
        res.status == csInputOutputNoAccess ? "ACCESS DENIED" :
        "!!!unknown!!!",
        res.inputSize, res.outputSize
    );
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
            fprintf(stderr, 
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
                //sscanf("[rnd%c%c%c%c]", &)    //TODO
                strcpy(cfg.srcPath, arg);
            }
            if (1 == posArgsCnt) {
                if (strcmp(larg, "[sum]") == 0)
                    cfg.dstCheckSum = true;
                else
                    strcpy(cfg.dstPath, arg);
            }
            posArgsCnt++;
        }
        else
            fprintf(stderr, "Ignored command line parameter: %s\n", arg);
    }

    Check(posArgsCnt == 2, "Must specify input and output files as two positional arguments");
    cfg.Init();

    int errorCounter = 0;
    BaseBufferProcessor *processor = GenerateProcessor(
        cfg.srcFormat,
        cfg.dstFormat,
        cfg.maxBytesFast,
        cmValidate,
        cfg.smallConverter ? 1 : 4,
        cfg.errorCorrection ? &errorCounter : 0
    );
    Check(processor, "Cannot generate processor with specified parameters!");

    ConversionResult allResult;
    if (cfg.fileToFile) {
        for (int r = 0; r < cfg.numberOfRuns; r++) {
            ConversionResult convres = ConvertFiles(*processor, cfg.srcPath, cfg.dstPath);
            if (r && !IsSameResult(allResult, convres))
                fprintf(stderr, "Consecutive conversion runs produce different results!\n");
            allResult = convres;
        }
    }
    else {
        char *inputData = 0;
        long long inputSize = 0;
        ReadFileContents(cfg.srcPath, inputData, inputSize);

        char *outputData = 0;
        long long outputSize = ConvertInMemorySize(*processor, inputSize, &outputData);

        for (int r = 0; r < cfg.numberOfRuns; r++) {
            ConversionResult convres = ConvertInMemory(*processor, inputData, inputSize, outputData, outputSize);
            if (r && !IsSameResult(allResult, convres))
                fprintf(stderr, "Consecutive conversion runs produce different results!\n");
            allResult = convres;
        }

        WriteFileContents(cfg.dstPath, outputData, outputSize);
    }

    delete processor;
    processor = 0;

    return 0;
}
