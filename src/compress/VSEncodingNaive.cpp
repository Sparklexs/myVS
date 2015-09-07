/*-----------------------------------------------------------------------------
 *  VSEncodingNaive.hpp - A compact-oriented implementation of VSEncoding
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

#include <compress/policy/VSEncodingNaive.hpp>

namespace integer_encoding {
namespace internals {

namespace {

const uint32_t VSENAIVE_LOGLEN = 3;
const uint32_t VSENAIVE_LOGLOG = 3;
// 这个3是有问题的，因为b的取值范围是2^4个
// 但naive的作用只是处理VSE-R的bit位数，故3足够了

const uint32_t VSENAIVE_LENS_LEN = 1U << VSENAIVE_LOGLEN;

// lens则存储了k所能取得的值
const uint32_t VSENAIVE_LENS[] = { 1, 2, 4, 6, 8, 16, 32, 64 };

// 这里的logs和remaplogs存储了b所能取得的值，即从13开始，向4的整数倍重映射
const uint32_t VSENAIVE_LOGS[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 16,
		20, 32 };

const uint32_t VSENAIVE_REMAPLOGS[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
		12, 16, 16, 16, 16, 20, 20, 20, 20, 32, 32, 32, 32, 32, 32, 32, 32, 32,
		32, 32, 32 };
//该数组存的是VSENAIVE_LENS的索引,lens[0]=1,lens[1]=2,lens[2]=4,lens[7]=64
const uint32_t VSENAIVE_CODELENS[] = { 0/*1*/, 0, 1/*2*/, 0, 2/*4*/, 0, 3/*6*/,
		0, 4/*8*/, 0, 0, 0, 0, 0, 0, 0, 5/*16*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 6/*32*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7 /*64*/};
//同理，存的是VSENAIVE_LOGS的索引，value -> key
const uint32_t VSENAIVE_CODELOGS[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
		13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
		15, 15 };

} /* namespace: */

VSEncodingNaive::VSEncodingNaive() :
		EncodingBase(E_INVALID), vdp_(new VSEncodingDP(VSENAIVE_LENS,
		NULL, ARRAYSIZE(VSENAIVE_LENS), false)) {
}

VSEncodingNaive::~VSEncodingNaive() throw () {
}

void VSEncodingNaive::encodeArray(const uint32_t *in, uint64_t len,
		uint32_t *out, uint64_t *nvalue) const {
	if (in == NULL)
		THROW_ENCODING_EXCEPTION("Invalid input: in");
	if (len == 0)
		THROW_ENCODING_EXCEPTION("Invalid input: len");
	if (out == NULL)
		THROW_ENCODING_EXCEPTION("Invalid input: out");
	if (*nvalue == 0)
		THROW_ENCODING_EXCEPTION("Invalid input: nvalue");

	ASSERT_ADDR(in, len);ASSERT_ADDR(out, *nvalue);

	/* Compute optimal partition */
	std::vector<uint32_t> logs;
	for (uint64_t i = 0; i < len; i++) {
		// MSB的作用是返回左起第一个1之前的0的个数,
		// 32-MSB(x)并不表示ceiling(log(x)),而是表示二进制x需要多少位描述的意思
		// uint32_t msb = MSB32(in[i]);
		logs.push_back(VSENAIVE_REMAPLOGS[32 - MSB32(in[i])]);
	}
	ASSERT(logs.size() == len);
#ifdef PARTITIONWITHOP //使用近似划分

	// 不同于DP版本的(k,b) tuple，这里k和b分开存储
	optimal_partition op(logs, 64);
	std::vector<uint64_t> parts = op.partition;
	std::vector<uint32_t> bParts = op.Bs;
//	for (int i = 0; i < op.Bs.size(); i++) {
//		std::cout << parts[i] << std::endl;
//		std::cout << op.Ks[i] << std::endl;
//		std::cout << bParts[i] << std::endl;
//	}
//	std::cout << parts[parts.size() - 1] << std::endl;
	uint64_t csize = *nvalue - 2;

	// 使用Simple16来压缩所有的K值
	Simple16 simp;
	/* Write the values of K */
	simp.encodeArray(op.Ks.data(), op.Ks.size(), out + 2, &csize);

	//分别Simple16的存储压缩大小和原始大小
	BYTEORDER_FREE_STORE32(out, csize);
	BYTEORDER_FREE_STORE32(out + 1, op.Ks.size());
//	for (int i = 0; i < csize + 2; i++) {
//		std::cout << out[i] << std::endl;
//	}
	out += csize + 2;
//	std::cout << out[0] << std::endl;
	BitsWriter wt(out, *nvalue - csize - 2);
	uint64_t num = parts.size() - 1;

	for (uint64_t i = 0; i < num; i++) {
		/* Write the value of B*/
		wt.write_bits(VSENAIVE_CODELOGS[bParts[i]], VSENAIVE_LOGLOG);
		/* Write integers */
		for (uint64_t j = parts[i]; j < parts[i + 1]; j++)
			wt.write_bits(in[j], bParts[i]);
	}

	wt.flush_bits();
	*nvalue = wt.size() + 2 + csize;

#else //使用动态规划

	std::vector<uint32_t> parts;
	ASSERT(parts.size() == 0);

	// 计算出每一个block中的k值，存储在parts链表中
	vdp_->computePartition(logs, &parts, VSENAIVE_LOGLEN + VSENAIVE_LOGLOG);
	ASSERT(parts.size() > 1);

	/* Ready to write data */
	BitsWriter wt(out, *nvalue);

	uint64_t num = parts.size() - 1;
	for (uint64_t i = 0; i < num; i++) {
		/* Compute max B in the block */
		// 在每个block中计算出所需要的b的值
		uint32_t maxB = 0;

		for (auto j = parts[i]; j < parts[i + 1]; j++) {
			if (maxB < logs[j])
			maxB = logs[j];
		}

		/* Write the value of B and K */
		wt.write_bits(VSENAIVE_CODELOGS[maxB], VSENAIVE_LOGLOG);
		wt.write_bits(VSENAIVE_CODELENS[parts[i + 1] - parts[i]],
				VSENAIVE_LOGLEN);
		/* Write integers */
		for (uint64_t j = parts[i]; j < parts[i + 1]; j++)
		wt.write_bits(in[j], maxB);
	}

	wt.flush_bits();
	*nvalue = wt.size();
#endif
}

void VSEncodingNaive::decodeArray(const uint32_t *in, uint64_t len,
		uint32_t *out, uint64_t nvalue) const {
	if (in == NULL)
		THROW_ENCODING_EXCEPTION("Invalid input: in");
	if (len == 0)
		THROW_ENCODING_EXCEPTION("Invalid input: len");
	if (out == NULL)
		THROW_ENCODING_EXCEPTION("Invalid input: out");
	if (nvalue == 0)
		THROW_ENCODING_EXCEPTION("Invalid input: nvalue");

	ASSERT_ADDR(in, len);ASSERT_ADDR(out, nvalue);

	uint32_t *oterm = out + nvalue;
#ifdef PARTITIONWITHOP //使用近似划分

	//读取Simple16的压缩大小和原始大小
	uint32_t cmpSize = BYTEORDER_FREE_LOAD32(in);
	uint32_t leng = BYTEORDER_FREE_LOAD32(in + 1);
//	for (int i = 0; i < cmpSize + 2; i++) {
//		std::cout << in[i] << std::endl;
//	}

	//读取Simple16压缩的Ks并另外存储
	Simple16 simp;
//	uint32_t Ks[leng];
	uint32_t* Ks = (uint32_t*) malloc(leng * 4);
	simp.decodeArray(in + 2, cmpSize, Ks, nvalue);
//
//	memcpy(Ks, out, leng * 4);
	in += cmpSize + 2;

	BitsReader rd(in, len - cmpSize - 2);
	uint32_t i = 0;
//	int count = 0;
	while (LIKELY(out < oterm)) {
		uint32_t B = VSENAIVE_LOGS[rd.read_bits(VSENAIVE_LOGLOG)];

		for (uint32_t j = 0; j < Ks[i]; j++) {
			out[j] = (B != 0) ? rd.read_bits(B) : 0;
//			std::cout << count++ << ":" << out[j] << std::endl;
		}
		out += Ks[i++];
	}

#else

	BitsReader rd(in, len);
	while (LIKELY(out < oterm)) {
		uint32_t B = VSENAIVE_LOGS[rd.read_bits(VSENAIVE_LOGLOG)];
		uint32_t K = VSENAIVE_LENS[rd.read_bits(VSENAIVE_LOGLEN)];

		for (uint32_t i = 0; i < K; i++) {
			out[i] = (B != 0) ? rd.read_bits(B) : 0;
//		printf("out[%d]:%d\cmpSize",i,out[i]);
		}
		out += K;
	}
#endif
}

uint64_t VSEncodingNaive::require(uint64_t len) const {
	/* FIXME: Fill correct the required size */
	return len;
}

} /* namespace: internals */
} /* namespace: integer_encoding */
