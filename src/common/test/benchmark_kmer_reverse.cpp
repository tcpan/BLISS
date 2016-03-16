/*
 * Copyright 2015 Georgia Institute of Technology
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file    test_kmer_reverse.cpp
 * @ingroup
 * @author  tpan
 * @brief
 * @details
 */



#include "utils/logging.h"

// include google test
#include <gtest/gtest.h>
#include "utils/bitgroup_ops.hpp"

#include <random>
#include <cstdint>

#include "common/kmer.hpp"
#include "common/test/kmer_reverse_helper.hpp"

#include "common/alphabets.hpp"
#include "common/alphabet_traits.hpp"
#include "utils/benchmark_utils.hpp"

// include files to test

//TESTS: Sequential, SWAR/BSWAP, SSSE3, AVX2 versions of kmer reverse.
//TESTS: for each, test kmer size, different word type, and Alphabet (bit group size)
//       different offsets, different bit group sizes, different word types, and different byte array lengths.

template <typename T>
class KmerReverseBenchmark : public ::testing::Test {
  protected:



    static bliss::common::test::KmerReverseHelper<T> helper;

    static constexpr size_t iterations = 1000000;

    static std::vector<T> kmers;
    static std::vector<T> outputs;

  public:
    static void SetUpTestCase()
    {
      kmers.resize(iterations);
      outputs.resize(iterations);


      srand(23);
      for (size_t i = 0; i < iterations; ++i) {
        for (size_t j = 0; j < T::nWords; ++j) {
          kmers[i].getDataRef()[j] = static_cast<typename T::KmerWordType>(static_cast<long>(rand()) << 32) | static_cast<long>(rand());
        }
      }
    }

    template <unsigned int BITS, unsigned char SIMD>
    struct reverse_op {
    	::bliss::utils::bit_ops::bitgroup_ops<BITS, SIMD> op;

    	template <typename WORD_TYPE>
    	inline WORD_TYPE operator()(WORD_TYPE const & src) const {
    		return op.reverse(src);
    	}
    };
    template <unsigned int BITS, unsigned char SIMD>
    struct reverse_negate_op {
    	::bliss::utils::bit_ops::bitgroup_ops<BITS, SIMD> op;

    	template <typename WORD_TYPE>
    	inline WORD_TYPE operator()(WORD_TYPE const & src) const {
    		return bliss::utils::bit_ops::bit_not(op.reverse(src));
    	}
    };
    template <unsigned int BITS, unsigned char SIMD>
    struct reverse_1bit_op {
    	::bliss::utils::bit_ops::bitgroup_ops<1, SIMD> op;

    	template <typename WORD_TYPE>
    	inline WORD_TYPE operator()(WORD_TYPE const & src) const {
    		return op.reverse(src);
    	}
    };



    template <typename SIMDType, typename TT = T,
        typename ::std::enable_if<::std::is_same<typename TT::KmerAlphabet, ::bliss::common::DNA>::value ||
        ::std::is_same<typename TT::KmerAlphabet, ::bliss::common::RNA>::value ||
         ::std::is_same<typename TT::KmerAlphabet, ::bliss::common::DNA16>::value ||
         ::std::is_same<typename TT::KmerAlphabet, ::bliss::common::DNA6>::value ||
          ::std::is_same<typename TT::KmerAlphabet, ::bliss::common::RNA6>::value, int>::type = 1>
    void benchmark() {

      constexpr size_t shift = TT::nWords * sizeof(typename TT::KmerWordType) * 8 - TT::nBits;
      constexpr uint16_t overlap = sizeof(typename SIMDType::MachineWord) % TT::bitsPerChar;

      reverse_op<TT::bitsPerChar, SIMDType::SIMDVal> op;

      for (size_t i = 0; i < iterations; ++i) {

        bliss::utils::bit_ops::reverse_transform<SIMDType, shift, overlap,
          typename TT::KmerWordType, TT::nWords>(outputs[i].getDataRef(), kmers[i].getDataRef(),
            op);
      }
    }
    template <typename SIMDType, typename TT = T,
        typename ::std::enable_if<!(::std::is_same<typename TT::KmerAlphabet, ::bliss::common::DNA>::value ||
        ::std::is_same<typename TT::KmerAlphabet, ::bliss::common::RNA>::value ||
         ::std::is_same<typename TT::KmerAlphabet, ::bliss::common::DNA16>::value ||
         ::std::is_same<typename TT::KmerAlphabet, ::bliss::common::DNA6>::value ||
          ::std::is_same<typename TT::KmerAlphabet, ::bliss::common::RNA6>::value), int>::type = 1>
    void benchmark() {
    }

    template <typename SIMDType, typename TT = T,
        typename ::std::enable_if<::std::is_same<typename TT::KmerAlphabet, ::bliss::common::DNA>::value ||
        ::std::is_same<typename TT::KmerAlphabet, ::bliss::common::RNA>::value, int>::type = 1>
    void benchmark_c() {

      constexpr size_t shift = TT::nWords * sizeof(typename TT::KmerWordType) * 8 - TT::nBits;
      constexpr uint16_t overlap = sizeof(typename SIMDType::MachineWord) % TT::bitsPerChar;

      reverse_negate_op<TT::bitsPerChar, SIMDType::SIMDVal> op;

      for (size_t i = 0; i < iterations; ++i) {

        bliss::utils::bit_ops::reverse_transform<SIMDType, shift, overlap,
          typename TT::KmerWordType, TT::nWords>(outputs[i].getDataRef(), kmers[i].getDataRef(),
            op);

      }
    }
    template <typename SIMDType, typename TT = T,
        typename ::std::enable_if<::std::is_same<typename TT::KmerAlphabet, ::bliss::common::DNA16>::value ||
        ::std::is_same<typename TT::KmerAlphabet, ::bliss::common::DNA6>::value ||
         ::std::is_same<typename TT::KmerAlphabet, ::bliss::common::RNA6>::value, int>::type = 1>
    void benchmark_c() {
      constexpr size_t shift = TT::nWords * sizeof(typename TT::KmerWordType) * 8 - TT::nBits;

      reverse_1bit_op<TT::bitsPerChar, SIMDType::SIMDVal> op1;

      for (size_t i = 0; i < iterations; ++i) {

        bliss::utils::bit_ops::reverse_transform<SIMDType, shift, 0,
          typename TT::KmerWordType, TT::nWords>(outputs[i].getDataRef(), kmers[i].getDataRef(),
            op1);

      }
    }
    template <typename SIMDType, typename TT = T,
        typename ::std::enable_if<!(::std::is_same<typename TT::KmerAlphabet, ::bliss::common::DNA>::value ||
        ::std::is_same<typename TT::KmerAlphabet, ::bliss::common::RNA>::value ||
         ::std::is_same<typename TT::KmerAlphabet, ::bliss::common::DNA16>::value ||
         ::std::is_same<typename TT::KmerAlphabet, ::bliss::common::DNA6>::value ||
          ::std::is_same<typename TT::KmerAlphabet, ::bliss::common::RNA6>::value), int>::type = 1>
    void benchmark_c() {
    }

};



template <typename T>
bliss::common::test::KmerReverseHelper<T> KmerReverseBenchmark<T>::helper;

template <typename T>
constexpr size_t KmerReverseBenchmark<T>::iterations;

template <typename T>
std::vector<T> KmerReverseBenchmark<T>::kmers;
template <typename T>
std::vector<T> KmerReverseBenchmark<T>::outputs;


// to save some typing.  note that func is not a functor nor lambda function, so using macro is easier than as a templated function.
#define TEST_REV(name, func, kmertype) do { \
    BL_TIMER_START(km); \
    for (size_t i = 0; i < KmerReverseBenchmark<kmertype>::iterations; ++i) { \
      KmerReverseBenchmark<kmertype>::outputs[i] = func(KmerReverseBenchmark<kmertype>::kmers[i]); \
    } \
    BL_TIMER_END(km, name, KmerReverseBenchmark<kmertype>::iterations); \
  } while (0)


// to save some typing.  note that func is not a functor nor lambda function, so using macro is easier than as a templated function.
#define TEST_KMER_REV(name, func, kmertype) do { \
    BL_TIMER_START(km); \
    for (size_t i = 0; i < KmerReverseBenchmark<kmertype>::iterations; ++i) { \
      KmerReverseBenchmark<kmertype>::kmers[i].func(KmerReverseBenchmark<kmertype>::outputs[i]); \
    } \
    BL_TIMER_END(km, name, KmerReverseBenchmark<kmertype>::iterations); \
  } while (0)


// to save some typing.  note that func is not a functor nor lambda function, so using macro is easier than as a templated function.
#define TEST_REV_BITOPS(name, simd, kmertype) do { \
    BL_TIMER_START(km); \
    \
    constexpr uint16_t pad_bits = (kmertype::nWords * sizeof(typename kmertype::KmerWordType) * 8) - kmertype::nBits; \
    \
    for (size_t i = 0; i < KmerReverseBenchmark<kmertype>::iterations; ++i) { \
      bliss::utils::bit_ops::reverse<TypeParam::bitsPerChar, simd, pad_bits, \
        typename kmertype::KmerWordType, kmertype::nWords>(KmerReverseBenchmark<kmertype>::outputs[i].getDataRef(), \
    		  KmerReverseBenchmark<kmertype>::kmers[i].getDataRef()); \
      KmerReverseBenchmark<kmertype>::outputs[i].template right_shift_bits<(TypeParam::nWords * sizeof(typename TypeParam::KmerWordType) * 8 - TypeParam::nBits)>();  \
      /* shift by remainder/padding. */ \
    } \
    BL_TIMER_END(km, name, KmerReverseBenchmark<kmertype>::iterations); \
    } while (0)


// to save some typing.  note that func is not a functor nor lambda function, so using macro is easier than as a templated function.
#define TEST_REVC_BITOPS(name, simd, kmertype) do { \
    BL_TIMER_START(km); \
    \
    constexpr uint16_t pad_bits = (kmertype::nWords * sizeof(typename kmertype::KmerWordType) * 8) - kmertype::nBits; \
    \
    for (size_t i = 0; i < KmerReverseBenchmark<kmertype>::iterations; ++i) { \
      switch (TypeParam::bitsPerChar) { \
      case 2: \
          bliss::utils::bit_ops::reverse<TypeParam::bitsPerChar, simd, pad_bits, \
            typename kmertype::KmerWordType, kmertype::nWords>(KmerReverseBenchmark<kmertype>::outputs[i].getDataRef(), \
                                                                       KmerReverseBenchmark<kmertype>::kmers[i].getDataRef()); \
          bliss::utils::bit_ops::bit_not<simd, \
            typename kmertype::KmerWordType, kmertype::nWords>(KmerReverseBenchmark<kmertype>::outputs[i].getDataRef(), \
                                        	   KmerReverseBenchmark<kmertype>::outputs[i].getDataRef()); \
          break; \
        case 3: \
        case 4: \
          bliss::utils::bit_ops::reverse<1, simd, 0, \
            typename kmertype::KmerWordType, kmertype::nWords>(KmerReverseBenchmark<kmertype>::outputs[i].getDataRef(), \
                                                  KmerReverseBenchmark<kmertype>::kmers[i].getDataRef()); \
          break; \
        default: \
          break; \
      } \
      \
      KmerReverseBenchmark<kmertype>::outputs[i].template right_shift_bits<(TypeParam::nWords * sizeof(typename TypeParam::KmerWordType) * 8 - TypeParam::nBits)>();  \
      /* shift by remainder/padding. */ \
    } \
    BL_TIMER_END(km, name, KmerReverseBenchmark<kmertype>::iterations); \
    } while (0)



// indicate this is a typed test
TYPED_TEST_CASE_P(KmerReverseBenchmark);

TYPED_TEST_P(KmerReverseBenchmark, reverse)
{
  BL_TIMER_INIT(km);

//  TEST_REV("oldseq", KmerReverseBenchmark<TypeParam>::helper.reverse_serial(km), TypeParam, rev_gold);

  if (((TypeParam::bitsPerChar & (TypeParam::bitsPerChar - 1)) == 0) &&
      (::std::is_same<typename TypeParam::KmerAlphabet, bliss::common::DNA>::value ||
          ::std::is_same<typename TypeParam::KmerAlphabet, bliss::common::RNA>::value ||
           ::std::is_same<typename TypeParam::KmerAlphabet, bliss::common::DNA16>::value)) {
    TEST_REV("bswap", KmerReverseBenchmark<TypeParam>::helper.reverse_bswap, TypeParam);
    TEST_REV("swar", KmerReverseBenchmark<TypeParam>::helper.reverse_swar, TypeParam);

#ifdef __SSSE3__
    TEST_REV("ssse3", KmerReverseBenchmark<TypeParam>::helper.reverse_simd, TypeParam);
#endif

  }  // alphabet for DNA, RNA, and DNA16 are the only ones accelerated with simd type operations.

  TEST_REV_BITOPS("swar_new", ::bliss::utils::bit_ops::BITREV_SWAR, TypeParam);
  BL_TIMER_START(km);
  this->template benchmark<bliss::utils::bit_ops::BITREV_SWAR>();
  BL_TIMER_END(km, "revop swar", KmerReverseBenchmark<TypeParam>::iterations);
#ifdef __SSSE3__
    TEST_REV_BITOPS("ssse3_new", ::bliss::utils::bit_ops::BITREV_SSSE3, TypeParam);
    BL_TIMER_START(km);
    this->template benchmark<bliss::utils::bit_ops::BITREV_SSSE3>();
    BL_TIMER_END(km, "revop ssse3", KmerReverseBenchmark<TypeParam>::iterations);
#endif
#ifdef __AVX2__
    TEST_REV_BITOPS("avx2_new", ::bliss::utils::bit_ops::BITREV_AVX2, TypeParam);
    BL_TIMER_START(km);
    this->template benchmark<bliss::utils::bit_ops::BITREV_AVX2>();
    BL_TIMER_END(km, "revop avx2", KmerReverseBenchmark<TypeParam>::iterations);
#endif
//  TEST_REV_BITOPS("seq_new", ::bliss::utils::bit_ops::BIT_REV_SEQ, TypeParam);
    TEST_KMER_REV("rev", reverse, TypeParam);

BL_TIMER_START(km);
	constexpr size_t bytes = sizeof(typename TypeParam::KmerWordType) * TypeParam::nWords;
this->template benchmark<bliss::utils::bit_ops::BITREV_AUTO_AGGRESSIVE<bytes> >();
BL_TIMER_END(km, "revop auto", KmerReverseBenchmark<TypeParam>::iterations);

  BL_TIMER_REPORT(km);

}


TYPED_TEST_P(KmerReverseBenchmark, revcomp)
{
  BL_TIMER_INIT(km);

  //TEST_REV("oldseqC", KmerReverseBenchmark<TypeParam>::helper.reverse_complement_serial(km), TypeParam, revcomp_gold);

  if (((TypeParam::bitsPerChar & (TypeParam::bitsPerChar - 1)) == 0) &&
      (::std::is_same<typename TypeParam::KmerAlphabet, bliss::common::DNA>::value ||
                ::std::is_same<typename TypeParam::KmerAlphabet, bliss::common::RNA>::value ||
                 ::std::is_same<typename TypeParam::KmerAlphabet, bliss::common::DNA16>::value) ) {
    TEST_REV("bswapC", KmerReverseBenchmark<TypeParam>::helper.reverse_complement_bswap, TypeParam);
    TEST_REV("swarC", KmerReverseBenchmark<TypeParam>::helper.reverse_complement_swar, TypeParam);

#ifdef __SSSE3__
    TEST_REV("ssse3C", KmerReverseBenchmark<TypeParam>::helper.reverse_complement_simd, TypeParam);
#endif

  }  // alphabet for DNA, RNA, and DNA16 are the only ones accelerated with simd type operations.


  TEST_REVC_BITOPS("swarC_new", ::bliss::utils::bit_ops::BITREV_SWAR, TypeParam);

  BL_TIMER_START(km);
  this->template benchmark_c<bliss::utils::bit_ops::BITREV_SWAR>();
  BL_TIMER_END(km, "revopc swar", KmerReverseBenchmark<TypeParam>::iterations);

#ifdef __SSSE3__
    TEST_REVC_BITOPS("ssse3_new", ::bliss::utils::bit_ops::BITREV_SSSE3, TypeParam);

    BL_TIMER_START(km);
    this->template benchmark_c<bliss::utils::bit_ops::BITREV_SSSE3>();
    BL_TIMER_END(km, "revopc ssse3", KmerReverseBenchmark<TypeParam>::iterations);
#endif
#ifdef __AVX2__
    TEST_REVC_BITOPS("avx2_new", ::bliss::utils::bit_ops::BITREV_AVX2, TypeParam);
    BL_TIMER_START(km);
    this->template benchmark_c<bliss::utils::bit_ops::BITREV_AVX2>();
    BL_TIMER_END(km, "revopc avx2", KmerReverseBenchmark<TypeParam>::iterations);
#endif

    // macro does not support bit_ops::BIT_REV_SEQ
    TEST_KMER_REV("revC", reverse_complement, TypeParam);


  BL_TIMER_START(km);
	constexpr size_t bytes = sizeof(typename TypeParam::KmerWordType) * TypeParam::nWords;
  this->template benchmark_c<bliss::utils::bit_ops::BITREV_AUTO_AGGRESSIVE<bytes> >();
  BL_TIMER_END(km, "revopc auto", KmerReverseBenchmark<TypeParam>::iterations);
//  printf("AUTO chose SIMD %u and MachineWord size %lu for kmer k %u, wordsize %lu, bitsPerChar %u, bytes %lu\n",
//         bliss::utils::bit_ops::BITREV_AUTO_AGGRESSIVE<bytes>::SIMDVal,
//         sizeof(typename bliss::utils::bit_ops::BITREV_AUTO_AGGRESSIVE<bytes>::MachineWord),
//         TypeParam::size, sizeof(typename TypeParam::KmerWordType), TypeParam::bitsPerChar, bytes);

  BL_TIMER_REPORT(km);

}





//REGISTER_TYPED_TEST_CASE_P(KmerReverseBenchmark, rev_seq, rev_seq2, revcomp_seq, rev_bswap, revcomp_bswap, rev_swar, revcomp_swar, rev, revcomp, rev_ssse3, revcomp_ssse3);

REGISTER_TYPED_TEST_CASE_P(KmerReverseBenchmark,
		reverse,
		revcomp);

//////////////////// RUN the tests with different types.

// max of 50 cases
typedef ::testing::Types<
    ::bliss::common::Kmer<  3, bliss::common::DNA,    uint8_t>,
    ::bliss::common::Kmer<  3, bliss::common::DNA,   uint16_t>,
    ::bliss::common::Kmer<  3, bliss::common::DNA,   uint32_t>,
    ::bliss::common::Kmer<  3, bliss::common::DNA,   uint64_t>,
    ::bliss::common::Kmer<  7, bliss::common::DNA,    uint8_t>,
    ::bliss::common::Kmer<  7, bliss::common::DNA,   uint16_t>,
    ::bliss::common::Kmer<  7, bliss::common::DNA,   uint32_t>,
    ::bliss::common::Kmer<  7, bliss::common::DNA,   uint64_t>,
    ::bliss::common::Kmer< 15, bliss::common::DNA,    uint8_t>,
    ::bliss::common::Kmer< 15, bliss::common::DNA,   uint16_t>,
    ::bliss::common::Kmer< 15, bliss::common::DNA,   uint32_t>,
    ::bliss::common::Kmer< 15, bliss::common::DNA,   uint64_t>,
    ::bliss::common::Kmer< 31, bliss::common::DNA,    uint8_t>,
    ::bliss::common::Kmer< 31, bliss::common::DNA,   uint16_t>,
    ::bliss::common::Kmer< 31, bliss::common::DNA,   uint32_t>,
    ::bliss::common::Kmer< 31, bliss::common::DNA,   uint64_t>,
    ::bliss::common::Kmer< 15, bliss::common::DNA,   uint64_t>,
    ::bliss::common::Kmer< 31, bliss::common::DNA,   uint64_t>,
    ::bliss::common::Kmer< 63, bliss::common::DNA,   uint64_t>,
    ::bliss::common::Kmer< 95, bliss::common::DNA,   uint64_t>,
    ::bliss::common::Kmer<127, bliss::common::DNA,   uint64_t>,
    ::bliss::common::Kmer< 15, bliss::common::DNA5,  uint64_t>,
    ::bliss::common::Kmer< 31, bliss::common::DNA5,  uint64_t>,
    ::bliss::common::Kmer< 63, bliss::common::DNA5,  uint64_t>,
    ::bliss::common::Kmer< 95, bliss::common::DNA5,  uint64_t>,
    ::bliss::common::Kmer<127, bliss::common::DNA5,  uint64_t>,
    ::bliss::common::Kmer< 15, bliss::common::DNA16, uint64_t>,
    ::bliss::common::Kmer< 31, bliss::common::DNA16, uint64_t>,
    ::bliss::common::Kmer< 63, bliss::common::DNA16, uint64_t>,
    ::bliss::common::Kmer< 95, bliss::common::DNA16, uint64_t>,
    ::bliss::common::Kmer<127, bliss::common::DNA16, uint64_t>,
    ::bliss::common::Kmer< 32, bliss::common::DNA,   uint64_t>,
    ::bliss::common::Kmer< 64, bliss::common::DNA,   uint64_t>,
    ::bliss::common::Kmer< 96, bliss::common::DNA,   uint64_t>,
    ::bliss::common::Kmer<128, bliss::common::DNA,   uint64_t>,
    ::bliss::common::Kmer<256, bliss::common::DNA,   uint64_t>,
    ::bliss::common::Kmer< 32, bliss::common::DNA5,  uint64_t>,
    ::bliss::common::Kmer< 64, bliss::common::DNA5,  uint64_t>,
    ::bliss::common::Kmer< 96, bliss::common::DNA5,  uint64_t>,
    ::bliss::common::Kmer<128, bliss::common::DNA5,  uint64_t>,
    ::bliss::common::Kmer<256, bliss::common::DNA5,  uint64_t>,
    ::bliss::common::Kmer< 32, bliss::common::DNA16, uint64_t>,
    ::bliss::common::Kmer< 64, bliss::common::DNA16, uint64_t>,
    ::bliss::common::Kmer< 96, bliss::common::DNA16, uint64_t>,
    ::bliss::common::Kmer<128, bliss::common::DNA16, uint64_t>,
    ::bliss::common::Kmer<256, bliss::common::DNA16, uint64_t>

//    ::bliss::common::Kmer< 15, bliss::common::DNA_IUPAC, uint64_t>,
//    ::bliss::common::Kmer< 16, bliss::common::DNA_IUPAC, uint64_t>,
//    ::bliss::common::Kmer< 32, bliss::common::DNA_IUPAC, uint64_t>,
//    ::bliss::common::Kmer< 47, bliss::common::DNA_IUPAC, uint64_t>,
//    ::bliss::common::Kmer< 64, bliss::common::DNA_IUPAC, uint64_t>,
//    ::bliss::common::Kmer< 96, bliss::common::DNA_IUPAC, uint64_t>,
//    ::bliss::common::Kmer<128, bliss::common::DNA_IUPAC, uint64_t>,
//    ::bliss::common::Kmer<192, bliss::common::DNA_IUPAC, uint64_t>,
//    ::bliss::common::Kmer<256, bliss::common::DNA_IUPAC, uint64_t>,
//    ::bliss::common::Kmer<  7, bliss::common::ASCII, uint64_t>,
//    ::bliss::common::Kmer< 15, bliss::common::ASCII, uint64_t>,
//    ::bliss::common::Kmer< 31, bliss::common::ASCII, uint64_t>,
//    ::bliss::common::Kmer< 63, bliss::common::ASCII, uint64_t>,
//    ::bliss::common::Kmer< 95, bliss::common::ASCII, uint64_t>,
//    ::bliss::common::Kmer<127, bliss::common::ASCII, uint64_t>,
//    ::bliss::common::Kmer< 32, bliss::common::ASCII, uint64_t>,
//    ::bliss::common::Kmer< 64, bliss::common::ASCII, uint64_t>,
//    ::bliss::common::Kmer< 96, bliss::common::ASCII, uint64_t>,
//    ::bliss::common::Kmer<128, bliss::common::ASCII, uint64_t>,
//    ::bliss::common::Kmer<256, bliss::common::ASCII, uint64_t>
> KmerReverseBenchmarkTypes;
//typedef ::testing::Types<
//    ::bliss::common::Kmer< 192, bliss::common::DNA,   uint64_t>,
//     ::bliss::common::Kmer< 96, bliss::common::DNA16,   uint64_t>
//> KmerReverseBenchmarkTypes;
INSTANTIATE_TYPED_TEST_CASE_P(Bliss, KmerReverseBenchmark, KmerReverseBenchmarkTypes);


