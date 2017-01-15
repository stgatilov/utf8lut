#include <vector>
#include <random>
#include <stdint.h>
#include <stdarg.h>
#include <exception>
#include <algorithm>

#include "Message/MessageConverter.h"
#include "Buffer/BufferDecoder.h"
#include "Buffer/BufferEncoder.h"


#define RND std::mt19937

typedef std::vector<uint8_t> Data;
enum Format { Utf8, Utf16, Utf32, UtfCount };

//simple string operations
Data operator+ (const Data &a, const Data &b) {
    Data res = a;
    res.insert(res.end(), b.begin(), b.end());
    return res;
}
Data Substr(const Data &data, int start, int end = -1) {
    if (end < 0) end = data.size();
    start = std::max(std::min(start, (int)data.size()), 0);
    end = std::max(std::min(end, (int)data.size()), start);
    return Data(data.begin() + start, data.begin() + end);
}
Data Reverse(const Data &data) {
    Data res = data;
    std::reverse(res.begin(), res.end());
    return res;
}

class SimpleConverter {
protected:
    Format format;  

public:
    static const int MaxCode = 0x10FFFF;
    static const int MaxBytes = 4;
    static int MaxCodeOfSize(int bytes) {
        if (bytes == 0) return -1;
        if (bytes == 1) return 0x0000007F;
        if (bytes == 2) return 0x000007FF;
        if (bytes == 3) return 0x0000FFFF;
        if (bytes == 4) return MaxCode;
        assert(0); return 0;
    }

    SimpleConverter(Format format) : format(format) {}

    static void WriteWord8(Data &data, uint8_t word) {
        data.push_back(word & 0xFFU);
    }
    static void WriteWord16(Data &data, uint16_t word) {
        data.push_back(word & 0xFFU); word >>= 8;
        data.push_back(word & 0xFFU);
    }
    static void WriteWord32(Data &data, uint32_t word) {
        data.push_back(word & 0xFFU); word >>= 8;
        data.push_back(word & 0xFFU); word >>= 8;
        data.push_back(word & 0xFFU); word >>= 8;
        data.push_back(word & 0xFFU);
    }

    void AddChar(Data &data, int code) const {
        assert(code >= 0 && code <= MaxCode);
        assert(!(code >= 0xD800U && code < 0xE000U));
        if (format == Utf32) {
            WriteWord32(data, uint32_t(code));
        }
        else if (format == Utf16) {
            //from https://ru.wikipedia.org/wiki/UTF-16#.D0.9A.D0.BE.D0.B4.D0.B8.D1.80.D0.BE.D0.B2.D0.B0.D0.BD.D0.B8.D0.B5
            if (code < 0x10000)
                WriteWord16(data, uint16_t(code));
            else {
                code = code - 0x10000;
                int lo10 = (code & 0x03FF);
                int hi10 = (code >> 10);
                WriteWord16(data, uint16_t(0xD800 | hi10));
                WriteWord16(data, uint16_t(0xDC00 | lo10));
            }
        }
        else if (format == Utf8) {
            //from http://stackoverflow.com/a/6240184/556899
            if (code <= 0x0000007F)             //0xxxxxxx
                WriteWord8(data, uint8_t(0x00 | ((code >>  0) & 0x7F)));
            else if (code <= 0x000007FF) {      //110xxxxx 10xxxxxx
                WriteWord8(data, uint8_t(0xC0 | ((code >>  6) & 0x1F)));
                WriteWord8(data, uint8_t(0x80 | ((code >>  0) & 0x3F)));
            }
            else if (code <= 0x0000FFFF) {      //1110xxxx 10xxxxxx 10xxxxxx
                WriteWord8(data, uint8_t(0xE0 | ((code >> 12) & 0x0F)));
                WriteWord8(data, uint8_t(0x80 | ((code >>  6) & 0x3F)));
                WriteWord8(data, uint8_t(0x80 | ((code >>  0) & 0x3F)));
            }
            else {                              //11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
                WriteWord8(data, uint8_t(0xF0 | ((code >> 18) & 0x07)));
                WriteWord8(data, uint8_t(0x80 | ((code >> 12) & 0x3F)));
                WriteWord8(data, uint8_t(0x80 | ((code >>  6) & 0x3F)));
                WriteWord8(data, uint8_t(0x80 | ((code >>  0) & 0x3F)));
            }
        }
        else assert(0);
    }

    static int ParseWord8(const Data &data, const uint8_t *&ptr) {
        if (ptr + 1 > data.data() + data.size())
            throw std::runtime_error("Cannot read a byte");
        uint32_t a = (*ptr++);
        return a;
    }
    static int ParseWord16(const Data &data, const uint8_t *&ptr) {
        if (ptr + 2 > data.data() + data.size())
            throw std::runtime_error("Cannot read a 16-bit word");
        uint32_t a = (*ptr++);
        uint32_t b = (*ptr++);
        return a ^ (b << 8);
    }
    static int ParseWord32(const Data &data, const uint8_t *&ptr) {
        if (ptr + 4 > data.data() + data.size())
            throw std::runtime_error("Cannot read a 32-bit word");
        uint32_t a = (*ptr++);
        uint32_t b = (*ptr++);
        uint32_t c = (*ptr++);
        uint32_t d = (*ptr++);
        return a ^ (b << 8) ^ (c << 16) ^ (c << 24);
    }

    int ParseChar(const Data &data, const uint8_t *&ptr) const {
        uint32_t code = (uint32_t)-1;
        if (format == Utf32) {
            code = ParseWord32(data, ptr);
        }
        else if (format == Utf16) {
            code = ParseWord16(data, ptr);
            if (code >= 0xD800U && code < 0xDC00U) {
                uint32_t rem = ParseWord16(data, ptr);
                if (!(rem >= 0xDC00U && rem < 0xE000U))
                    throw std::runtime_error("Second part of surrogate pair is out of range");
                code = ((code - 0xD800U) << 10) + (rem - 0xDC00U) + 0x010000;
            }
        }
        else if (format == Utf8) {
            code = ParseWord8(data, ptr);
            int cnt;
            if ((code & 0x80U) == 0x00U)            //0xxxxxxx
                cnt = 0;
            else if ((code & 0xE0U) == 0xC0U) {     //110xxxxx 10xxxxxx
                cnt = 1;
                code -= 0xC0U;
            }
            else if ((code & 0xF0U) == 0xE0U) {     //1110xxxx 10xxxxxx 10xxxxxx
                cnt = 2;
                code -= 0xE0U;
            }
            else if ((code & 0xF8U) == 0xF0U) {     //11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
                cnt = 3;
                code -= 0xF0U;
            }
            else
                throw std::runtime_error("Failed to parse leading byte");

            for (int t = 0; t < cnt; t++) {
                uint32_t add = ParseWord8(data, ptr);
                if ((add & 0xC0U) != 0x80U)
                    throw std::runtime_error("Continuation byte out of range");
                code = (code << 6) ^ (add - 0x80U);
            }
            if (int(code) <= MaxCodeOfSize(cnt))
                throw std::runtime_error("Overlong encoding");
        }
        else assert(0);

        if (code >= 0xD800U && code < 0xE000U)
            throw std::runtime_error("Code point in surrogate range");
        if (code > MaxCode)
            throw std::runtime_error("Code point too large");

        return code;
    }

    Data ByteToData(uint8_t byte) const {
        return Data(1, byte);
    }
    Data CodeToData(int code) const {
        Data res;
        AddChar(res, code);
        return res;
    }
    template<typename BytesContainer>
    Data BytesToData(BytesContainer bytes) {
        Data res;
        for (auto it = std::begin(bytes); it != std::end(bytes); it++) {
            uint8_t byte = *it;
            res.push_back(byte);
        }
        return res;
    }
    template<typename CodesContainer>
    Data CodesToData(CodesContainer codes) {
        Data res;
        for (auto it = std::begin(codes); it != std::end(codes); it++) {
            int code = *it;
            AddChar(res, code);
        }
        return res;
    }

    std::vector<int> ParseCodes(const Data &data) {
        const uint8_t *ptr = data.data();
        std::vector<int> res;
        while (ptr < data.data() + data.size()) {
            int code = ParseChar(data, ptr);
            res.push_back(code);
        }
        return res;
    }

    //always finds a split at range [pos .. pos+3]
    //if input is correct, then split is surely between two chars
    //if input is invalid, then any split can be returned
    int FindSplit(const Data &data, int pos) const {
        pos = std::max(std::min(pos, (int)data.size()), 0);
        if (format == Utf32) {
            //align with 32-bit words
            while (pos & 3) pos++;
        }
        else if (format == Utf16) {
            //align with 16-bit words
            while (pos & 1) pos++;
            //check if in the middle of surrogate pair
            if (pos + 1 < data.size()) {
                uint16_t val = data[pos] + (data[pos+1] << 8);
                if (val >= 0xDC00U && val < 0xE000U)
                    pos += 2;   //move past surrogate's second half
            }
        }
        else if (format == Utf8) {
            //move past continuation bytes (at most 3 of them)
            for (int i = 0; i < 3; i++)
                if (pos < data.size() && data[pos] >= 0x80U && data[pos] < 0xC0U)
                    pos++;
        }
        pos = std::min(pos, int(data.size()));
        return pos;
    }
};


class BaseGenerator : public SimpleConverter {
protected:
    RND &rnd;

    typedef std::uniform_int_distribution<int> Distrib;

public:
    BaseGenerator(Format format, RND &rnd) : SimpleConverter(format), rnd(rnd) {}

//=============== Data generation ==================

    uint8_t RandomByte() {
        uint8_t byte = Distrib(0, 255)(rnd);
        return byte;
    }
    int RandomCode(int mask = 0) {
        if (mask == 0) mask = (1 << MaxBytes) - 1;
        assert(mask > 0 && mask < (1 << MaxBytes));

        int bytes;
        do {
            bytes = Distrib(1, MaxBytes)(rnd);
        } while (!(mask & (1 << (bytes-1))));

        int minCode = MaxCodeOfSize(bytes - 1) + 1;
        int maxCode = MaxCodeOfSize(bytes);
        int code;
        do {
            code = Distrib(minCode, maxCode)(rnd);
        } while (code >= 0xD800U && code < 0xE000U);

        return code;
    }
    std::vector<uint8_t> RandomBytes(int size) {
        std::vector<uint8_t> res;
        for (int i = 0; i < size; i++)
            res.push_back(RandomByte());
        return res;
    }
    std::vector<int> RandomCodes(int size, int mask = 0) {
        std::vector<int> res;
        for (int i = 0; i < size; i++)
            res.push_back(RandomCode(mask));
        return res;
    }

    int RandomPos(const Data &data) {
        int pos = Distrib(0, int(data.size()))(rnd);
        return pos;
    }
    int RandomSplit(const Data &data) {
        int pos = RandomPos(data);
        pos = FindSplit(data, pos);
        return pos;
    }
};


class TestsGenerator : public BaseGenerator {
    int mutRad;

public:
    TestsGenerator (Format format, RND &rnd) : BaseGenerator(format, rnd) {
        SetMutationRadius();
    }

    //how close mutations are to each other (if there are several of them)
    void SetMutationRadius(int rad = 10) {
        mutRad = rad;
    }

    int PosByHint(const Data &data, int hint = -1) {
        if (hint < 0)
            hint = RandomPos(data);
        hint += Distrib(-mutRad, mutRad)(rnd);
        return hint;
    }

//============ Mutations of single data ===============

    int MutateDoubleBytes(Data &data, int hint = -1) {
        int pos = PosByHint(data, hint);
        int next = pos + Distrib(1, mutRad)(rnd);
        data = Substr(data, 0, pos) + Substr(data, pos, next) + Substr(data, pos, next) + Substr(data, next);
        return next;
    }
    int MutateDoubleChars(Data &data, int hint = -1) {
        int pos = FindSplit(data, PosByHint(data, hint));
        int next = FindSplit(data, pos + Distrib(1, mutRad)(rnd));
        data = Substr(data, 0, pos) + Substr(data, pos, next) + Substr(data, pos, next) + Substr(data, next);
        return next;
    }

    int MutateAddRandomBytes(Data &data, int hint = -1) {
        int pos = PosByHint(data, hint);
        data = Substr(data, 0, pos) + BytesToData(RandomBytes(Distrib(1, mutRad)(rnd))) + Substr(data, pos);
        return pos;
    }
    int MutateAddRandomChar(Data &data, int hint = -1) {
        int pos = FindSplit(data, PosByHint(data, hint));
        data = Substr(data, 0, pos) + CodesToData(RandomCodes(Distrib(1, mutRad)(rnd))) + Substr(data, pos);
        return pos;
    }

    int MutateRemoveRandomBytes(Data &data, int hint = -1) {
        int pos = PosByHint(data, hint);
        int next = pos + Distrib(1, mutRad)(rnd);
        data = Substr(data, 0, pos) + Substr(data, next);
        return pos;
    }
    int MutateRemoveRandomChars(Data &data, int hint = -1) {
        int pos = FindSplit(data, PosByHint(data, hint));
        int next = FindSplit(data, pos + Distrib(1, mutRad)(rnd));
        data = Substr(data, 0, pos) + Substr(data, next);
        return pos;
    }

    int MutationRevertBytes(Data &data, int hint = -1) {
        int pos = PosByHint(data, hint);
        int next = pos + Distrib(1, mutRad)(rnd);
        data = Substr(data, 0, pos) + Reverse(Substr(data, pos, next)) + Substr(data, next);
        return pos;
    }

    int MutationShortenEnd(Data &data, int hint = -1) {
        int num = Distrib(1, mutRad)(rnd);
        if (Distrib(0, 1)(rnd))
            data = Substr(data, 0, data.size() - num);
        else 
            data = Substr(data, num);
        return hint;
    }

//============ Mixes of several data ================

    static Data MixConcatenate(const Data &a, const Data &b) {
        return a + b;
    }
    Data MixOneInsideOther(const Data &a, const Data &b) {
        int pos = RandomPos(a);
        return Substr(a, 0, pos) + b + Substr(a, pos);
    }
    Data MixInterleaveBytes(const Data &a, const Data &b) {
        Data res;
        int pa = 0, pb = 0;
        while (pa < a.size() || pb < b.size()) {
            int q = Distrib(0, 1)(rnd);
            if (q == 0 && pa < a.size())
                res.push_back(a[pa++]);
            if (q == 1 && pb < b.size())
                res.push_back(b[pb++]);
        }
        return res;
    }

};


std::unique_ptr<Data> SimpleConvert(const Data &data, Format from, Format to) {
    SimpleConverter Usrc(from), Udst(to);
    try {
        std::vector<int> codes = Usrc.ParseCodes(data);
        Data answer = Udst.CodesToData(codes);
        return std::unique_ptr<Data>(new Data(std::move(answer)));
    }
    catch (std::exception &e) {
        return nullptr;
    }
}

std::unique_ptr<BaseBufferProcessor> GenerateConverter(Format srcFormat, Format dstFormat) {
    std::unique_ptr<BaseBufferProcessor> res;
    if (dstFormat == Utf8 && srcFormat == Utf16)
        res.reset(new BufferEncoder<3, 2, emValidate, 1>());
    else if (dstFormat == Utf8 && srcFormat == Utf32)
        res.reset(new BufferEncoder<3, 4, emValidate, 1>());
    else if (dstFormat == Utf16 && srcFormat == Utf8)
        res.reset(new BufferDecoder<3, 2, dmValidate, 1>());
    else if (dstFormat == Utf32 && srcFormat == Utf8)
        res.reset(new BufferDecoder<3, 4, dmValidate, 1>());
    return res;
}

//TODO: template arguments?
std::unique_ptr<Data> TestedConvert(const Data &data, Format from, Format to) {
    auto processor = GenerateConverter(from, to);
        
    long long outMaxSize = ConvertInMemorySize(*processor, data.size());
    Data answer(outMaxSize);
    auto res = ConvertInMemory(*processor, (const char*)data.data(), data.size(), (char*)answer.data(), answer.size());
    if (res.status)
        return nullptr;
    answer.resize(res.outputSize);

    return std::unique_ptr<Data>(new Data(std::move(answer)));
}

void CheckResults(const std::unique_ptr<Data> &ans, const std::unique_ptr<Data> &out) {
    if (bool(ans) != bool(out) || bool(ans) && *ans != *out) {
        printf("Error!\n");
        //TODO: save
        std::terminate();
    }
}

void RunTest(const Data &data, const std::string &name) {
    std::string str(data.begin(), data.end());
    uint32_t hash = std::hash<std::string>()(str);
    printf("%s (%08X): ", name.c_str(), hash);
    Format dirs[4][2] = {
        {Utf8, Utf16},
        {Utf8, Utf32},
        {Utf16, Utf8},
        {Utf32, Utf8}
    };
    for (int d = 0; d < 4; d++) {
        Format from = dirs[d][0], to = dirs[d][1];
        auto ans = SimpleConvert(data, from, to);
        auto res = TestedConvert(data, from, to);
        printf(" %c%c", (ans ? '.' : 'x'), (res ? '.' : 'x'));
        CheckResults(ans, res);
    }
    printf("\n");
}

void RunTestF(const Data &data, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char name[256];
    vsprintf(name, format, args);
    va_end(args);
    RunTest(data, std::string(name));
}


int main() {
    RND rnd;
    for (int fmt = 0; fmt < UtfCount; fmt++) {
        TestsGenerator gen(Format(fmt), rnd);

        RunTestF(Data(), "empty");
        for (int i = 1; i <= 32; i++)
            for (int b = 1; b < 16; b++)
                RunTestF(gen.CodesToData(gen.RandomCodes(i, b)), "random_codes(%d)_%d", b, i);
        for (int i = 1; i <= 32; i++)
            RunTestF(gen.RandomBytes(i), "random_bytes_%d", i);
    }

    return 0;
}

