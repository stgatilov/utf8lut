#include <vector>
#include <random>
#include <stdint.h>
#include <locale>
#include <codecvt>

#include "Message/MessageConverter.h"
#include "BufferDecoder.h"


#define RND std::mt19937

typedef std::vector<uint8_t> Data;
enum Format { Utf8, Utf16, Utf32 };

//simple string operations
Data operator+ (const Data &a, const Data &b) {
	Data res = a;
	res.insert(res.end(), b.begin(), b.end());
	return res;
}
Data Substr(const Data &data, int start, int end = -1) {
	if (end < 0) end = data.size();
	start = std::max(std::min(start, data.size()), 0);
	end = std::max(std::min(end, data.size()), start);
	return Data(data.begin() + start, data.begin() + end);
}
Data Reverse(const Data &data) {
	Data res = data;
	std::reverse(res.begin(), res.end());
	return res;
}

class BaseGenerator {
	Format format;	
	RND &rnd;

	typedef std::uniform_int_distribution<int> Distrib;

public:
	static const int MaxCode = 0x10FFFF;
	static const int MaxBytes = 4;
	static int MaxCodeOfSize(int bytes) {
		if (bytes == 0)	return -1;
		if (bytes == 1) return 0x0000007F;
		if (bytes == 2) return 0x000007FF;
		if (bytes == 3) return 0x0000FFFF;
		if (bytes == 4) return MaxCode;
		assert(0);
	}

	DataGenerator(Format format, RND &rnd) : format(format), rnd(rnd) {}

//=============== Data generation ==================

	static void WriteWord(Data &data, uint8_t word) {
		data.push_back(code & 0xFFU);
	}
	static void WriteWord(Data &data, uint16_t word) {
		data.push_back(code & 0xFFU); word >>= 8;
		data.push_back(code & 0xFFU);
	}
	static void WriteWord(Data &data, uint32_t word) {
		data.push_back(code & 0xFFU); word >>= 8;
		data.push_back(code & 0xFFU); word >>= 8;
		data.push_back(code & 0xFFU); word >>= 8;
		data.push_back(code & 0xFFU);
	}

	void AddChar(Data &data, int code) const {
		assert(code >= 0 && code <= MaxCode);
		if (format == Utf32) {
			WriteWord(uint32_t(code));
		}
		else if (format == Utf16) {
			//from https://ru.wikipedia.org/wiki/UTF-16#.D0.9A.D0.BE.D0.B4.D0.B8.D1.80.D0.BE.D0.B2.D0.B0.D0.BD.D0.B8.D0.B5
			if (code < 0x10000)
				WriteWord(uint16_t(code));
			else {
				code = code - 0x10000;
				int lo10 = (code & 0x03FF);
				int hi10 = (code >> 10);
				WriteWord(uint16_t(0xD800 | hi10));
				WriteWord(uint16_t(0xDC00 | lo10));
			}
		}
		else if (format == Utf8) {
			//from http://stackoverflow.com/a/6240184/556899
			if (code <= 0x0000007F)				//0xxxxxxx
				WriteWord(uint8_t(0x00 | ((code >>  0) & 0x7F)));
			else if (code <= 0x000007FF) {		//110xxxxx 10xxxxxx
				WriteWord(uint8_t(0xC0 | ((code >>  6) & 0x1F)));
				WriteWord(uint8_t(0x80 | ((code >>  0) & 0x3F)));
			}
			else if (code <= 0x0000FFFF) {		//1110xxxx 10xxxxxx 10xxxxxx
				WriteWord(uint8_t(0xE0 | ((code >> 12) & 0x0F)));
				WriteWord(uint8_t(0x80 | ((code >>  6) & 0x3F)));
				WriteWord(uint8_t(0x80 | ((code >>  0) & 0x3F)));
			}
			else {								//11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
				WriteWord(uint8_t(0xF0 | ((code >> 18) & 0x07)));
				WriteWord(uint8_t(0x80 | ((code >> 12) & 0x3F)));
				WriteWord(uint8_t(0x80 | ((code >>  6) & 0x3F)));
				WriteWord(uint8_t(0x80 | ((code >>  0) & 0x3F)));
			}
		}
		else assert(0);
	}

	uint8_t RandomByte() {
		uint8_t byte = Distrib(minCode, maxCode)(rnd);
		return byte;
	}
	int RandomCode(int mask = 0) {
		if (mask == 0) mask = (1 << MaxBytes) - 1;
		assert(mask > 0 && mask < );

		int bytes;
		do {
			bytes = Distrib(1, MaxBytes)(rnd);
		} while (!(mask & (1<<bytes)));

		int minCode = MaxCodeOfSize(bytes - 1) + 1;
		int maxCode = MaxCodeOfSize(bytes);
		int code = Distrib(minCode, maxCode)(rnd);

		return code;
	}
	std::vector<uint8_t> RandomBytes(int size) {
		std:vector<uint8_t> res;
		for (int i = 0; i < size; i++)
			res += RandomByte();
		return res;
	}
	std::vector<int> RandomCodes(int size, int mask = 0) {
		std:vector<int> res;
		for (int i = 0; i < size; i++)
			res += RandomCode(mask);
		return res;
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

//=============== Data splits ==================

	//always finds a split at range [pos .. pos+3]
	//if input is correct, then split is surely between two chars
	//if input is invalid, then any split can be returned
	int FindSplit(const Data &data, int pos) const {
		pos = std::max(std::min(pos, data.size()), 0);
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
					pos += 2;	//move past surrogate's second half
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

	int RandomPos(const Data &data) {
		int pos = Distrib(0, int(data.size()));
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

	int PosByHint(int hint = -1) {
		if (hint < 0)
			hint = RandomPos(data);
		hint += Distrib(-mutRad, mutRad)(rnd);
		return hint;
	}

//============ Mutations of single data ===============

	int MutateDoubleBytes(Data &data, int hint = -1) {
		int pos = PosByHint(hint);
		int next = pos + Distrib(1, mutRad)(rnd);
		data = Substr(data, 0, pos) + Substr(data, pos, next) + Substr(data, pos, next) + Substr(data, next);
		return next;
	}
	int MutateDoubleChars(Data &data, int hint = -1) {
		int pos = FindSplit(data, PosByHint(hint));
		int next = FindSplit(data, pos + Distrib(1, mutRad)(rnd));
		data = Substr(data, 0, pos) + Substr(data, pos, next) + Substr(data, pos, next) + Substr(data, next);
		return next;
	}

	int MutateAddRandomBytes(Data &data, int hint = -1) {
		int pos = PosByHint(hint);
		data = Substr(data, 0, pos) + BytesToData(RandomBytes(Distrib(1, mutRad)(rnd))) + Substr(data, pos);
		return pos;
	}
	int MutateAddRandomChar(Data &data, int hint = -1) {
		int pos = FindSplit(data, PosByHint(hint));
		data = Substr(data, 0, pos) + CodesToData(RandomCodes(Distrib(1, mutRad)(rnd))) + Substr(data, pos);
		return pos;
	}

	int MutateRemoveRandomBytes(Data &data, int hint = -1) {
		int pos = PosByHint(hint);
		int next = pos + Distrib(1, mutRad)(rnd);
		data = Substr(data, 0, pos) + Substr(data, next);
		return pos;
	}
	int MutateRemoveRandomChars(Data &data, int hint = -1) {
		int pos = FindSplit(data, PosByHint(hint));
		int next = FindSplit(data, pos + Distrib(1, mutRad)(rnd));
		data = Substr(data, 0, pos) + Substr(data, next);
		return pos;
	}

	int MutationRevertBytes(Data &data, int hint = -1) {
		int pos = PosByHint(hint);
		int next = pos + Distrib(1, mutRad)(rnd);
		data = Substr(data, 0, pos) + Reverse(Substr(data, pos, next)) + Substr(data, next);
		return pos;
	}

	int MutationShortenEnd(Data &data, int hint = -1) {
		int num = Distrib(1, mutRad)(rnd)
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


void RunTest(const Data &data) {
	std::string input(data.begin(), data.end());
	auto converter = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{};
	bool err1 = false;
	try {
		std::u16string output = converter.from_bytes(input.data());
	}
	catch(std::exception &e) {
		err1 = true;
		//error
	}
	Data ans((char*)output.data(), (char*)(output.data() + output.size()));

	BufferDecoder<3, 2, dmValidate, 1> processor;
	long long outSize = ConvertInMemorySize(processor, data.size());
	Data res(outSize);
	auto r = ConvertInMemory(processor, data.data(), data.size(), res.data(), res.size());
	bool err2 = false;
	if (r.status)
		err2 = true;//error
	res.resize(r.outputSize);

	if (err1 != err2 || err1 == true && res != ans) {
		printf("Error!\n");
		//TODO: save
		std::terminate();
	}
}


struct BaseData {
	std::string name;
	Data data;

	BaseData(const std::string &name = "", const Data &data = Data()) : name(name), data(data) {}
};

std::vector<BaseData> testBases;

void AddBase(const Data &data, const char *format, ...) {
	va_list args;
	va_start(args, format);
	char name[256];
	vsprintf(name, format, args);
	va_end(args);
	BaseData base;
	base.data = data;
	base.name = name;
	testBases.push_back(base);
}

int main() {
	TestsGenerator gen;
	for (int i = 0; i <= 32; i++)
		for (int b = 1; b <= 4; b++)
		AddBase(gen.CodesToData(gen.RandomCodes(i, g.MaxCodeOfSize(b))), "random_codes(%d)_%d", b, i);
	for (int i = 0; i <= 32; i++)
		AddBase(gen.RandomBytes(i), "random_bytes_%d", i);

	for (int i = 0; i < testBases.size(); i++) {
		printf("%s\n", testBases[i].name.c_str());
		RunTest(testBases[i].data);
	}

    return 0;
}

