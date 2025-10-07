#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32(n + zero_point.raw_value_);
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint32_t dis;
  // 第一个点，可能减溢出
  if(raw_value_ < zero_point.raw_value_) dis = (1UL << 32) - (zero_point.raw_value_ - raw_value_);
  else dis = raw_value_ - zero_point.raw_value_;

  uint64_t base = (checkpoint & 0xFFFFFFFF00000000) + dis;

  // 第二个点是可能在起始
  uint64_t ret = base;
  if (checkpoint > base && checkpoint - base > (1UL << 31) ) {
    ret = base + (1UL << 32);
  } else if (base > checkpoint && base - checkpoint > (1UL << 31) && base >= (1UL << 32) ) {
    ret = base - (1UL << 32);
  }
  return ret;
}
