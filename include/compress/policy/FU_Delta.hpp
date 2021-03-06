/*-----------------------------------------------------------------------------
 *  FU_Delta.hpp - A encoder/decoder for naive Delta
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

#ifndef __FU_DELTA_HPP__
#define __FU_DELTA_HPP__

#include <misc/encoding_internals.hpp>

#include <compress/EncodingBase.hpp>
#include <io/BitsReader.hpp>
#include <io/BitsWriter.hpp>

namespace integer_encoding {
namespace internals {

class FU_Delta : public EncodingBase {
 public:
  FU_Delta() : EncodingBase(E_FU_DELTA) {}
  ~FU_Delta() throw() {}

  void encodeArray(const uint32_t *in,
                   uint64_t len,
                   uint32_t *out,
                   uint64_t *nvalue) const {
    BitsWriter wt(out, *nvalue);
    *nvalue = wt.deltaArray(in, len);
  }

  uint64_t require(uint64_t len) const {
    /* Delta needs 42-bit for UINT32_MAX */
    return (42 * len) >> 5;
  }

  void decodeArray(const uint32_t *in,
                   uint64_t len,
                   uint32_t *out,
                   uint64_t nvalue) const {
    BitsReader rd(in, len);
    rd.fudeltaArray(out, nvalue);
  }
}; /* FU_Delta */

} /* namespace: internals */
} /* namespace: integer_encoding */

#endif /* __FU_DELTA_HPP__ */
