/* 
 * QR Code generator test suite (C)
 * 
 * When compiling this program, the library qrcodegen.c needs QRCODEGEN_TEST
 * to be defined. Run this command line program with no arguments.
 * 
 * Copyright (c) Project Nayuki. (MIT License)
 * https://www.nayuki.io/page/qr-code-generator-library
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * - The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * - The Software is provided "as is", without warranty of any kind, express or
 *   implied, including but not limited to the warranties of merchantability,
 *   fitness for a particular purpose and noninfringement. In no event shall the
 *   authors or copyright holders be liable for any claim, damages or other
 *   liability, whether in an action of contract, tort or otherwise, arising from,
 *   out of or in connection with the Software or the use or other dealings in the
 *   Software.
 */

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "qrcodegen.h"

#define ARRAY_LENGTH(name)  (sizeof(name) / sizeof(name[0]))


// Global variables
static int numTestCases = 0;


// Prototypes of private functions under test
extern const int8_t ECC_CODEWORDS_PER_BLOCK[4][41];
extern const int8_t NUM_ERROR_CORRECTION_BLOCKS[4][41];
int getTextProperties(const char *text, bool *isNumeric, bool *isAlphanumeric, int *textBits);
void appendBitsToBuffer(unsigned int val, int numBits, uint8_t buffer[], int *bitLen);
void appendErrorCorrection(uint8_t data[], int version, enum qrcodegen_Ecc ecl, uint8_t result[]);
int getNumDataCodewords(int version, enum qrcodegen_Ecc ecl);
int getNumRawDataModules(int version);
void calcReedSolomonGenerator(int degree, uint8_t result[]);
void calcReedSolomonRemainder(const uint8_t data[], int dataLen, const uint8_t generator[], int degree, uint8_t result[]);
uint8_t finiteFieldMultiply(uint8_t x, uint8_t y);
void initializeFunctionModules(int version, uint8_t qrcode[]);
int getAlignmentPatternPositions(int version, uint8_t result[7]);
bool getModule(const uint8_t qrcode[], int x, int y);
void setModule(uint8_t qrcode[], int x, int y, bool isBlack);
void setModuleBounded(uint8_t qrcode[], int x, int y, bool isBlack);


/*---- Test cases ----*/

static void testGetTextProperties(void) {
	bool isNumeric, isAlphanumeric;
	int textLen, textBits;
	
	textLen = getTextProperties("", &isNumeric, &isAlphanumeric, &textBits);
	assert(textLen == 0 && isNumeric && isAlphanumeric && textBits == 0);
	numTestCases++;
	
	textLen = getTextProperties("0", &isNumeric, &isAlphanumeric, &textBits);
	assert(textLen == 1 && isNumeric && isAlphanumeric && textBits == 4);
	numTestCases++;
	
	textLen = getTextProperties("768", &isNumeric, &isAlphanumeric, &textBits);
	assert(textLen == 3 && isNumeric && isAlphanumeric && textBits == 10);
	numTestCases++;
	
	textLen = getTextProperties("A1", &isNumeric, &isAlphanumeric, &textBits);
	assert(textLen == 2 && !isNumeric && isAlphanumeric && textBits == 11);
	numTestCases++;
	
	textLen = getTextProperties("THE: QUICK+/*BROWN$FOX.", &isNumeric, &isAlphanumeric, &textBits);
	assert(textLen == 23 && !isNumeric && isAlphanumeric && textBits == 127);
	numTestCases++;
	
	textLen = getTextProperties("aB 9", &isNumeric, &isAlphanumeric, &textBits);
	assert(textLen == 4 && !isNumeric && !isAlphanumeric && textBits == 32);
	numTestCases++;
	
	char text[32769];
	
	memset(text, '5', sizeof(text));
	text[32768] = '\0';
	textLen = getTextProperties(text, &isNumeric, &isAlphanumeric, &textBits);
	assert(textLen < 0);
	numTestCases++;
	
	memset(text, '1', sizeof(text));
	text[32767] = '\0';
	textLen = getTextProperties(text, &isNumeric, &isAlphanumeric, &textBits);
	assert((109224L > INT_MAX && textLen < 0) ||
		(109224L <= INT_MAX && textLen == 32767 && isNumeric && isAlphanumeric && textBits == 109224L));
	numTestCases++;
	
	memset(text, 'a', sizeof(text));
	text[4095] = '\0';
	textLen = getTextProperties(text, &isNumeric, &isAlphanumeric, &textBits);
	assert(textLen == 4095 && !isNumeric && !isAlphanumeric && textBits == 32760);
	numTestCases++;
	
	memset(text, 'a', sizeof(text));
	text[32767] = '\0';
	textLen = getTextProperties(text, &isNumeric, &isAlphanumeric, &textBits);
	assert((262136L > INT_MAX && textLen < 0) ||
		(262136L <= INT_MAX && textLen == 32767 && !isNumeric && !isAlphanumeric && textBits == 262136L));
	numTestCases++;
}


static void testAppendBitsToBuffer(void) {
	{
		uint8_t buf[1] = {0};
		int bitLen = 0;
		appendBitsToBuffer(0, 0, buf, &bitLen);
		assert(bitLen == 0);
		assert(buf[0] == 0);
		appendBitsToBuffer(1, 1, buf, &bitLen);
		assert(bitLen == 1);
		assert(buf[0] == 0x80);
		appendBitsToBuffer(0, 1, buf, &bitLen);
		assert(bitLen == 2);
		assert(buf[0] == 0x80);
		appendBitsToBuffer(5, 3, buf, &bitLen);
		assert(bitLen == 5);
		assert(buf[0] == 0xA8);
		appendBitsToBuffer(6, 3, buf, &bitLen);
		assert(bitLen == 8);
		assert(buf[0] == 0xAE);
		numTestCases++;
	}
	{
		uint8_t buf[6] = {0};
		int bitLen = 0;
		appendBitsToBuffer(16942, 16, buf, &bitLen);
		assert(bitLen == 16);
		assert(buf[0] == 0x42 && buf[1] == 0x2E && buf[2] == 0x00 && buf[3] == 0x00 && buf[4] == 0x00 && buf[5] == 0x00);
		appendBitsToBuffer(10, 7, buf, &bitLen);
		assert(bitLen == 23);
		assert(buf[0] == 0x42 && buf[1] == 0x2E && buf[2] == 0x14 && buf[3] == 0x00 && buf[4] == 0x00 && buf[5] == 0x00);
		appendBitsToBuffer(15, 4, buf, &bitLen);
		assert(bitLen == 27);
		assert(buf[0] == 0x42 && buf[1] == 0x2E && buf[2] == 0x15 && buf[3] == 0xE0 && buf[4] == 0x00 && buf[5] == 0x00);
		appendBitsToBuffer(26664, 15, buf, &bitLen);
		assert(bitLen == 42);
		assert(buf[0] == 0x42 && buf[1] == 0x2E && buf[2] == 0x15 && buf[3] == 0xFA && buf[4] == 0x0A && buf[5] == 0x00);
		numTestCases++;
	}
}


// Ported from the Java version of the code.
static uint8_t *appendErrorCorrectionReference(const uint8_t *data, int version, enum qrcodegen_Ecc ecl) {
	// Calculate parameter numbers
	int numBlocks = NUM_ERROR_CORRECTION_BLOCKS[(int)ecl][version];
	int blockEccLen = ECC_CODEWORDS_PER_BLOCK[(int)ecl][version];
	int rawCodewords = getNumRawDataModules(version) / 8;
	int numShortBlocks = numBlocks - rawCodewords % numBlocks;
	int shortBlockLen = rawCodewords / numBlocks;
	
	// Split data into blocks and append ECC to each block
	uint8_t **blocks = malloc(numBlocks * sizeof(uint8_t*));
	uint8_t *generator = malloc(blockEccLen * sizeof(uint8_t));
	calcReedSolomonGenerator(blockEccLen, generator);
	for (int i = 0, k = 0; i < numBlocks; i++) {
		uint8_t *block = malloc((shortBlockLen + 1) * sizeof(uint8_t));
		int blockDataLen = shortBlockLen - blockEccLen + (i < numShortBlocks ? 0 : 1);
		memcpy(block, &data[k], blockDataLen * sizeof(uint8_t));
		calcReedSolomonRemainder(&data[k], blockDataLen, generator, blockEccLen, &block[shortBlockLen + 1 - blockEccLen]);
		k += blockDataLen;
		blocks[i] = block;
	}
	free(generator);
	
	// Interleave (not concatenate) the bytes from every block into a single sequence
	uint8_t *result = malloc(rawCodewords * sizeof(uint8_t));
	for (int i = 0, k = 0; i < shortBlockLen + 1; i++) {
		for (int j = 0; j < numBlocks; j++) {
			// Skip the padding byte in short blocks
			if (i != shortBlockLen - blockEccLen || j >= numShortBlocks) {
				result[k] = blocks[j][i];
				k++;
			}
		}
	}
	for (int i = 0; i < numBlocks; i++)
		free(blocks[i]);
	free(blocks);
	return result;
}


static void testAppendErrorCorrection(void) {
	for (int version = 1; version <= 40; version++) {
		for (int ecl = 0; ecl < 4; ecl++) {
			int dataLen = getNumDataCodewords(version, (enum qrcodegen_Ecc)ecl);
			uint8_t *pureData = malloc(dataLen * sizeof(uint8_t));
			for (int i = 0; i < dataLen; i++)
				pureData[i] = rand() % 256;
			uint8_t *expectOutput = appendErrorCorrectionReference(pureData, version, (enum qrcodegen_Ecc)ecl);
			
			int dataAndEccLen = getNumRawDataModules(version) / 8;
			uint8_t *paddedData = malloc(dataAndEccLen * sizeof(uint8_t));
			memcpy(paddedData, pureData, dataLen * sizeof(uint8_t));
			uint8_t *actualOutput = malloc(dataAndEccLen * sizeof(uint8_t));
			appendErrorCorrection(paddedData, version, (enum qrcodegen_Ecc)ecl, actualOutput);
			
			assert(memcmp(actualOutput, expectOutput, dataAndEccLen * sizeof(uint8_t)) == 0);
			free(pureData);
			free(expectOutput);
			free(paddedData);
			free(actualOutput);
			numTestCases++;
		}
	}
}


static void testGetNumDataCodewords(void) {
	int cases[][3] = {
		{ 3, 1,   44},
		{ 3, 2,   34},
		{ 3, 3,   26},
		{ 6, 0,  136},
		{ 7, 0,  156},
		{ 9, 0,  232},
		{ 9, 1,  182},
		{12, 3,  158},
		{15, 0,  523},
		{16, 2,  325},
		{19, 3,  341},
		{21, 0,  932},
		{22, 0, 1006},
		{22, 1,  782},
		{22, 3,  442},
		{24, 0, 1174},
		{24, 3,  514},
		{28, 0, 1531},
		{30, 3,  745},
		{32, 3,  845},
		{33, 0, 2071},
		{33, 3,  901},
		{35, 0, 2306},
		{35, 1, 1812},
		{35, 2, 1286},
		{36, 3, 1054},
		{37, 3, 1096},
		{39, 1, 2216},
		{40, 1, 2334},
	};
	for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
		int *tc = cases[i];
		assert(getNumDataCodewords(tc[0], (enum qrcodegen_Ecc)tc[1]) == tc[2]);
		numTestCases++;
	}
}


static void testGetNumRawDataModules(void) {
	int cases[][2] = {
		{ 1,   208},
		{ 2,   359},
		{ 3,   567},
		{ 6,  1383},
		{ 7,  1568},
		{12,  3728},
		{15,  5243},
		{18,  7211},
		{22, 10068},
		{26, 13652},
		{32, 19723},
		{37, 25568},
		{40, 29648},
	};
	for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
		int *tc = cases[i];
		assert(getNumRawDataModules(tc[0]) == tc[1]);
		numTestCases++;
	}
}


static void testCalcReedSolomonGenerator(void) {
	uint8_t generator[30];
	
	calcReedSolomonGenerator(1, generator);
	assert(generator[0] == 0x01);
	numTestCases++;
	
	calcReedSolomonGenerator(2, generator);
	assert(generator[0] == 0x03);
	assert(generator[1] == 0x02);
	numTestCases++;
	
	calcReedSolomonGenerator(5, generator);
	assert(generator[0] == 0x1F);
	assert(generator[1] == 0xC6);
	assert(generator[2] == 0x3F);
	assert(generator[3] == 0x93);
	assert(generator[4] == 0x74);
	numTestCases++;
	
	calcReedSolomonGenerator(30, generator);
	assert(generator[ 0] == 0xD4);
	assert(generator[ 1] == 0xF6);
	assert(generator[ 5] == 0xC0);
	assert(generator[12] == 0x16);
	assert(generator[13] == 0xD9);
	assert(generator[20] == 0x12);
	assert(generator[27] == 0x6A);
	assert(generator[29] == 0x96);
	numTestCases++;
}


static void testCalcReedSolomonRemainder(void) {
	{
		uint8_t data[1];
		uint8_t generator[3];
		uint8_t remainder[ARRAY_LENGTH(generator)];
		calcReedSolomonGenerator(ARRAY_LENGTH(generator), generator);
		calcReedSolomonRemainder(data, 0, generator, ARRAY_LENGTH(generator), remainder);
		assert(remainder[0] == 0);
		assert(remainder[1] == 0);
		assert(remainder[2] == 0);
		numTestCases++;
	}
	{
		uint8_t data[2] = {0, 1};
		uint8_t generator[4];
		uint8_t remainder[ARRAY_LENGTH(generator)];
		calcReedSolomonGenerator(ARRAY_LENGTH(generator), generator);
		calcReedSolomonRemainder(data, ARRAY_LENGTH(data), generator, ARRAY_LENGTH(generator), remainder);
		assert(remainder[0] == generator[0]);
		assert(remainder[1] == generator[1]);
		assert(remainder[2] == generator[2]);
		assert(remainder[3] == generator[3]);
		numTestCases++;
	}
	{
		uint8_t data[5] = {0x03, 0x3A, 0x60, 0x12, 0xC7};
		uint8_t generator[5];
		uint8_t remainder[ARRAY_LENGTH(generator)];
		calcReedSolomonGenerator(ARRAY_LENGTH(generator), generator);
		calcReedSolomonRemainder(data, ARRAY_LENGTH(data), generator, ARRAY_LENGTH(generator), remainder);
		assert(remainder[0] == 0xCB);
		assert(remainder[1] == 0x36);
		assert(remainder[2] == 0x16);
		assert(remainder[3] == 0xFA);
		assert(remainder[4] == 0x9D);
		numTestCases++;
	}
	{
		uint8_t data[43] = {
			0x38, 0x71, 0xDB, 0xF9, 0xD7, 0x28, 0xF6, 0x8E, 0xFE, 0x5E,
			0xE6, 0x7D, 0x7D, 0xB2, 0xA5, 0x58, 0xBC, 0x28, 0x23, 0x53,
			0x14, 0xD5, 0x61, 0xC0, 0x20, 0x6C, 0xDE, 0xDE, 0xFC, 0x79,
			0xB0, 0x8B, 0x78, 0x6B, 0x49, 0xD0, 0x1A, 0xAD, 0xF3, 0xEF,
			0x52, 0x7D, 0x9A,
		};
		uint8_t generator[30];
		uint8_t remainder[ARRAY_LENGTH(generator)];
		calcReedSolomonGenerator(ARRAY_LENGTH(generator), generator);
		calcReedSolomonRemainder(data, ARRAY_LENGTH(data), generator, ARRAY_LENGTH(generator), remainder);
		assert(remainder[ 0] == 0xCE);
		assert(remainder[ 1] == 0xF0);
		assert(remainder[ 2] == 0x31);
		assert(remainder[ 3] == 0xDE);
		assert(remainder[ 8] == 0xE1);
		assert(remainder[12] == 0xCA);
		assert(remainder[17] == 0xE3);
		assert(remainder[19] == 0x85);
		assert(remainder[20] == 0x50);
		assert(remainder[24] == 0xBE);
		assert(remainder[29] == 0xB3);
		numTestCases++;
	}
}


static void testFiniteFieldMultiply(void) {
	uint8_t cases[][3] = {
		{0x00, 0x00, 0x00},
		{0x01, 0x01, 0x01},
		{0x02, 0x02, 0x04},
		{0x00, 0x6E, 0x00},
		{0xB2, 0xDD, 0xE6},
		{0x41, 0x11, 0x25},
		{0xB0, 0x1F, 0x11},
		{0x05, 0x75, 0xBC},
		{0x52, 0xB5, 0xAE},
		{0xA8, 0x20, 0xA4},
		{0x0E, 0x44, 0x9F},
		{0xD4, 0x13, 0xA0},
		{0x31, 0x10, 0x37},
		{0x6C, 0x58, 0xCB},
		{0xB6, 0x75, 0x3E},
		{0xFF, 0xFF, 0xE2},
	};
	for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
		uint8_t *tc = cases[i];
		assert(finiteFieldMultiply(tc[0], tc[1]) == tc[2]);
		numTestCases++;
	}
}


static void testInitializeFunctionModulesEtc(void) {
	for (int ver = 1; ver <= 40; ver++) {
		uint8_t *qrcode = malloc(qrcodegen_BUFFER_LEN_FOR_VERSION(ver) * sizeof(uint8_t));
		assert(qrcode != NULL);
		initializeFunctionModules(ver, qrcode);
		
		int size = qrcodegen_getSize(qrcode);
		if (ver == 1)
			assert(size == 21);
		else if (ver == 40)
			assert(size == 177);
		else
			assert(size == ver * 4 + 17);
		
		bool hasWhite = false;
		bool hasBlack = false;
		for (int y = 0; y < size; y++) {
			for (int x = 0; x < size; x++) {
				bool color = qrcodegen_getModule(qrcode, x, y);
				if (color)
					hasBlack = true;
				else
					hasWhite = true;
			}
		}
		assert(hasWhite && hasBlack);
		free(qrcode);
		numTestCases++;
	}
}


static void testGetAlignmentPatternPositions(void) {
	int cases[][9] = {
		{ 1, 0,  -1,  -1,  -1,  -1,  -1,  -1,  -1},
		{ 2, 2,   6,  18,  -1,  -1,  -1,  -1,  -1},
		{ 3, 2,   6,  22,  -1,  -1,  -1,  -1,  -1},
		{ 6, 2,   6,  34,  -1,  -1,  -1,  -1,  -1},
		{ 7, 3,   6,  22,  38,  -1,  -1,  -1,  -1},
		{ 8, 3,   6,  24,  42,  -1,  -1,  -1,  -1},
		{16, 4,   6,  26,  50,  74,  -1,  -1,  -1},
		{25, 5,   6,  32,  58,  84, 110,  -1,  -1},
		{32, 6,   6,  34,  60,  86, 112, 138,  -1},
		{33, 6,   6,  30,  58,  86, 114, 142,  -1},
		{39, 7,   6,  26,  54,  82, 110, 138, 166},
		{40, 7,   6,  30,  58,  86, 114, 142, 170},
	};
	for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
		int *tc = cases[i];
		uint8_t pos[7];
		int num = getAlignmentPatternPositions(tc[0], pos);
		assert(num == tc[1]);
		for (int j = 0; j < num; j++)
			assert(pos[j] == tc[2 + j]);
		numTestCases++;
	}
}


static void testGetSetModule(void) {
	uint8_t qrcode[qrcodegen_BUFFER_LEN_FOR_VERSION(23)];
	initializeFunctionModules(23, qrcode);
	int size = qrcodegen_getSize(qrcode);
	
	for (int y = 0; y < size; y++) {  // Clear all to white
		for (int x = 0; x < size; x++)
			setModule(qrcode, x, y, false);
	}
	for (int y = 0; y < size; y++) {  // Check all white
		for (int x = 0; x < size; x++)
			assert(qrcodegen_getModule(qrcode, x, y) == false);
	}
	for (int y = 0; y < size; y++) {  // Set all to black
		for (int x = 0; x < size; x++)
			setModule(qrcode, x, y, true);
	}
	for (int y = 0; y < size; y++) {  // Check all black
		for (int x = 0; x < size; x++)
			assert(qrcodegen_getModule(qrcode, x, y) == true);
	}
	
	// Set some out of bounds modules to white
	setModuleBounded(qrcode, -1, -1, false);
	setModuleBounded(qrcode, -1, 0, false);
	setModuleBounded(qrcode, 0, -1, false);
	setModuleBounded(qrcode, size, 5, false);
	setModuleBounded(qrcode, 72, size, false);
	setModuleBounded(qrcode, size, size, false);
	for (int y = 0; y < size; y++) {  // Check all black
		for (int x = 0; x < size; x++)
			assert(qrcodegen_getModule(qrcode, x, y) == true);
	}
	
	// Set some modules to white
	setModule(qrcode, 3, 8, false);
	setModule(qrcode, 61, 49, false);
	for (int y = 0; y < size; y++) {  // Check most black
		for (int x = 0; x < size; x++) {
			bool white = (x == 3 && y == 8) || (x == 61 && y == 49);
			assert(qrcodegen_getModule(qrcode, x, y) != white);
		}
	}
	numTestCases++;
}


static void testGetSetModuleRandomly(void) {
	uint8_t qrcode[qrcodegen_BUFFER_LEN_FOR_VERSION(1)];
	initializeFunctionModules(1, qrcode);
	int size = qrcodegen_getSize(qrcode);
	
	bool modules[21][21];
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++)
			modules[y][x] = qrcodegen_getModule(qrcode, x, y);
	}
	
	long trials = 100000;
	for (long i = 0; i < trials; i++) {
		int x = rand() % (size * 2) - size / 2;
		int y = rand() % (size * 2) - size / 2;
		bool isInBounds = 0 <= x && x < size && 0 <= y && y < size;
		bool oldColor = isInBounds && modules[y][x];
		if (isInBounds)
			assert(getModule(qrcode, x, y) == oldColor);
		assert(qrcodegen_getModule(qrcode, x, y) == oldColor);
		
		bool newColor = rand() % 2 == 0;
		if (isInBounds)
			modules[y][x] = newColor;
		if (isInBounds && rand() % 2 == 0)
			setModule(qrcode, x, y, newColor);
		else
			setModuleBounded(qrcode, x, y, newColor);
	}
	numTestCases++;
}


/*---- Main runner ----*/

int main(void) {
	srand(time(NULL));
	testGetTextProperties();
	testAppendBitsToBuffer();
	testAppendErrorCorrection();
	testGetNumDataCodewords();
	testGetNumRawDataModules();
	testCalcReedSolomonGenerator();
	testCalcReedSolomonRemainder();
	testFiniteFieldMultiply();
	testInitializeFunctionModulesEtc();
	testGetAlignmentPatternPositions();
	testGetSetModule();
	testGetSetModuleRandomly();
	printf("All %d test cases passed\n", numTestCases);
	return EXIT_SUCCESS;
}
