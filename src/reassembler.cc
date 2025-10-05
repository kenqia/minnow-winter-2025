#include "reassembler.hh"
#include "debug.hh"

using namespace std;

struct piece;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // 1
  // std::cout << "now_idx = " << first_index << "+ data_size = " << data.size()  << "we have " << count_bytes_pending() << std::endl;
  uint64_t availble = output_.writer().available_capacity();
  uint64_t end = first_index + data.size();
  uint64_t limit = next + availble;

  if(is_last_substring){
    eof_idx = end;
  }
  
  if(first_index >= limit) return;
  

  if(end > limit){
    uint64_t left = end - limit;
    data.erase(data.end() - left, data.end());
  }

  if(first_index < next ){
    if(end > next){
      data.erase(data.begin(), data.begin() + next - first_index);
      first_index = next;
    }else{
      return;
    }
  }

  for(auto big = mp.lower_bound(first_index); big != mp.end();big = mp.erase(big)){
    if(end >= big->first){
      uint64_t len = end - big->first;
      if(len >= big->second.size()){
      }else{
        data.erase(data.end() - len, data.end());
        data += std::move(big->second);
      }
    }else{
      break;
    }
  }
  bool flag = true;
  auto small = mp.lower_bound(first_index);
  if(small != mp.begin()){
    small = std::prev(small);
    if(small->first + small->second.size() >= first_index){
      flag = false;
      uint64_t len = small->first + small->second.size() - first_index;
      if(len >= data.size()){
    
      }else{
        small->second.erase(small->second.end() - len, small->second.end());
        small->second += std::move(data);
      }
    }
  }
  if(flag) mp.insert({first_index, data}); 

  while(!mp.empty() && mp.begin()->first == next){
    output_.writer().push(mp.begin()->second);
    next += mp.begin()->second.size();
    mp.erase(mp.begin());
  }

  if(next == eof_idx){
    output_.writer().close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t result = 0;
  for(const auto& s : mp){
    result += s.second.size();
  }
  return result;
}
