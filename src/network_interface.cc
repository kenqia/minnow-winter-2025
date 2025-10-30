#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t next_ip = next_hop.ipv4_numeric();
  auto found = ARP_cache.find(next_ip);

  if(found == ARP_cache.end()){

    if(ARP_sent_cnt.find(next_ip) == ARP_sent_cnt.end()){
      EthernetFrame bc {};
      ARPMessage ARPmsg {};
      bc.header.src = this->ethernet_address_;
      bc.header.dst = ETHERNET_BROADCAST;
      bc.header.type = EthernetHeader::TYPE_ARP;

      ARPmsg.opcode = ARPMessage::OPCODE_REQUEST;
      ARPmsg.sender_ethernet_address = this->ethernet_address_;
      ARPmsg.sender_ip_address = this->ip_address_.ipv4_numeric();
      ARPmsg.target_ethernet_address = {};
      ARPmsg.target_ip_address = next_ip;
      
      bc.payload = serialize(ARPmsg);
      transmit(bc);
      ARP_sent_cnt[next_ip] = current_time_ms_;
    }

    waiting_queue[next_ip].push(dgram);
    return;
  }

  EthernetHeader Ehead {};
  EthernetFrame ret {};

  Ehead.src = this->ethernet_address_;
  Ehead.dst = found->second;
  Ehead.type = EthernetHeader::TYPE_IPv4;
  ret.header = move(Ehead);
  ret.payload = serialize(dgram);

  transmit(ret);
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  if(frame.header.dst == this->ethernet_address_ || frame.header.dst == ETHERNET_BROADCAST){
    if(frame.header.type == EthernetHeader::TYPE_IPv4){
      InternetDatagram dgram {};
      if(!parse(dgram, frame.payload)) return;

      datagrams_received_.push(dgram);
    }else if(frame.header.type == EthernetHeader::TYPE_ARP){
      ARPMessage ARPmsg {};
      if(!parse(ARPmsg, frame.payload) || !ARPmsg.supported()) return;
      
      ARP_cache.insert({ARPmsg.sender_ip_address, ARPmsg.sender_ethernet_address}); //好像还要计时
      ARP_cache_cnt[ARPmsg.sender_ip_address] = current_time_ms_;

      auto &wqueue = waiting_queue[ARPmsg.sender_ip_address];
      while(!wqueue.empty()){
        EthernetFrame Ef {};
        EthernetHeader Ehead {};
        EthernetAddress target = ARP_cache[ARPmsg.sender_ip_address];
        InternetDatagram dgram =  wqueue.front();
        wqueue.pop();

        Ehead.src = this->ethernet_address_;
        Ehead.dst = target;
        Ehead.type = EthernetHeader::TYPE_IPv4;
        Ef.header = move(Ehead);
        Ef.payload = serialize(dgram);
        transmit(Ef);
      }

      if(ARPmsg.target_ip_address == this->ip_address_.ipv4_numeric() && ARPmsg.opcode == ARPMessage::OPCODE_REQUEST){
        EthernetFrame bc {};
        ARPMessage AmsgS {};
        bc.header.src = this->ethernet_address_;
        bc.header.dst = ARPmsg.sender_ethernet_address;
        bc.header.type = EthernetHeader::TYPE_ARP;

        AmsgS.opcode = ARPMessage::OPCODE_REPLY;
        AmsgS.sender_ethernet_address = this->ethernet_address_;
        AmsgS.sender_ip_address = this->ip_address_.ipv4_numeric();
        AmsgS.target_ethernet_address = ARPmsg.sender_ethernet_address;
        AmsgS.target_ip_address = ARPmsg.sender_ip_address;
        
        bc.payload = serialize(AmsgS);
        transmit(bc);
      }
    }
  }

  return;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  current_time_ms_ += ms_since_last_tick;

  std::vector<uint32_t> deleted {};
  for(auto p : ARP_sent_cnt){
    if(current_time_ms_ - p.second > 5000){
      deleted.push_back(p.first);
    }
  }
  for(auto elm : deleted){
    ARP_sent_cnt.erase(elm);

    waiting_queue.erase(elm);
  }

  deleted.clear();

  for(auto p : ARP_cache_cnt){
    if(current_time_ms_ - p.second > 30000){
      deleted.push_back(p.first);
    }
  }

  for(auto elm : deleted){
    ARP_cache_cnt.erase(elm);
    ARP_cache.erase(elm);
  }
}
