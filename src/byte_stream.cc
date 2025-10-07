#include <cassert>
#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : buffer(2 * capacity), 
                                              writer_pos(0), 
                                              reader_pos(0), 
                                              capacity_( capacity ),
                                              wn(0),
                                              rn(0), 
                                              error_(false),
                                              writer_finished(false){}

void Writer::push(const string& data)
{
  uint64_t n = std::min<uint64_t>(data.size(), available_capacity());
  if(n == 0) return;
  uint64_t start = writer_pos % buffer.capacity();
  uint64_t left = buffer.capacity() - start;

  if(n <= left){
    std::copy(data.begin(), data.begin() + n, buffer.begin() + start);
  }else{
    std::copy(data.begin(), data.begin() + left, buffer.begin() + start);
    std::copy(data.begin() + left, data.begin() + n, buffer.begin());
  }
  
  wn += n;
  writer_pos += n;

  check();
}

void Writer::close()
{
  writer_finished = true;
  check();
}

bool Writer::is_closed() const
{
  return writer_finished;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - (writer_pos - reader_pos);
}

uint64_t Writer::bytes_pushed() const
{
  return wn;
}

string_view Reader::peek() const
{
  uint64_t len = writer_pos - reader_pos;

  if(len == 0) return {};

  uint64_t start = reader_pos % buffer.capacity();
  uint64_t left = buffer.capacity() - start;
  uint64_t n = std::min<uint64_t>(len , left);
  string_view result(&buffer[start], n);
  
  return result;
}

void Reader::pop( uint64_t len )
{
  uint64_t n = std::min<uint64_t>(len, bytes_buffered());

  rn += n;
  reader_pos += n;

  check();
}

bool Reader::is_finished() const
{
  return writer_finished && (bytes_buffered() == 0);
}

uint64_t Reader::bytes_buffered() const
{
  return writer_pos - reader_pos;
}

uint64_t Reader::bytes_popped() const
{
  return rn;
}

void ByteStream::check(){
  assert(writer_pos >= reader_pos);
  assert(writer_pos - reader_pos <= capacity_);
}