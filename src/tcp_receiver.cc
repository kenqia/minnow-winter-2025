#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if(message.RST){
    cout << "HELLO? " << endl;
    reassembler_.reader().set_error();
    return;
  }

  if(message.SYN && !zero_point.has_value()){
    zero_point = Wrap32(message.seqno);
  }

  if(!zero_point.has_value()){
    // cout << "pls get SYN " << endl;
    return;
  }
  // size_t len = message.sequence_length();
  uint64_t checkpoint = reassembler_.writer().bytes_pushed();
  uint64_t absolute_seqno = message.seqno.unwrap( zero_point.value(), checkpoint );
  uint64_t stream_idx = message.SYN ? 0 : absolute_seqno - 1;

  reassembler_.insert(stream_idx, message.payload, message.FIN);
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage msg;
  msg.RST = false;

  if(reassembler_.writer().has_error() || reassembler_.reader().has_error()){
    msg.RST = true;
    return msg;
  }

  uint64_t next = reassembler_.writer().bytes_pushed();
  uint64_t absolute_seqno = next + 1;

  if(zero_point.has_value()){
    msg.ackno = Wrap32::wrap(absolute_seqno, zero_point.value());
  
    if(reassembler_.writer().is_closed()) msg.ackno = msg.ackno.value() + 1;
  }

  uint64_t capacity = reassembler_.writer().available_capacity();
  msg.window_size = std::min( capacity, static_cast<uint64_t>( UINT16_MAX ) );
  return msg;
}
