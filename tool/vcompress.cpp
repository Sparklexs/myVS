/*-----------------------------------------------------------------------------
 *  vcompress.cpp - A demo code for integer encoders/decoders
 *
 *  Coding-Style: google-styleguide
 *      https://code.google.com/p/google-styleguide/
 *
 *  Authors:
 *      Takeshi Yamamuro <linguin.m.s_at_gmail.com>
 *      Fabrizio Silvestri <fabrizio.silvestri_at_isti.cnr.it>
 *      Rossano Venturini <rossano.venturini_at_isti.cnr.it>
 *
 *  Copyright 2012 Integer Encoding Library <integerencoding_at_isti.cnr.it>
 *      http://integerencoding.ist.cnr.it/
 *-----------------------------------------------------------------------------
 */

#include <integer_encoding.hpp>
#include <compress/policy/VSEncodingOP.hpp>
#include <vcompress.hpp>

#include<iostream>
#include <errno.h>
#include <omp.h>
#include<vector>
#include <thread>
#include <stdlib.h>

using namespace integer_encoding;
using namespace integer_encoding::internals;

namespace {

/* Current version embeded in a file header */
const uint32_t VC_MAGIC = 0x0f823cb4;
const uint32_t VC_MAJOR = 0;
const uint32_t VC_MINOR = 3;
const uint32_t VC_PATCHLEVEL = 0;
const uint32_t VC_VERSION = ((VC_MAJOR << 16) | (VC_MINOR << 8) | VC_PATCHLEVEL);

/* Valid options */
bool decompress_enabled = false;
bool verbose_enabled = false;
bool squeeze_enabled = false;
int encoder_id = -1;
uint64_t num_compressed = 0;
std::string input;
std::string output;

/* Extensions for encoders */
const std::string encoder_suffix[] = { ".gamma", /* N Gamma */
".gamma", /* FU Gamma */
".gamma", /* F Gamma */
".delta", /* N Delta */
".delta", /* FU Delta */
".delta", /* FG Delta */
".delta", /* F Delta */
".vb", /* Variable Byte */
".bip", /* Binary Interpolative */
".s9", /* Simple 9 */
".s16", /* Simple 16 */
".p4d", /* PForDelta */
".op4d", /* OPTPForDelta */
".vse", /* VSEncodingBlocks */
".vser", /* VSE-R */
".vsert", /* VSencodingRest */
".vseh", /* VSEncodingBlocksHybrid */
".vses", /* VSEncodingSimple */
".kafor", /* KAFOR */
".afor" /* AFOR */
};

/* For position of data */
const std::string pos_suffix = ".vc";

void show_usage() {
	fprintf(stderr, "Usage: vcompress [OPTIONS]... [ID] [FILE] [OUT]\n");
	fprintf(stderr, "Compress or uncompress FILE ");
	fprintf(stderr, "(by default, compress FILE).\n");
	fprintf(stderr, "-d, decompress\n");
	fprintf(stderr, "-l, compressor ID list\n");
	fprintf(stderr, "-n XXX, number to decompress\n");
	fprintf(stderr, "-v, verbose mode\n\n");
	fprintf(stderr, "Report bugs to <integerencoding_at_isti.cnr.it>\n\n");

	exit(1);
}

void show_ids() {
	fprintf(stderr, "The library uses following IDs:\n");
	fprintf(stderr, "ID\tName\n");
	fprintf(stderr, "---\n");
	fprintf(stderr, "0\tN Gamma\n");
	fprintf(stderr, "1\tFU Gamma\n");
	fprintf(stderr, "2\tF Gamma\n");
	fprintf(stderr, "3\tN Delta\n");
	fprintf(stderr, "4\tFU Delta\n");
	fprintf(stderr, "5\tFG Delta\n");
	fprintf(stderr, "6\tF Delta\n");
	fprintf(stderr, "7\tVariable Byte\n");
	fprintf(stderr, "8\tBinary Interpolative\n");
	fprintf(stderr, "9\tSimple 9\n");
	fprintf(stderr, "10\tSimple 16\n");
	fprintf(stderr, "11\tPForDelta\n");
	fprintf(stderr, "12\tOPTPForDelta\n");
	fprintf(stderr, "13\tVSEncodingBlocks\n");
	fprintf(stderr, "14\tVSE-R\n");
	fprintf(stderr, "15\tVSEncodingRest\n");
	fprintf(stderr, "16\tVSEncodingBlocksHybrid\n");
	fprintf(stderr, "17\tVSEncodingSimple\n");
	fprintf(stderr, "18\tKAFOR\n");
	fprintf(stderr, "19\tAFOR\n");
	fprintf(stderr, "\n");

	exit(1);
}

int parse_command(int argc, char **argv) {
	int result;
	char *end;

	/* Read input options 获取运行参数 */
	while ((result = getopt(argc, argv, "dlvhn:")) != -1) {
		switch (result) {
		case 'd': {
			decompress_enabled = true;
			break;
		}
		case 'v': {
			verbose_enabled = true;
			break;
		}
		case 'n': {
			squeeze_enabled = true;
			num_compressed = strtol(optarg, &end, 10);
			break;
		}
		case 'l': {
			show_ids();
			break;
		}
		case 'h': {
			show_usage();
			break;
		}
		case '?': {
			show_usage();
			break;
		}
		}
	}

	if (squeeze_enabled
			&& (num_compressed <= NSKIP || num_compressed >= MAXLEN))
		return 1;

	if (decompress_enabled) {
		/* Left arguments MUST be >= 1 */
		/* getopt函数会将选项及其参数放在argv最左边
		 * 非选项参数依次放在argv的最后
		 * 第一个参数是解压的文件，注意是vc文件
		 * 第二个参数（如果有的话）是输出位置
		 */

		if (argc <= optind)
			return 1;

		input = argv[optind++];
		if (argc > optind)
			output = argv[optind];

		return 0;
	}

	/* Left arguments MUST be >= 2 */
	// "encoderID path"
	//这里默认是压缩选项
	if (argc < optind + 2)
		return 1;

	/* Get a encoder ID */
	encoder_id = strtol(argv[optind++], &end, 10);
	if ((errno == ERANGE) || (*end != '\0') || (encoder_id < 0)
			|| (encoder_id >= NUMCODERS)) {
		fprintf(stderr, "Invalid EncoderID: %d\n\n", encoder_id);
		show_ids();
	}

	input = argv[optind++];

	return 0;
}

void skip_headerinfo(FILE *cmp, FILE *pos) {
	ASSERT(cmp != NULL);
	ASSERT(pos != NULL);
//	fseek(cmp, CMP_HEADER_SZ, SEEK_SET);
//	fseek(pos, POS_HEADER_SZ, SEEK_SET);
	fseeko(cmp, CMP_HEADER_SZ, SEEK_SET);
	fseeko(pos, POS_HEADER_SZ, SEEK_SET);
}

void write_headerinfo(FILE *cmp, FILE *pos) {
	ASSERT(cmp != NULL);
	ASSERT(pos != NULL);

	char buf[32];

	/* Generate a random-generated magic number */
	uint32_t rmagic = xor128();

	/* Get file size */
	/* 获取文件位置指针当前位置相对于文件首的偏移字节数 */
	uint64_t cmplen = ftell(cmp);
	uint64_t poslen = ftell(pos);

	/* Rewind the file descriptors */
	// 修改
	fseeko(cmp, 0, SEEK_SET);
	fseeko(pos, 0, SEEK_SET);
//	fseek(cmp, 0, SEEK_SET);
//	fseek(pos, 0, SEEK_SET);

	/* Write down in cmp */
	BYTEORDER_FREE_STORE32(buf, rmagic);
	fwrite(buf, 4, 1, cmp);
	fsync(cmp->_file);

	/* Write down in pos */
	BYTEORDER_FREE_STORE32(buf, encoder_id);
	BYTEORDER_FREE_STORE32(buf + 4, VC_VERSION);
	BYTEORDER_FREE_STORE32(buf + 8, VC_MAGIC);
	BYTEORDER_FREE_STORE32(buf + 12, rmagic);
	BYTEORDER_FREE_STORE64(buf + 16, cmplen);
	BYTEORDER_FREE_STORE64(buf + 24, poslen);
	fwrite(buf, 32, 1, pos);
	fsync(pos->_file);
}

void write_pos_entry(uint32_t num, uint32_t prev, uint64_t pos, FILE *out) {
	ASSERT(out != NULL);
	char buf[16];
	BYTEORDER_FREE_STORE64(buf, pos);
	BYTEORDER_FREE_STORE32(buf + 8, num);
	BYTEORDER_FREE_STORE32(buf + 12, prev);
	fwrite(buf, 16, 1, out);
}

void write_pos_entry(uint64_t pos, FILE *out) {
	ASSERT(out != NULL);
	char buf[8];
	BYTEORDER_FREE_STORE64(buf, pos);
	fwrite(buf, 8, 1, out);
}

void validate_encoder_id(uint32_t **pos) {
	encoder_id = VC_LOAD32(*pos);
	if ((encoder_id < 0) || (encoder_id >= NUMCODERS))
		OUTPUT_AND_DIE("File format exception: encoder ID");
}

void validate_headerinfo(uint32_t **cmp, uint64_t cmplen, uint32_t **pos,
		uint64_t poslen) {
	/* Check VC_VERSION and VC_MAGIC */
	uint32_t version = VC_LOAD32(*pos);
	uint32_t magic = VC_LOAD32(*pos);

	if (magic != VC_MAGIC || version != VC_VERSION)
		OUTPUT_AND_DIE("File format exception: version");

	/* Check a correct pair of compressed data */
	uint32_t rmagic = VC_LOAD32(*pos);
	uint32_t cmp_rmagic = VC_LOAD32(*cmp);

	if (rmagic != cmp_rmagic)
		OUTPUT_AND_DIE("File format exception: file");

	/* Check a length of input files */
	uint64_t cmpvlen = VC_LOAD64(*pos);
	uint64_t posvlen = VC_LOAD64(*pos);

	if (cmplen != cmpvlen || poslen != posvlen)
		OUTPUT_AND_DIE("File format exception: file length");
}

void do_compress(const std::string& input, int id) {
	/****************************************************************/
	//          compress posting lists using multi-thread           //
	/****************************************************************/
//	{
//		// 获取每个倒排链的入口，存入vector之中
//		uint64_t len = 0;
//		uint32_t *addr = OpenFile(input, &len);
//		uint32_t *term = addr + (len >> 2);
//		std::vector<uint32_t*> vec;
//		while (addr < term) {
//			vec.push_back(addr);
//			uint32_t num = VC_LOAD32(addr);
//			addr += num;
//		}
//
//		// 准备最后的输出文件: .vser, .vc
//		FILE *cmp = fopen(("./share/synthesis" + encoder_suffix[id]).c_str(),
//				"w");
//		FILE *pos = fopen(("./share/synthesis" + pos_suffix).c_str(), "w");
//		skip_headerinfo(cmp, pos);
//
//		// 执行过程中所需要的变量
//		uint32_t numUsedForFileName = 0;
//		uint32_t filesMerged = 0;			// 对应于filesGenerated
//		uint32_t filesGenerated = 0;
//		uint64_t posInPos = 0;
//		char buffer[50];
//		uint64_t lengthInvalid = 0;
//		uint32_t *cmptmp, *postmp;
//
//		uint32_t const MERGE_THRESHOLD = 10000;
//		uint32_t loopTimes = (vec.size() + MERGE_THRESHOLD - 1)
//				/ MERGE_THRESHOLD;
//
//		for (uint32_t iOutter = 0; iOutter < loopTimes; iOutter++) {
//			uint32_t currThres =
//					iOutter == loopTimes - 1 ?
//							vec.size() - MERGE_THRESHOLD * (loopTimes - 1) :
//							MERGE_THRESHOLD;
//#pragma omp parallel for shared(vec)
//			for (uint32_t i = 0; i < currThres; i++) {
////				std::cout << "for loop(" << omp_get_thread_num() << "): " << i
////						<< std::endl;
//				uint32_t currFileNum = i + iOutter * MERGE_THRESHOLD;
//				uint32_t num = VC_LOAD32(vec.at(currFileNum));
//				if (LIKELY(num > NSKIP && num < MAXLEN)) {
//
//					char buffer[50];
//					filesGenerated++;
//					sprintf(buffer, "%d", currFileNum);
//
//					FILE *cmp = fopen(
//							(input + buffer + encoder_suffix[id]).c_str(), "w");
//					FILE *pos = fopen((input + buffer + pos_suffix).c_str(),
//							"w");
//					if (cmp == NULL || pos == NULL)
//						OUTPUT_AND_DIE("Exception: can't open output files");
//
//					//	 Allocate the pre-defined size of memory
//					REGISTER_VECTOR_RAII(uint32_t, list, MAXLEN);
//					REGISTER_VECTOR_RAII(uint32_t, cmp_array, MAXLEN);
//
//					EncodingPtr c = EncodingFactory::create(id);
//
//					//	 Do actual compression
//					uint64_t cmp_pos = 0;
//					uint32_t prev = VC_LOAD32(vec.at(currFileNum));
//					uint32_t base = prev;
//
//					// d-gap
//					for (uint32_t j = 0; j < num - 1; j++) {
//						// 这里因为base已经另外存储了，所以num-1
//						uint32_t d = VC_LOAD32(vec.at(currFileNum));
//						if (UNLIKELY(d < prev))
//							fprintf(stderr,
//									"List Order Exception: Lists MUST be increasing\n");
//						list[j] = d - prev - 1;
//						prev = d;
//					}
//					write_pos_entry(num, base, cmp_pos, pos);
//
//					uint64_t cmp_size = MAXLEN;
//					c->encodeArray(list, num - 1, cmp_array, &cmp_size);
//					fwrite(cmp_array, 4, cmp_size, cmp);
//					cmp_pos += cmp_size;
//
//					// Write the terminal position for decoding
//					write_pos_entry(cmp_pos, pos);
//
//					fclose(cmp);
//					fclose(pos);
//				} else
//					continue;
//			}	//inner FOR loop, generate temporary files
//
//			// now let's merge all the intermediate files together
//			while (filesMerged < filesGenerated) {
//				do {
//					sprintf(buffer, "%d", numUsedForFileName++);
//				} while (access((input + buffer + encoder_suffix[id]).c_str(),
//				R_OK) != 0);
//				cmptmp = OpenFile((input + buffer + encoder_suffix[id]).c_str(),
//						&lengthInvalid);
//				postmp = OpenFile((input + buffer + pos_suffix).c_str(),
//						&lengthInvalid);
//				if (cmp == NULL || pos == NULL)
//					OUTPUT_AND_DIE("Exception: can't open output files");
//				//unit for n is BYTE
//				VC_LOAD64(postmp);
//				uint32_t num = VC_LOAD32(postmp);
//				uint32_t base = VC_LOAD32(postmp);
//				uint64_t endpos = VC_LOAD64(postmp);
//
//				write_pos_entry(num, base, posInPos, pos);
//				//cmp是FILE型指针，不能直接copy
//				//本欲打算使用mmap直接写入文件，但发现新建的文件大小时0KB，
//				//无法映射到用户空间上无奈只好使用fwrite函数继续
//				fwrite(cmptmp, 4, endpos, cmp);
//				//			MEMCPY(cmp->_p, cmptmp, endpos * 4);
//
//				posInPos += endpos;
//
//				//remove tmp file
//				filesMerged++;
//				remove((input + buffer + encoder_suffix[id]).c_str());
//				remove((input + buffer + pos_suffix).c_str());
//			}
//		}
//		write_pos_entry(posInPos, pos);
//		write_headerinfo(cmp, pos);
//
//		fclose(cmp);
//		fclose(pos);
//
//		/*
//		 #pragma omp parallel shared(filesGenerated, isGenerationFinished), num_threads(2)
//		 {
//		 #pragma omp master
//		 {
//		 // now let's merge all the intermediate files together
//		 //				std::cout << "\"" << omp_get_num_threads() << "\"" << std::endl;
//		 uint32_t const MERGE_THRESHOLD = 10;
//		 uint32_t filesMerged = 0;			//对应于listcount
//		 uint32_t numUsedForFileName = 0;			//对应于i
//		 uint64_t posInPos = 0;
//		 char buffer[50];
//		 uint64_t lengthInvalid = 0;
//		 uint32_t *cmptmp, *postmp;
//
//		 FILE *cmp = fopen(
//		 ("./share/synthesis" + encoder_suffix[id]).c_str(),
//		 "w");
//		 FILE *pos = fopen(("./share/synthesis" + pos_suffix).c_str(),
//		 "w");
//		 skip_headerinfo(cmp, pos);
//
//		 while (!isGenerationFinished) {
//		 std::cout << "section2: " << omp_get_thread_num()
//		 << std::endl;
//		 // 每100个文件合并一次
//		 if (filesGenerated - filesMerged >= MERGE_THRESHOLD) {
//		 for (uint32_t i = 0; i < MERGE_THRESHOLD; i++) {
//		 do {
//		 sprintf(buffer, "%d", numUsedForFileName++);
//		 } while (access(
//		 (input + buffer + encoder_suffix[id]).c_str(),
//		 R_OK) != 0);
//		 cmptmp =
//		 OpenFile(
//		 (input + buffer + encoder_suffix[id]).c_str(),
//		 &lengthInvalid);
//		 postmp = OpenFile(
//		 (input + buffer + pos_suffix).c_str(),
//		 &lengthInvalid);
//		 if (cmp == NULL || pos == NULL)
//		 OUTPUT_AND_DIE(
//		 "Exception: can't open output files");
//		 //unit for n is BYTE
//		 VC_LOAD64(postmp);
//		 uint32_t num = VC_LOAD32(postmp);
//		 uint32_t base = VC_LOAD32(postmp);
//		 uint64_t endpos = VC_LOAD64(postmp);
//
//		 write_pos_entry(num, base, posInPos, pos);
//		 //cmp是FILE型指针，不能直接copy
//		 //本欲打算使用mmap直接写入文件，但发现新建的文件大小时0KB，
//		 //无法映射到用户空间上无奈只好使用fwrite函数继续
//		 fwrite(cmptmp, 4, endpos, cmp);
//		 //			MEMCPY(cmp->_p, cmptmp, endpos * 4);
//
//		 posInPos += endpos;
//
//		 //remove tmp file
//		 filesMerged++;
//		 remove(
//		 (input + buffer + encoder_suffix[id]).c_str());
//		 remove((input + buffer + pos_suffix).c_str());
//
//		 }			//for
//		 }			//if
//		 sleep(1);			//wait 10 secs...
//		 }			//while
//		 for (; filesMerged < filesGenerated;) {
//		 do {
//		 sprintf(buffer, "%d", numUsedForFileName++);
//		 } while (access(
//		 (input + buffer + encoder_suffix[id]).c_str(),
//		 R_OK) != 0);
//		 cmptmp = OpenFile(
//		 (input + buffer + encoder_suffix[id]).c_str(),
//		 &lengthInvalid);
//		 postmp = OpenFile((input + buffer + pos_suffix).c_str(),
//		 &lengthInvalid);
//		 if (cmp == NULL || pos == NULL)
//		 OUTPUT_AND_DIE("Exception: can't open output files");
//		 //unit for n is BYTE
//		 VC_LOAD64(postmp);
//		 uint32_t num = VC_LOAD32(postmp);
//		 uint32_t base = VC_LOAD32(postmp);
//		 uint64_t endpos = VC_LOAD64(postmp);
//
//		 write_pos_entry(num, base, posInPos, pos);
//		 //cmp是FILE型指针，不能直接copy
//		 //本欲打算使用mmap直接写入文件，但发现新建的文件大小时0KB，
//		 //无法映射到用户空间上无奈只好使用fwrite函数继续
//		 fwrite(cmptmp, 4, endpos, cmp);
//		 //			MEMCPY(cmp->_p, cmptmp, endpos * 4);
//
//		 posInPos += endpos;
//
//		 //remove tmp file
//		 filesMerged++;
//		 remove((input + buffer + encoder_suffix[id]).c_str());
//		 remove((input + buffer + pos_suffix).c_str());
//		 }
//
//		 write_pos_entry(posInPos, pos);
//		 write_headerinfo(cmp, pos);
//
//		 fclose(cmp);
//		 fclose(pos);
//		 }			//#pragma omp master
//
//		 //读取每个倒排链并压缩
//		 // note that vec stores one element more
//		 //than the number of posting lists.
//		 #pragma omp parallel for shared(vec)
//		 for (uint32_t i = 0; i < vec.size(); i++) {
//		 std::cout << "for loop(" << omp_get_thread_num() << "): " << i
//		 << std::endl;
//		 uint32_t num = VC_LOAD32(vec.at(i));
//		 if (LIKELY(num > NSKIP && num < MAXLEN)) {
//
//		 char buffer[50];
//		 filesGenerated++;
//		 sprintf(buffer, "%d", i);
//
//		 FILE *cmp = fopen(
//		 (input + buffer + encoder_suffix[id]).c_str(), "w");
//		 FILE *pos = fopen((input + buffer + pos_suffix).c_str(),
//		 "w");
//		 if (cmp == NULL || pos == NULL)
//		 OUTPUT_AND_DIE("Exception: can't open output files");
//
//		 //	 Allocate the pre-defined size of memory
//		 REGISTER_VECTOR_RAII(uint32_t, list, MAXLEN);
//		 REGISTER_VECTOR_RAII(uint32_t, cmp_array, MAXLEN);
//
//		 EncodingPtr c = EncodingFactory::create(id);
//
//		 //	 Do actual compression
//		 uint64_t cmp_pos = 0;
//		 uint32_t prev = VC_LOAD32(vec.at(i));
//		 uint32_t base = prev;
//
//		 // d-gap
//		 for (uint32_t j = 0; j < num - 1; j++) {
//		 // 这里因为base已经另外存储了，所以num-1
//		 uint32_t d = VC_LOAD32(vec.at(i));
//		 if (UNLIKELY(d < prev))
//		 fprintf(stderr,
//		 "List Order Exception: Lists MUST be increasing\n");
//		 list[j] = d - prev - 1;
//		 prev = d;
//		 }
//		 write_pos_entry(num, base, cmp_pos, pos);
//
//		 uint64_t cmp_size = MAXLEN;
//		 c->encodeArray(list, num - 1, cmp_array, &cmp_size);
//		 fwrite(cmp_array, 4, cmp_size, cmp);
//		 cmp_pos += cmp_size;
//
//		 // Write the terminal position for decoding
//		 write_pos_entry(cmp_pos, pos);
//
//		 fclose(cmp);
//		 fclose(pos);
//		 } else
//		 continue;
//		 }
//		 isGenerationFinished = true;
//		 }			//#pragma omp parallel
//		 */
//	}			//FIELD
	/****************************************************************/
	//         compress posting lists using single-thread           //
	/****************************************************************/

	/* Open a input file */
	uint64_t len = 0;
	uint32_t *addr = OpenFile(input, &len);

	/* Open output files */
	FILE *cmp = fopen((input + encoder_suffix[id]).c_str(), "w");
	FILE *pos = fopen((input + pos_suffix).c_str(), "w");
	if (cmp == NULL || pos == NULL)
		OUTPUT_AND_DIE("Exception: can't open output files");

	/* Allocate the pre-defined size of memory */
	REGISTER_VECTOR_RAII(uint32_t, list, MAXLEN);
	REGISTER_VECTOR_RAII(uint32_t, cmp_array, MAXLEN);

	EncodingPtr c = EncodingFactory::create(id);

	/* Skip a header, and fill it finally  */
	skip_headerinfo(cmp, pos);

	/* Do actual compression */
	uint64_t cmp_pos = 0;

// 这里addr是文件起始地址，而term应该是结束地址
// len（字节个数）右移两位是因为指针是按照uint32_t读取的
	uint32_t *term = addr + (len >> 2);

// 统计值
	uint64_t total = 0;
	double elapsed = 0;

	while (LIKELY(addr < term)) {
		/* Read the numer of integers in a list */
		uint32_t num = VC_LOAD32(addr);
		total += num;
		if (UNLIKELY(addr + num > term))
			goto LOOP_END;

		uint32_t prev = VC_LOAD32(addr);
		uint32_t base = prev;

		if (LIKELY(num > NSKIP && num < MAXLEN)) {
			for (uint32_t i = 0; i < num - 1; i++) {
				// 这里因为base已经另外存储了，所以num-1
				uint32_t d = VC_LOAD32(addr);
				if (UNLIKELY(d < prev)) {
					fprintf(stderr,
							"List Order Exception: Lists MUST be increasing\n");
					exit(1);
				}

				//目前只有IPC不用d-gap
				if (id != E_BINARYIPL)
					list[i] = d - prev - 1;
				else
					list[i] = d;

				prev = d;
			}

			if (squeeze_enabled) {
				if (num < num_compressed)
					continue;

				/* Chop lists to compress for performance tests */
				num = num_compressed;
			}

			write_pos_entry(num, base, cmp_pos, pos);

			uint64_t cmp_size = MAXLEN;

			BenchmarkTimer t;
			c->encodeArray(list, num - 1, cmp_array, &cmp_size);
			elapsed += t.elapsed();

			/* NOTE: the data in cmp_array are byte-order free */
			fwrite(cmp_array, 4, cmp_size, cmp);
			cmp_pos += cmp_size;
		} else {
			/* Skip a needless list */
			addr += num - 1;
		}
	}

	LOOP_END:
	/* Write the terminal position for decoding */
	write_pos_entry(cmp_pos, pos);
	/* Fill the header */
	write_headerinfo(cmp, pos);

	fclose(cmp);
	fclose(pos);

	/* Show performance results */
	fprintf(stdout, "Performance Results(ID:%d):\n", encoder_id);
	fprintf(stdout, "  Total Num Encoded: %llu\n",
			static_cast<unsigned long long>(total));
	fprintf(stdout, "  Elapsed: %.2lf\n", elapsed);
	fprintf(stdout, "  Performance: %.2lfmis\n",
			(total + 0.0) / (elapsed * 1000000));
	fprintf(stdout, "  Throughput: %.2lfGiB/s\n",
			total * 4.0 / (elapsed * 1024 * 1024 * 1024));
	fprintf(stdout, "  Size: %.2lfbpi\n", ((cmp_pos + 0.0) / total) * 32);
}

void do_decompress(const std::string& input, const std::string& output) {
	/* Open a file for position */
	uint64_t poslen = 0;
	uint32_t *pos = OpenFile(input, &poslen);

	/* Get a encoder id from the header */
	validate_encoder_id(&pos);

	/* Open a file for position */
	uint64_t cmplen = 0;
	uint32_t *cmp = OpenFile(
			input.substr(0, input.length() - pos_suffix.length())
					+ encoder_suffix[encoder_id], &cmplen);

	/* Open a output file */
	FILE *out = NULL;
	if (output.length() != 0) {
		out = fopen(output.c_str(), "w");
		if (out == NULL)
			OUTPUT_AND_DIE("Exception: can't open a output file");
	}

	/* Validate input files with header infomation */
	validate_headerinfo(&cmp, cmplen, &pos, poslen);

	REGISTER_VECTOR_RAII(uint32_t, list, DECODE_REQUIRE_MEM(MAXLEN + 128));

	EncodingPtr c = EncodingFactory::create(encoder_id);

	/* Summary information */
	uint64_t dnum = 0;
	double elapsed = 0;

	/* Do actual decompression */
	uint32_t numHeaders = (poslen - POS_HEADER_SZ) / POS_EACH_ENTRY_SZ;

	uint64_t cmp_pos = VC_LOAD64(pos);
	for (uint64_t i = 0; i < numHeaders; i++) {
		uint32_t num = VC_LOAD32(pos);
		uint32_t prev = VC_LOAD32(pos);
		uint64_t next_pos = VC_LOAD64(pos);

		ASSERT(num < MAXLEN);

		/* Do decoding */
		uint32_t *ptr = reinterpret_cast<uint32_t *>(cmp) + cmp_pos;

		BenchmarkTimer t;
		c->decodeArray(ptr, next_pos - cmp_pos, list, num - 1);
		elapsed += t.elapsed();
		dnum += num - 1;

		/* Write in the output file */
		if (out != NULL) {
			char buf[8];
			BYTEORDER_FREE_STORE32(buf, num);
			BYTEORDER_FREE_STORE32(buf + 4, prev);

			fwrite(buf, 8, 1, out);

			if (encoder_id != E_BINARYIPL) {
				//with d-gap
				for (uint32_t j = 0; j < num - 1; j++) {
					uint32_t d = VC_LOAD32(list);
					prev += d + 1;
					BYTEORDER_FREE_STORE32(buf, prev);
					fwrite(buf, 4, 1, out);
				}
			} else {
				// without d-gap
				fwrite(list, num - 1, 4, out);
			}
		}

		/* Move to next */
		cmp_pos = next_pos;
	}

	/* Show performance results */
	fprintf(stdout, "Performance Results(ID:%d):\n", encoder_id);
	fprintf(stdout, "  Total Num Decoded: %llu\n",
			static_cast<unsigned long long>(dnum));
	fprintf(stdout, "  Elapsed: %.2lf\n", elapsed);
	fprintf(stdout, "  Performance: %.2lfmis\n",
			(dnum + 0.0) / (elapsed * 1000000));
	fprintf(stdout, "  Throughput: %.2lfGiB/s\n",
			dnum * 4.0 / (elapsed * 1024 * 1024 * 1024));
	fprintf(stdout, "  Size: %.2lfbpi\n", ((cmp_pos + 0.0) / dnum) * 32);
}

void CodecTest() {
	const int SEQUENCE_LENGTH = 65536;
	const int CODEWORD_LENGTH = 50000;

	EncodingPtr ptr = EncodingFactory::create(18);
//	ptr =
//			EncodingPtr(
//					static_cast<internals::EncodingBase *>(new internals::VSEncodingNaive()));

//	std::cout << "integers to be compressed:" << std::endl;
	uint32_t* const in = (uint32_t*) malloc(SEQUENCE_LENGTH * sizeof(uint32_t));

	for (int i = 0; i < SEQUENCE_LENGTH; i++) {
		*(in + i) = i;
		//		std::cout << *(in + i) << std::endl;
	}
	uint32_t*out = (uint32_t*) malloc(CODEWORD_LENGTH * sizeof(uint32_t));
	uint64_t size = CODEWORD_LENGTH;
	ptr->encodeArray(in, SEQUENCE_LENGTH, out, &size);

	uint32_t* outcome = (uint32_t*) malloc(SEQUENCE_LENGTH * sizeof(uint32_t));
	uint64_t size1 = SEQUENCE_LENGTH;
	std::cout << "Decompression started:" << std::endl;
	ptr->decodeArray(out, size, outcome, SEQUENCE_LENGTH);
	for (int i = 0; i < SEQUENCE_LENGTH; i++) {
		std::cout << outcome[i] << std::endl;
	}
}
}
;
/* namespace: */

int main(int argc, char **argv) {

	/*#pragma omp parallel num_threads(8)
	 {
	 #pragma omp for nowait
	 for (int i = 0; i < 10; ++i) {
	 std::cout << i << "i" << std::endl;
	 }
	 #pragma omp for
	 for (int j = 0; j < 10; ++j) {
	 std::cout << j << "j" << std::endl;
	 }
	 }
	 #pragma omp parallel
	 {
	 #pragma omp master
	 {
	 std::cout << "master" << std::endl;
	 }
	 #pragma omp for
	 for (int j = 0; j < 10; ++j) {
	 std::cout << j << "j" << std::endl;
	 }
	 }*/
	if (parse_command(argc, argv)) {
		//注意java是大端，而这里面的函数却又都是小端
		//解压文件输入是vc记录文件，不是压缩文件

		fprintf(stderr,
				"vcompress: compressed data not written to a terminal.\n");
		fprintf(stderr, "For help, type: vcompress -h\n");
		exit(1);
	}
	for (int i = 0; i < 1; ++i) {
		if (decompress_enabled)
			do_decompress(input, output);
		else
			do_compress(input, encoder_id);
	}

	return 0;
}
