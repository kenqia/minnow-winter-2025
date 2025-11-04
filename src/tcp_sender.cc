#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  uint64_t ret = 0;

  if(!sections.empty()){
    for(auto pair : sections){
      // cout << pair.second.payload << " : " << pair.second.payload.size() << endl;
      ret += pair.second.sequence_length();
    } 
  }
  return ret;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return retransmit_cnt;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  
  if(!window_size.has_value()){
    // cout << "break 1" << endl;
    return;
  }

  if(window_size.value() < sequence_numbers_in_flight()){
    // cout << "break 2" << endl;
    return;
  }
  
  uint16_t available_window = window_size.value() - sequence_numbers_in_flight();
    
  uint64_t stream_idx = input_.reader().bytes_popped();
  uint64_t abs_seqno = (stream_idx == 0 && !SYN) ? 0 : stream_idx + 1;
  Wrap32 seqno = Wrap32::wrap(abs_seqno, isn_);
  uint16_t len, i = 0;
  std::string payloads;

  if(input_.has_error()){
    TCPSenderMessage msg;
    msg.RST = true;
    msg.seqno = seqno;
    transmit(move(msg));
    return;
  }

  if(window_size.value() == 0){
    if(sequence_numbers_in_flight() > 0){
      // cout << "no need do that just wait" << endl;
      return;
    }
    available_window = 1;
  }

  len = available_window;

  if(abs_seqno == 0){
    SYN = true;
    len = len == 0 ? 0 : len - 1;
  }
  
  read(input_.reader(), len, payloads);
  len = std::min<uint16_t>(payloads.size(), len);
  // len应该是读多少负载
  // cout << "len: " << len  << "window_size : " << window_size.value() << endl;
  //拆成段
  while(available_window > 0){
    TCPSenderMessage msg;
    uint16_t n;

    if(abs_seqno == 0){
      msg.SYN = true;
      SYN = true;
      available_window = available_window == 0 ? 0 : available_window - 1;
    }

    n = std::min<uint16_t>(TCPConfig::MAX_PAYLOAD_SIZE, len - i);

    // assign payload safely only if n > 0
    if (n > 0) {
      msg.payload.assign(payloads.data() + i, n);
    } else {
      msg.payload.clear();
    }
    msg.seqno = seqno;

    i += n;
    available_window -= n;

    // len 本质上是我要发多少payload，首先要reader结束， 我是最后一个或者不发payload，并且window_size不为空，并且空间还够
    if(input_.reader().is_finished() && !FIN && (i >= len) && (available_window > 0)){
      msg.FIN = true;
      FIN = true;
      available_window -= 1;
    }

    if(msg.FIN == false && msg.SYN == false && msg.payload.empty()) break;

    sections.insert({abs_seqno, msg});
    seqno = seqno + msg.sequence_length();
    abs_seqno += msg.sequence_length();
    transmit(move(msg));

    if (!timer_start) {
      timer_start = true;
      ticks = 0;
    }
  }
  // cout << "len: " << len  << "window_size : " << window_size.value() << endl;
  // (void)transmit;
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;

  if(input_.has_error()){
    msg.RST = true;
  }

  uint64_t stream_idx = input_.reader().bytes_popped();
  uint64_t abs_seqno = (stream_idx == 0 && !SYN) ? 0 : stream_idx + 1;
  Wrap32 seqno = Wrap32::wrap(abs_seqno, isn_);
  if(FIN){
    seqno = seqno + 1;
  }
  msg.seqno = seqno;

  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{

  window_size.value() = msg.window_size;

  if(msg.RST){
    input_.set_error();
    return;
  }


  if(!msg.ackno.has_value()){
    // cout << msg.window_size << "wait, do you want me to sent it again???" << endl;
    return;
  }

  bool flag = false;
  uint64_t abs_seqno = msg.ackno.value().unwrap(isn_, input_.reader().bytes_popped());

  if(sections.empty()) return;
  auto temp = --sections.end();
  if(abs_seqno > temp->first + temp->second.sequence_length()){
    // cout << "broken" << endl;
    return;
  }


  for (auto itr = sections.begin(); itr != sections.end(); ) {
      uint64_t seg_end = itr->first + itr->second.sequence_length();
      // cout << "first : " << seg_end << "abs: " << abs_seqno << endl;
      if (seg_end <= abs_seqno) {
          itr = sections.erase(itr);
          flag = true;
      } else {
          ++itr;
      }
  }
  // cout << sections.empty() << endl;
  if(flag){
    RTO = initial_RTO_ms_;
    ticks = 0;
    retransmit_cnt = 0;
    timer_start = !sections.empty();
  }
  // (void)msg;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if(!timer_start) return;

  ticks += ms_since_last_tick;

  // cout << "ticks : " << ticks << " retransimits_cnt :" << retransmit_cnt << endl;

  if(ticks >= RTO){
    // cout << "retransmit hey! ticks : "<< ticks << " retransimits_cnt :" << retransmit_cnt << endl;
    if(sections.empty()){
      // cout << "idk what's going on" << endl;
      return;
    }
    transmit(sections.begin()->second);
    ticks = 0;

    if(window_size.has_value() && window_size.value() > 0){
      retransmit_cnt += 1;
      RTO *= 2;
    }
    timer_start = true;
  }

  // (void)transmit;
}
