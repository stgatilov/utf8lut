
default: test

all: utf8fuzz

test: utf8fuzz runtest

runtest:
	./utf8fuzz -s -vv

OPTFLAGS += -O2

CXX = g++
CXXFLAGS_NO_WERROR = $(OPTFLAGS) -g -Wall -std=gnu++11 -msse4.1 -Icodecs/utf8lut
CXXFLAGS = $(CXXFLAGS_NO_WERROR)
CC = gcc
CFLAGS_NO_WERROR = $(OPTFLAGS) -g -Wall
CFLAGS = $(CFLAGS_NO_WERROR)
LDFLAGS = -g

OBJS = utf8fuzz.o validator.o verbosity.o aligned_alloc.o stangvik.o postgresql.o u8u16.o pg_wchar.o af_with_verbose_hack.o testset_standard.o testset_random.o testset_files.o DecoderLut.o EncoderLut.o BaseBufferProcessor.o

utf8fuzz: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS)

utf8fuzz.o: utf8fuzz.cpp validator.h verbosity.h codecs/AfUtf8/memory.h aligned_alloc.h testset_random.h testset_standard.h testset_files.h 

validator.o: validator.cpp validator.h codecs/AfUtf8/memory.h verbosity.h codecs/AfUtf8/memory.h codecs/stangvik.h codecs/postgresql.h codecs/AfUtf8/AfUtf8.h codecs/u8u16.h

testset_standard.o: testset_standard.cpp testset_standard.h testset.h kuhn_extracted.h

testset_files.o: testset_files.cpp testset_files.h testset.h

testset_random.o: testset_random.cpp testset_random.h testset.h

stangvik.o: codecs/stangvik.cpp codecs/stangvik.h
	$(CXX) $(CXXFLAGS) -o $@ -c $<

postgresql.o: codecs/postgresql.c codecs/postgresql.h codecs/pg_wchar.h
	$(CC) $(CFLAGS) -o $@ -c $<

pg_wchar.o: codecs/pg_wchar.c codecs/pg_wchar.h
	$(CC) $(CFLAGS) -o $@ -c $<

u8u16.o: codecs/u8u16.cpp codecs/u8u16.h
	$(CXX) $(CXXFLAGS_NO_WERROR) -o $@ -c $<

af_with_verbose_hack.o: codecs/af_with_verbose_hack.cpp codecs/AfUtf8/AfUtf8.cpp codecs/AfUtf8/AfUtf8.h
	$(CXX) $(CXXFLAGS) -o $@ -c $<

kuhn_extracted.h: contrib/convert-kuhn-tests.pl contrib/UTF-8-test.txt
	perl contrib/convert-kuhn-tests.pl < contrib/UTF-8-test.txt > $@

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

clean:
	rm -f utf8fuzz $(OBJS) kuhn_extracted.h

DecoderLut.o: codecs/utf8lut/core/DecoderLut.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

EncoderLut.o: codecs/utf8lut/core/EncoderLut.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

BaseBufferProcessor.o: codecs/utf8lut/buffer/BaseBufferProcessor.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

