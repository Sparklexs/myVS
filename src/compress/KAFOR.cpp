/**
 * filename:KAFOR.cpp
 * @brief:
 * @data:2013-11-18
 * @author:zxd
 */
#include <compress/policy/AFOR/KAFOR.hpp>

using namespace integer_encoding::internals;
KAFOR::KAFOR() :
		EncodingBase(E_KAFOR) {

}
KAFOR::~KAFOR() throw () {

}
void KAFOR::encodeArray(const uint32_t *in, uint64_t len, uint32_t *out,
		uint64_t *nvalue) const {
//	char* outInChar = static_cast<char*>(out);
	*nvalue = encode<uint32_t>((char*) out, in, len);
}
void KAFOR::decodeArray(const uint32_t *in, uint64_t len, uint32_t *out,
		uint64_t nvalue) const {
//	char* inInChar = static_cast<char*>(in);
	if (len != decode<uint32_t>(out, (char*) in, nvalue))
		throw "len of in unequals to length actually used!";
}
uint64_t KAFOR::require(uint64_t len) const {
	return len;
}
int KAFOR::encodeUint32(char* des, const uint32_t* src, uint32_t encodeNum) {
	return encode<uint32_t>(des, src, encodeNum);
}
int KAFOR::decodeUint32(uint32_t* des, const char* src, uint32_t decodeNum) {
	return decode<uint32_t>(des, src, decodeNum);
}
int KAFOR::encodeUint16(char* des, const uint16_t* src, uint32_t encodeNum) {
	return encode<uint16_t>(des, src, encodeNum);
}
int KAFOR::decodeUint16(uint16_t* des, const char* src, uint32_t decodeNum) {
	return decode<uint16_t>(des, src, decodeNum);
}
int KAFOR::encodeUint8(char* des, const uint8_t* src, uint32_t encodeNum) {
	return encode<uint8_t>(des, src, encodeNum);
}
int KAFOR::decodeUint8(uint8_t* des, const char* src, uint32_t decodeNum) {
	return decode<uint8_t>(des, src, decodeNum);
}

Compressor* KAFOR::clone() {
	Compressor* pNewComp = new KAFOR(*this);
	return pNewComp;
//	return NULL;
}

