#include "router.hh"
#include "debug.hh"

#include <iostream>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

       string prefix = Address::from_ipv4_numeric(route_prefix).ip();
       if(prefix_length < 32){
        uint32_t mask = prefix_length == 0 ? 0 : 0xFFFFFFFF << (32 - prefix_length);
        uint32_t temp = route_prefix & mask;
        prefix = Address::from_ipv4_numeric(temp).ip();

        vector<size_t> idx;
        for(size_t i = 0 ; i < prefix.size() ; ++i){
          if(prefix[i] == '.'){
            idx.push_back(i);
          }
        }

        if(prefix_length <= 8){
          prefix = {prefix.begin(), prefix.begin() + idx[0]};
        }else if(prefix_length <= 16){
          prefix = {prefix.begin(), prefix.begin() + idx[1]};
        }else if(prefix_length <= 24){
          prefix = {prefix.begin(), prefix.begin() + idx[2]};
        }

       }
      //  cout << "hi! i am " << prefix << endl;

       routing_table.insert({prefix + std::to_string(prefix_length), {next_hop, interface_num}});
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for(auto ptr : interfaces_){
    auto &q = ptr->datagrams_received();
    while(!q.empty()){
      auto dgram = q.front();
      uint32_t ip = dgram.header.dst;
      q.pop();

      if (dgram.header.ttl <= 1) {
        continue; // 丢弃这个包
      }

      for(uint8_t i = 0 ; i <= 32 ; ++i){
        if(i != 0) ip &= ~(1 << (i - 1));
        string temp = Address::from_ipv4_numeric(ip).ip();

        vector<size_t> idx;
        for(size_t j = 0 ; j < temp.size() ; ++j){
          if(temp[j] == '.'){
            idx.push_back(j);
          }
        }

        if(i >= 24){
          temp = {temp.begin(), temp.begin() + idx[0]};
        }else if(i >= 16){
          temp = {temp.begin(), temp.begin() + idx[1]};
        }else if(i >= 8){
          temp = {temp.begin(), temp.begin() + idx[2]};
        }

        auto found = routing_table.find(temp + std::to_string(32 - i));

        if(found != routing_table.end()){
          --dgram.header.ttl;
          dgram.header.compute_checksum();
          auto p = found->second;
          if(!p.first.has_value()) interfaces_[p.second]->send_datagram(dgram, Address::from_ipv4_numeric(dgram.header.dst));
          else interfaces_[p.second]->send_datagram(dgram, p.first.value());
          break;
        }
      }
    }
  }
}
