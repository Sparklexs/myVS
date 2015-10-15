/**
 * filename:AFOR.cpp
 * @brief:
 * @data:2013-11-18
 * @author:zxd
 */
#include <compress/policy/AFOR/AFOR.hpp>
using namespace integer_encoding::internals;
AFOR::AFOR() :
		EncodingBase(E_KAFOR) {

}
AFOR::~AFOR() throw () {

}
void AFOR::encodeArray(const uint32_t *in, uint64_t len, uint32_t *out,
		uint64_t *nvalue) const {
//	char* outInChar = static_cast<char*>(out);
	*nvalue = encode<uint32_t>((char*) out, in, len);
}
void AFOR::decodeArray(const uint32_t *in, uint64_t len, uint32_t *out,
		uint64_t nvalue) const {
//	char* inInChar = static_cast<char*>(in);
	if (len != decode<uint32_t>(out, (char*) in, nvalue))
		throw "len of in unequals to length actually used!";
}
uint64_t AFOR::require(uint64_t len) const {
	return len;
}
int AFOR::encodeUint32(char* des, const uint32_t* src, uint32_t encodeNum) {
	return encode<uint32_t>(des, src, encodeNum);
}
int AFOR::decodeUint32(uint32_t* des, const char* src, uint32_t decodeNum) {
	return decode<uint32_t>(des, src, decodeNum);
}
int AFOR::encodeUint16(char* des, const uint16_t* src, uint32_t encodeNum) {
	return encode<uint16_t>(des, src, encodeNum);
}
int AFOR::decodeUint16(uint16_t* des, const char* src, uint32_t decodeNum) {
	return decode<uint16_t>(des, src, decodeNum);
}
int AFOR::encodeUint8(char* des, const uint8_t* src, uint32_t encodeNum) {
	return encode<uint8_t>(des, src, encodeNum);
}
int AFOR::decodeUint8(uint8_t* des, const char* src, uint32_t decodeNum) {
	return decode<uint8_t>(des, src, decodeNum);
}

Compressor* AFOR::clone() {
	Compressor* pNewComp = new AFOR(*this);
	return pNewComp;
}

