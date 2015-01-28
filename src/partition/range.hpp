/**
 * @file    range.hpp
 * @ingroup bliss::partition
 * @author  Tony Pan
 * @brief   Generic representation of an interval on a 1D data structure
 * @details Represents an interval with start, end, and overlap length.
 *   Convenience functions are provided to compute an page aligned starting position that is
 *      suitable for the underlying data storage device, such as disk. however, page aligned start value
 *      is not stored here.
 *
 *
 * @copyright BLISS: Copyright (c) Georgia Institute of Technology.  All Rights Preserved.
 *
 * TODO add Licence
 *
 */

#ifndef RANGE_HPP_
#define RANGE_HPP_

#include <stdexcept>
#include <iostream>       // for printing to ostream
#include <limits>         // for numeric limits
#include <algorithm>      // for min/max

namespace bliss
{
  /**
   * @namespace partition
   */
  namespace partition
  {

    /**
     * @class range
     * @brief   Generic representation of an interval on a 1D data structure
     * @details Range specified with offsets and overlap.  specific for 1D.  overlap is on END side only, and is included in the END.
     *          Works for continuous value ranges (floating points).
     *
     * @note    All calculations include the overlap regions, as data in overlap region should undergo the same computation as the non-overlap regions.
     *          overlap region length is included as metadata for the application.
     *
     * @tparam T  data type used for the start and end offsets and overlap.
     */
    template<typename T>
    struct range
    {
        typedef T ValueType;

        /**
         * @var   start
         * @brief starting position of a range in absolute coordinates
         */
        T start;

        /**
         * @var     end
         * @brief   end position of a range in absolute coordinates.  DOES include overlap.
         * @details End points to 1 position past the last element in the range
         */
        T end;

        /**
         * @var   overlap
         * @brief number of elements at the "end" side of a range that is also part of the next range (at the "start" side)
         * @note  "overlap" is the term used to describe a replicated region that is often used to minimize communication between processes.
         *        In parallel and distributed image analysis, this is often called "ghost region"
         *        also note that each time the range is partitioned, a new overlap region should be specified for the subrange
         */
        T overlap;

        /**
         * @brief   construct directly from start and end offsets and overlap
         * @details _start should be less than or equal to _end
         *
         * @param[in] _start    starting position of range in absolute coordinates
         * @param[in] _end      ending position of range in absoluate coordinates.
         * @param[in] _overlap    amount of overlap between adjacent ranges.  optional
         */
        range(const T &_start, const T &_end, const T &_overlap = 0)
            : start(_start), end(_end), overlap(_overlap)
        {
          if (_end < _start)
            throw std::invalid_argument("ERROR: range constructor: end is less than start");
          if (_overlap < 0)
            throw std::invalid_argument("ERROR: range constructor: overlap is less than 0");
        }

        /**
         * @brief   copy construct from the field values of another range
         * @param[in] other   the range object to copy from
         */
        range(const range<T> &other)
            : start(other.start), end(other.end), overlap(other.overlap)
        {}

        //============= move constructor and move assignment operator
        //  NOTE: these are NOT defined because they would take more ops than the copy constructor/assignment operator.


        /**
         * @brief   default constructor.  construct an empty range, with start and end initialized to 0.
         */
        range() : start(0), end(0), overlap(0) {}


        /**
         * @brief assignment operator.  copy the field values from the operand range.
         *
         * @param[in] other   the range object to copy from
         * @return            the calling range object, with field values copied from the operand range object
         */
        range<T>& operator =(const range<T> & other)
        {
          start = other.start;
          end = other.end;
          overlap = other.overlap;
          return *this;
        }

        /**
         * @brief static equals function.  compares 2 ranges' start and end positions.
         *
         * @param[in] other   The range to compare to
         * @return            true if 2 ranges are same.  false otherwise.
         */
        static bool equal(const range<T> &self, const range<T> &other)
        {
          // same if the data range is identical and step is same.

          return (self.start == other.start) && (self.end == other.end);
        }

        /**
         * @brief equals function.  compares this range's start and end positions to the "other" range. (excluding overlap region)
         *
         * @param[in] other   The range to compare to
         * @return            true if 2 ranges are same.  false otherwise.
         */
        bool equal(const range<T> &other) const
        {
          // same if the data range is identical and step is same.

          return range<T>::equal(*this, other);
        }

        /**
         * @brief equals operator.  compares 2 ranges' start and end positions only. (excluding overlap region)
         *
         * @param[in] other   The range to compare to
         * @return            true if 2 ranges are same.  false otherwise.
         */
        bool operator ==(const range<T> &other) const
        {
          // same if the data range is identical and step is same.

          return range<T>::equal(*this, other);
        }



        /**
         * @brief Union of "this" range with "other" range, updates "this" range in place.
         * @details   Given 2 ranges R1 and R2, each with start s, and end e,
         *            then union is defined as R = [min(R1.s, R2.s), max(R1.e, R2.e))
         *            choice of end also chooses the overlap.
         * @note      this call requires that the ranges not to be disjoint.
         *            this is a subset of the set-union definition.
         *
         * @param other   the range to form the union with
         */
        void merge(const range<T>& other) {
          if (this->isDisjoint(other))
             throw std::invalid_argument("ERROR: Range merge() with disjoint range");

          overlap = (end < other.end) ? other.overlap : overlap;
          start = std::min(start,       other.start);
          end   = std::max(end,         other.end);
        }

        /**
         * @brief Static function for merging of 2 range, creating a new range object along the way..
         * @details   Given 2 ranges R1 and R2, each with start s, and end e,
         *            then union is defined as R = [min(R1.s, R2.s), max(R1.e, R2.e))
         *            choice of end also chooses the overlap.
         * @note      this call requires that the ranges not to be disjoint.
         *            this is a subset of the set-union definition.
         *
         * @param first   the first range to form the union with
         * @param second  the second range to form the union with
         * @return        a new range object containing the union of this and "other".
         */
        static range<T> merge(const range<T>& first, const range<T>& second)
        {
          range<T> output(first);
          output.merge(second);
          return output;
        }

        /**
         * @brief Intersection of "this" range with "other" range, updates "this" range in place.
         * @details   Given 2 ranges R1 and R2, each with start s, and end e,
         *            then intersection is defined as R = [max(R1.s, R2.s), min(R1.e, R2.e))
         *            choice of end also chooses the overlap.
         * @note      result may be an empty range, which will contain start = end = min(R1.e, R2.e)
         *
         * @param other   the range to form the intersection with
         */
        void intersect(const range<T>& other)
        {
          overlap =     (end > other.end) ? other.overlap : overlap;
          start =       std::max(start,       other.start);
          end   =       std::min(end,         other.end);

          // in case the ranges do not intersect
          start = std::min(start, end);
        }

        /**
         * @brief Static function for Intersection of 2 ranges, return a new Range object..
         * @details   Given 2 ranges R1 and R2, each with start s, and end e,
         *            then intersection is defined as R = [max(R1.s, R2.s), min(R1.e, R2.e))
         *            choice of end also chooses the overlap.
         * @note      result may be an empty range, which will contain start = end = min(R1.e, R2.e)
         *
         * @param first   the first range to form the intersection with
         * @param second   the second range to form the intersection with
         * @return        updated current range containing the intersection of this and "other".
         */
        static range<T> intersect(const range<T>& first, const range<T>& second)
        {
          range<T> output(first);
          output.intersect(second);
          return output;
        }


//        /**
//         * @brief set-theoretic difference between "this" and "other" ranges.  updates "this" range object.
//         * @details following standard definition of set-theoretic difference, also known as "relative complement".
//         *             Given 2 ranges R1 and R2, each with start s, and end e,
//         *             set-theoretic difference of R1 and R2 is denoted as R1 \ R2, defined as part of R1 not in R2.
//         *                then range_difference is then defined as R = [min(R1.s, R2.s), min(R1.e, R2.s))
//         *    cases: other.start < other.end < start < end  : output should be start     <-> end
//         *           other.start < start < other.end < end  : output should be other.end <-> end
//         *           other.start < start < end < other.end  : output should be start     <-> start        or    end <-> end
//         *           start < other.start < other.end < end  : output should be start     <-> other.start  and  other.end <-> end
//         *           start < other.start < end < other.end  : output should be start     <-> other.start
//         *           start < end < other.start < other.end  : output should be start     <-> end
//         *
//         * NOT DEFINED.  DOCUMENTATION LEFT HERE FOR FUTURE REFERENCE.
//         *
//         * @param other
//         * @return
//         */

        /**
         * @brief Shift range to the right (positive direction on number line) by a specified amount.
         * @details   both the start and end position of the range are incremented by the specified amount
         *            the size does not change.
         * @param amount    the amount to right shift the range by
         */
        void shiftRight(const T& amount)
        {
          start += amount;
          end   += amount;
        }
        /**
         * @brief Shift range to the right (positive direction on number line) by a specified amount.
         * @details   both the start and end position of the range are incremented by the specified amount
         *            the size does not change.
         * @param amount    the amount to right shift the range by
         * @return          new range with updated start and end
         */
        static range<T> shiftRight(const range<T>& r, const T& amount)
        {
          range<T> output(r);
          output.shiftRight(amount);
          return output;
        }

        /**
         * @brief Shift range to the right (positive direction on number line) by a specified amount.
         * @details   both the start and end position of the range are incremented by the specified amount
         *            the size does not change.
         * @param amount    the amount to right shift the range by
         * @return          updated range object.
         */
        range<T>& operator +=(const T& amount)
        {
          this->shiftRight(amount);
          return *this;
        }
        /**
         * @brief Shift range to the right (positive direction on number line) by a specified amount.
         * @details   both the start and end position of the range are incremented by the specified amount
         *            the size does not change.
         * @param amount    the amount to right shift the range by
         * @return          new range with updated start and end
         */
        range<T> operator +(const T& amount) const
        {
          range<T> output(*this);
          output.shiftRight(amount);
          return output;
        }


        /**
         * @brief Shift range to the left (negative direction on number line) by a specified amount.
         * @details   both the start and end position of the range are decremented by the specified amount
         *            the size does not change.
         * @param amount    the amount to left shift the range by
         */
        void shiftLeft(const T& amount)
        {
          start -= amount;
          end   -= amount;
        }
        /**
         * @brief Shift range to the left (negative direction on number line) by a specified amount.
         * @details   both the start and end position of the range are incremented by the specified amount
         *            the size does not change.
         * @param amount    the amount to left shift the range by
         * @return          new range with updated start and end
         */
        static range<T> shiftLeft(const range<T>& r, const T& amount)
        {
          range<T> output(r);
          output.shiftLeft(amount);
          return output;
        }

        /**
         * @brief Shift range to the left (negative direction on number line) by a specified amount.
         * @details   both the start and end position of the range are decremented by the specified amount
         *            the size does not change.
         * @param amount    the amount to left shift the range by
         * @return          updated range object.
         */
        range<T>& operator -=(const T& amount)
        {
          this->shiftLeft(amount);
          return *this;
        }
        /**
         * @brief Shift range to the left (negative direction on number line) by a specified amount.
         * @details   both the start and end position of the range are incremented by the specified amount
         *            the size does not change.
         * @param amount    the amount to left shift the range by
         * @return          new range with updated start and end
         */
        range<T> operator -(const T& amount)
        {
          range<T> output(*this);
          output.shiftLeft(amount);
          return output;
        }

        /**
         * @brief Determines if the other range is completely inside this range, and the other range is not a zero-length one.
         * @details       Given 2 ranges R1 and R2, each with start s, and end e,
         *                R1 contains R2 if R1.s <= R2.s, and R1.e >= R2.e
         *
         * @note          Note that this is not communicative.
         *                overlap regions are included in the comparison.
         * @param other   The range object that may be inside this one.
         * @return        bool, true if other is inside this range.
         */
        bool contains(const range<T> &other) const {
          return (other.size<T>() > 0) && (other.start >= this->start) && (other.end <= this->end);
        }

        /**
         * @brief Determines if this range overlaps the other range.
         * @details       Given 2 ranges R1 and R2, each with start s, and end e,
         *                R1 overlaps R2 if the intersection of R1 and R2 has non-zero size.
         * @note          this is communicative.
         *                overlap region is included.
         * @param other   The range object that may be overlapping this one.
         * @return        bool, true if other overlaps this range.
         */
        bool overlaps(const range<T> &other) const {
          return range<T>::intersect(*this, other).size() > 0;
        }

        /**
         * @brief Determines if this range is adjacent to the other range.
         * @details       Given 2 ranges R1 and R2, each with start s, and end e,
         *                R1 is adjacent to R2 if R1.s == R2.e || R2.s == R1.e.
         *                This includes the "overlap" within each range.
         * @note          this is communicative.
         *
         * @param other   The range object that may be adjacent to this one.
         * @return        bool, true if other is adjacent to this range.
         */
        bool isAdjacent(const range<T> &other) const {
          return (this->start == other.end) || (this->end == other.start);
        }

        /**
         * @brief Determines if this range is disjoint from the other range.
         * @details       Given 2 ranges R1 and R2, each with start s, and end e,
         *                R1 is joint to R2 if R1.e < R2.s || R2.e < R1.s.
         *                This includes the "overlap" within each range.
         *                Note that this is communicative.
         *
         * @param other   The range object that may be disjoint from this one.
         * @return        bool, true if other is disjoint from this range.
         */
        bool isDisjoint(const range<T> &other) const {
          return (this->start > other.end) || (this->end < other.start);
        }


        /**
         * @brief   Static function.  align the range to underlying block boundaries, e.g. disk page size.  only for integral types
         * @details range is aligned to underlying block boundaries by moving the block_start variable back towards minimum
         *    if range start is too close to the data type's minimum, then assertion is thrown.
         * @tparam TT							type of values for start/end.  used to check if type is integral.
         * @param[in] start       the start position of the range to be aligned.
         * @param[in] page_size   the size of the underlying block.
         * @return                the page aligned start position of the range.
         */
        template<typename TT = T>
        static typename std::enable_if<std::is_integral<TT>::value, TT >::type align_to_page(const TT& start, const size_t &page_size)
        {
          if (page_size == 0) throw std::invalid_argument("ERROR: range align_to_page: page size specified as 0.");

          // change start to align by page size.  extend range start.
          // note that if output.start is negative, it will put block_start at a bigger address than the start.
          TT block_start = (start / page_size) * page_size;

          if (block_start > start)  // only enters if start is negative.
          {

            // if near lowest possible value, then we can't align further.  assert this situation.
            if ((block_start - std::numeric_limits<TT>::lowest()) < page_size)
              throw std::range_error("ERROR: range align_to_page: start is within a single page size of a signed data type minimum. cannot align page.");

            // deal with negative start position.
            block_start = block_start - page_size;
          }
          // leave end as is.

          return block_start;
        }

        /**
         * @brief   Static function.  align the range to underlying block boundaries, e.g. disk page size.  only for integral types
         * @details range is aligned to underlying block boundaries by moving the block_start variable back towards minimum
         *    if range start is too close to the data type's minimum, then assertion is thrown.
         * @tparam TT             type of values for start/end.  used to check if type is integral.
         * @param[in] r           the range to be aligned.
         * @param[in] page_size   the size of the underlying block.
         * @return                the page aligned start position of the range.
         */
        template<typename TT = T>
        static typename std::enable_if<std::is_integral<TT>::value, TT >::type align_to_page(const range<TT>& r, const size_t &page_size)
        {
          return range<T>::align_to_page(r.start, page_size);
        }

//        /**
//         * @brief   Member function.  align the range to underlying block boundaries, e.g. disk page size.  only for integral types
//         * @details range is aligned to underlying block boundaries by moving the block_start variable back towards minimum
//         *    if range start is too close to the data type's minimum, then assertion is thrown.
//         * @tparam TT             type of values for start/end.  used to check if type is integral.
//         * @param[in] page_size   the size of the underlying block.
//         * @return                the page aligned start position of the range.
//         */
//        template<typename TT = T>
//        typename std::enable_if<std::is_integral<TT>::value, TT >::type align_to_page(const size_t &page_size) const
//        {
//          return range<T>::align_to_page(this->start, page_size);
//        }

        /**
         * @brief     Static function. check to see if the start position has been aligned to underlying block boundary.   only for integral types
         *
				 * @tparam		TT					type of start/end.  used to check if type is integral
         * @param[in] start       the start position of the range to be cehcked for alignment.
         * @param[in] page_size   the size of the underlying block.
         * @return                true if range is block aligned, false otherwise.
         */
        template<typename TT = T>
        static typename std::enable_if<std::is_integral<TT>::value, bool >::type is_page_aligned(const TT& start, const size_t &page_size)
        {
          return (start % page_size) == 0;
        }

        /**
         * @brief     Static function. check to see if the start position has been aligned to underlying block boundary.   only for integral types
         *
         * @tparam    TT          type of start/end.  used to check if type is integral
         * @param[in] r           the range to be checked for alignment.
         * @param[in] page_size   the size of the underlying block.
         * @return                true if range is block aligned, false otherwise.
         */
        template<typename TT = T>
        static typename std::enable_if<std::is_integral<TT>::value, bool >::type is_page_aligned(const range<TT>& r, const size_t &page_size)
        {
          return range<T>::is_page_aligned(r.start, page_size);
        }

//        /**
//         * @brief     Member function. check to see if the start position has been aligned to underlying block boundary.   only for integral types
//         *
//         * @tparam    TT          type of start/end.  used to check if type is integral
//         * @param[in] r           the range to be checked for alignment.
//         * @param[in] page_size   the size of the underlying block.
//         * @return                true if range is block aligned, false otherwise.
//         */
//        template<typename TT = T>
//        typename std::enable_if<std::is_integral<TT>::value, bool >::type is_page_aligned(const size_t &page_size)
//        {
//          return range<T>::is_page_aligned(this->start, page_size);
//        }


        /**
         * @brief   get the integral size of the range between [start, end), including the overlap region
         * @return  unsigned length of the range.
         */
        template<typename TT=T>
        typename std::enable_if<std::is_integral<TT>::value, size_t>::type size() const
        {
          return static_cast<size_t>(end) - static_cast<size_t>(start);
        }

        /**
         * @brief   get the floating point size of the range between [start, end), including the overlap region
         * @return  length of the range.
         */
        template<typename TT=T>
        typename std::enable_if<std::is_floating_point<TT>::value, TT>::type size() const
        {
          return end - start;
        }


    };

    /**
     * @brief << operator to write out range object's fields.  Signed integral version
     * @tparam  T           type of values used by Range internally.  This is type deduced by compiler.
     * @param[in/out] ost   output stream to which the content is directed.
     * @param[in]     r     range object to write out
     * @return              output stream object
     */
    template<typename T>
    typename std::enable_if<std::is_signed<T>::value and std::is_integral<T>::value, std::ostream>::type& operator<<(std::ostream& ost, const range<T>& r)
    {
      ost << "range: block [" << static_cast<int64_t>(r.start) << ":" << static_cast<int64_t>(r.end) << ") overlap " << static_cast<int64_t>(r.overlap);
      return ost;
    }

    /**
     * @brief << operator to write out range object's fields.  Unsigned integral version
     * @tparam  T           type of values used by Range internally.  This is type deduced by compiler.
     * @param[in/out] ost   output stream to which the content is directed.
     * @param[in]     r     range object to write out
     * @return              output stream object
     */
    template<typename T>
    typename std::enable_if<!std::is_signed<T>::value and std::is_integral<T>::value, std::ostream>::type& operator<<(std::ostream& ost, const range<T>& r)
    {
      ost << "range: block [" << static_cast<uint64_t>(r.start) << ":" << static_cast<uint64_t>(r.end) << ") overlap " << static_cast<uint64_t>(r.overlap);
      return ost;
    }

    /**
     * @brief << operator to write out range object's fields.  floating point version
     * @tparam  T           type of values used by Range internally.  This is type deduced by compiler.
     * @param[in/out] ost   output stream to which the content is directed.
     * @param[in]     r     range object to write out
     * @return              output stream object
     */
    template<typename T>
    typename std::enable_if<std::is_floating_point<T>::value, std::ostream>::type& operator<<(std::ostream& ost, const range<T>& r)
    {
      ost << "range: block [" << static_cast<double>(r.start) << ":" << static_cast<double>(r.end) << ") overlap " << static_cast<double>(r.overlap);
      return ost;
    }


  } /* namespace partition */
} /* namespace bliss */
#endif /* RANGE_HPP_ */
