/**
 * indexing.cpp
 *
 *  Created on: Jan 15, 2014
 *      Author: tpan
 *
 *  Description:  testing code to try different ways of generating the index key from a collection of strings
 *
 */

#include <vector>
#include <random>
#include <cmath>
#include <numeric>
#include <functional>

#include <cstdint>
#include <limits>

#include <exception>

#include "config.hpp"
#include "utils/logging.h"

#if defined(USE_OPENMP)
#include <omp.h>
#endif

// for debugging only
#include <chrono>
#include <iostream>
#include <fstream>
#include <bitset>
#include <cassert>

/**
 * parameters
 */

// packed are filled from left to right
// TODO: support big endian....
typedef uint16_t ReadLengthType; // length of each read.  chosen based on sequencer capability
typedef uint64_t ReadCountType; // total number of reads.  chosen based on sequencer capability
typedef uint64_t KeyType; // keys.  chosen based on k and alphabet size.  k can be related to genome size.
typedef uint16_t WordType; // multiple character packing.  chosen based on alphabet size and convenient machine capability
typedef uint64_t ReadBaseCountType; // total number of bases in this round. basically total number of reads * average read length.
// this is also the total number of kmers.

constexpr uint8_t WORD_BITS = sizeof(WordType) * 8;
constexpr uint8_t KEY_BITS = sizeof(KeyType) * 8;
constexpr uint8_t N_K = 21;
constexpr uint8_t N_BITS = 3;
constexpr uint8_t K_BITS = N_K * N_BITS;
constexpr uint8_t N_PACKED_CHARS = WORD_BITS / N_BITS;
constexpr uint8_t N_PADDING_BITS = WORD_BITS % N_BITS;
constexpr WordType PADDING_MASK = std::numeric_limits<WordType>::max()
    >> N_PADDING_BITS;
constexpr KeyType KEY_MASK = std::numeric_limits<KeyType>::max()
    >> ((K_BITS == WORD_BITS) ? 0 : (N_K * N_BITS));
constexpr uint8_t N_C = 4;            // core count
constexpr ReadCountType N_R = 125000;

constexpr double L_MEAN = 100.0f;
constexpr double L_STDEV = 7.0f;

typedef std::vector<WordType> SequenceT;

/**
 * generate the lengths
 */
template<int K>
void generateLengths(std::vector<ReadLengthType>& lengths)
{

  std::default_random_engine generator;
  std::normal_distribution<double> distribution(L_MEAN, L_STDEV);

  ReadLengthType l;

  for (ReadCountType i = 0; i < N_R; ++i)
  {
    // generate the random lengths of the reads
    l = static_cast<ReadLengthType>(round(distribution(generator)));

    if (l < 80 || l > 120)
    {
      --i;
      continue;
    }

    lengths[i] = l;
  }

}

/**
 * allocate input vector.
 * supports packed string with padding bits.
 */
void generateStrings(std::vector<ReadLengthType> const & lengths,
                     std::vector<SequenceT>& reads)
{
  std::default_random_engine generator;
  std::uniform_int_distribution<WordType> distribution;

  // use a random number generator to generate the "packed string"
  for (ReadCountType i = 0; i < N_R; ++i)
  {
    // allocate a sequence of packed string blocks.
    SequenceT read((lengths[i] + N_PACKED_CHARS - 1) / N_PACKED_CHARS, 0);

    int idx = 0;
    for (ReadLengthType j = 0; j < lengths[i]; j += N_PACKED_CHARS)
    {
      // packed bits, with padding.  low order bits only.
      read[idx] = distribution(generator) & PADDING_MASK;
      ++idx;
    }
    // the last one may not be completely filled.
    if (lengths[i] % N_PACKED_CHARS > 0)
    {
      WordType keep_mask = std::numeric_limits<WordType>::max()
          >> (WORD_BITS - (lengths[i] % N_PACKED_CHARS) * N_BITS);
      read[idx - 1] = read[idx - 1] & keep_mask;
    }
    read.shrink_to_fit();
    reads[i] = read;
  }

}

/**
 * output vectors
 */

template<int SIMD>
void computeKeys(std::vector<SequenceT> const & seqs,
                 std::vector<ReadLengthType> const & lengths,
                 std::vector<ReadBaseCountType> const & offsets,
                 std::vector<KeyType> & keys)
{
  FATAL("BASE TEMPLATE CALLED. NOT IMPLEMENTED.");
}

template<int SIMD>
void computeKeys(const SequenceT &seq, const ReadLengthType &key_count,
                 std::vector<KeyType>::iterator & key_iter)
{
  FATAL("BASE TEMPLATE CALLED. NOT IMPLEMENTED.");
}

// TODO:  reverse complement key, genome size sequence, putting position, compressed representation of position, etc.
// TODO:  determine cache sizes, total memory size, MPI block size, disk page size, number of sockets, number of cores per socket,

#define SCALAR 0
#define SSE2 1

constexpr uint8_t DIV = N_K / N_PACKED_CHARS;
constexpr uint8_t REM = N_K % N_PACKED_CHARS;
// NOTE: if there is no remainder, then shift or no shift, REM_MASK is not used.
constexpr WordType REM_MASK =
    (REM != 0) ?
        std::numeric_limits<WordType>::max() >> (WORD_BITS - (REM * N_BITS)) :
        0;
constexpr uint8_t SIG_BITS = WORD_BITS - N_PADDING_BITS; // high order bits are set to 0 for padding.
constexpr WordType CHAR_MASK = std::numeric_limits<WordType>::max()
    >> (WORD_BITS - N_BITS);

template<>
void computeKeys<SCALAR>(const SequenceT &seq, const ReadLengthType &key_count,
                         std::vector<KeyType>::iterator & key_iter)
{
  /*============
   * get the first kmer, since it requires looking at the whole kmer
   ============*/
  // only "paste" in the necessary length to avoid overflow.
  KeyType key = 0;
  KeyType temp = 0;

  for (ReadLengthType j = 0; j < DIV; ++j)
  {
    // little endian. so concat to to higher bits
    temp = seq[j];
    key |= temp << (j * SIG_BITS);  // high order bits are set to 0 for padding.
  }
  // now get the remainder if needed
  if (REM > 0)
  {
    temp = seq[DIV] & REM_MASK;
    key |= temp << (DIV * SIG_BITS);  // keep and add only the lower bits
  }
  *key_iter = key;
  ++key_iter;

  /*=============
   * the rest are constructed incrementally
   =============*/
  uint8_t last_block = DIV;  // these point to the "next" positions.
  uint8_t last_block_pos = REM;
  KeyType block = seq[last_block];
  //std::cout << "key_count " << key_count << " last block: " << static_cast<int>(last_block) << " size of seq: "  << seq.size()  << std::endl;
  for (ReadLengthType j = 1; j < key_count; j++)
  { // for each packed char

    // remove the oldest char
    key = key >> N_BITS;

    // append the new char

    temp = block >> (last_block_pos * N_BITS);
    temp &= CHAR_MASK;  // leaving it with only 1 char's bits.
    temp = temp << ((N_K - 1) * N_BITS);
    key |= temp;
    *key_iter = key;
    ++key_iter;

    // increment
    ++last_block_pos;
    if (last_block_pos % N_PACKED_CHARS == 0)
    {
      last_block_pos = 0;
      ++last_block;

      if (last_block > seq.size())
      {
        ERROR(
            "seq size: " << seq.size() << " lastblock " << static_cast<int>(last_block) << " j " << static_cast<int>(j));
        break;
      }
      else if (last_block == seq.size())
      {
        //INFO("seq size: " << seq.size() << " lastblock " << static_cast<int>(last_block) << " j " << static_cast<int>(j));
        break;
      }
      block = seq[last_block];
    }
  }

}

/**
 * serial version
 *
 * NOTE:  this assumes that entire index fits in 64 bits.
 *   this version uses bit shifting.  another way is to do base |alphabet| math.
 */
template<>
void computeKeys<SCALAR>(std::vector<SequenceT> const & seqs,
                         std::vector<ReadLengthType> const & lengths,
                         std::vector<ReadBaseCountType> const & offsets,
                         std::vector<KeyType> & keys)
{
  std::vector<KeyType>::iterator iter;

  //for (int i = 0; i < 10; i++)

  // runtime schedule performed the same as dynamic 0.034s (Release).  static and guided are at 0.020s.  100K reads.
#pragma omp parallel for schedule(static, 1) private(iter)
  for (ReadCountType i = 0; i < N_R; ++i)
  {
//    if (DIV > seqs[i].size())
//    {
//      //ERROR("seq size: " << seqs[i].size() << " lastblock " << static_cast<int>(DIV) << " i " << static_cast<int>(i) );
//    }
//    else if (DIV == seqs[i].size())
//    {
//      //INFO("seq size: " << seqs[i].size() << " lastblock " << static_cast<int>(DIV) << " i " << static_cast<int>(i));
//    }
//    DEBUG("TID: " << omp_get_thread_num() << " i: " << i);

    iter = keys.begin() + offsets[i];
    computeKeys<SCALAR>(seqs[i], lengths[i] - N_K + 1, iter);
  }
}

template<>
void computeKeys<SSE2>(std::vector<SequenceT> const & seqs,
                       std::vector<ReadLengthType> const & lengths,
                       std::vector<ReadBaseCountType> const & offsets,
                       std::vector<KeyType> & keys)
{

}

/**
 * main function.
 */
int main(int argc, char* argv[])
{

#if defined(USE_OPENMP)
  INFO("OpenMP max number of threads is " << omp_get_max_threads());
#endif

  std::chrono::high_resolution_clock::time_point t1, t2;
  std::chrono::duration<double> time_span;

  // allocate
  t1 = std::chrono::high_resolution_clock::now();
  std::vector<ReadLengthType> lengths(N_R, 0);
  t2 = std::chrono::high_resolution_clock::now();
  time_span = std::chrono::duration_cast<std::chrono::duration<double>>(
      t2 - t1);
  INFO(
      "allocate string lengths " << "elapsed time: " << time_span.count() << "s.");
  t1 = std::chrono::high_resolution_clock::now();
  generateLengths<N_K>(lengths);
  t2 = std::chrono::high_resolution_clock::now();
  time_span = std::chrono::duration_cast<std::chrono::duration<double>>(
      t2 - t1);
  INFO(
      "generate string lengths " << "elapsed time: " << time_span.count() << "s.");

  // write out the lengths
  std::ofstream csv;
  csv.open("lengths.csv");
  for (ReadLengthType len : lengths)
  {
    csv << len << "\n";
  }
  csv.close();

  // allocate
  t1 = std::chrono::high_resolution_clock::now();
  std::vector<SequenceT> reads(N_R);
  t2 = std::chrono::high_resolution_clock::now();
  time_span = std::chrono::duration_cast<std::chrono::duration<double>>(
      t2 - t1);
  INFO("allocate strings " << "elapsed time: " << time_span.count() << "s.");
  t1 = std::chrono::high_resolution_clock::now();
  generateStrings(lengths, reads);
  t2 = std::chrono::high_resolution_clock::now();
  time_span = std::chrono::duration_cast<std::chrono::duration<double>>(
      t2 - t1);
  INFO("generate strings " << "elapsed time: " << time_span.count() << "s.");

  // write out the packed strings
  csv.open("string.csv");
  csv << "length=" << lengths[0] << "\n";
  for (WordType w : reads[0])
  {
    csv << std::bitset<WORD_BITS>(w) << "\n";
  }
  csv.close();

  // compute prefix sum of the lengths
  t1 = std::chrono::high_resolution_clock::now();
  std::vector<ReadBaseCountType> offsets(N_R + 1, 0);
  offsets[0] = 0;
  // this is faster than copying and transforming by almost 2x: 100000 elements, 0.0032 vs 0.0017s in debug.  with release (-O4, down to 0.0007)
  for (ReadCountType i = 1; i <= N_R; ++i)
  {
    offsets[i] = offsets[i - 1]
        + static_cast<ReadBaseCountType>(lengths[i - 1] - N_K + 1);
  }
  ReadBaseCountType total = offsets[N_R];
//  std::copy(lengths.begin(), lengths.end()-1, offsets.begin()+1);
//  std::partial_sum(offsets.begin(), offsets.end(), offsets.begin());
  t2 = std::chrono::high_resolution_clock::now();
  time_span = std::chrono::duration_cast<std::chrono::duration<double>>(
      t2 - t1);
  INFO(
      "prefix scan of length " << "elapsed time: " << time_span.count() << "s.");

  // write out the lengths
  csv.open("offsets.csv");
  for (ReadBaseCountType len : offsets)
  {
    csv << len << "\n";
  }
  csv.close();

  t1 = std::chrono::high_resolution_clock::now();
  std::vector<KeyType> keys(total);
  t2 = std::chrono::high_resolution_clock::now();
  time_span = std::chrono::duration_cast<std::chrono::duration<double>>(
      t2 - t1);
  INFO("allocate output " << "elapsed time: " << time_span.count() << "s.");

  // call each and time it.
  t1 = std::chrono::high_resolution_clock::now();
  computeKeys<SCALAR>(reads, lengths, offsets, keys);
  t2 = std::chrono::high_resolution_clock::now();
  time_span = std::chrono::duration_cast<std::chrono::duration<double>>(
      t2 - t1);
#if defined(USE_OPENMP)
  INFO("OMP + SCALAR " << "elapsed time: " << time_span.count() << "s.");
#else
  INFO("SERIAL + SCALAR " << "elapsed time: " << time_span.count() << "s.");
#endif
  // write out the packed strings
  csv.open("keys.csv");
  csv << "length=" << lengths[0] << "\n";
  for (int i = 0; i < lengths[0] - N_K + 1; ++i)
  {
    csv << std::bitset<KEY_BITS>(keys[i]) << "\n";
  }
  csv.close();

  /*
   t1 = std::chrono::high_resolution_clock::now();
   computeKeys<SSE2>(reads, lengths, offsets, keys);
   t2 = std::chrono::high_resolution_clock::now();
   time_span = std::chrono::duration_cast<std::chrono::duration<double>>(
   t2 - t1);
   #if defined(USE_OPENMP)
   INFO("OMP + SSE " << "elapsed time: " << time_span.count() << "s.");
   #else
   INFO("SERIAL + SSE " << "elapsed time: " << time_span.count() << "s.");
   #endif
   */

  return 0;
}
