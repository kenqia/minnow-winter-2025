#include "socket.hh"
#include <arpa/inet.h>

using namespace std;

class RawSocket : public DatagramSocket
{
public:
  RawSocket() : DatagramSocket( AF_INET, SOCK_RAW, IPPROTO_RAW ) {}
};

struct iphdr {
    uint8_t  ihl:4, version:4;
    uint8_t  tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
} __attribute__((packed));

struct udph{
  uint16_t sPort;
  uint16_t dPort;
  uint16_t len;
  uint16_t checksum;
} __attribute__((packed));

struct PseudoUdpHeader {
    uint32_t saddr;
    uint32_t daddr;
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t udp_len;
} __attribute__((packed));

uint16_t checksum(const void* data, size_t len) {
    uint32_t sum = 0;
    const uint16_t* ptr = reinterpret_cast<const uint16_t*>(data);

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len > 0) {
        sum += *reinterpret_cast<const uint8_t*>(ptr);
    }

    // fold 32-bit sum to 16 bits
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return static_cast<uint16_t>(~sum);
}


int main()
{
  Address adr("127.0.0.1");
  RawSocket rs;
  struct iphdr ip;
  struct udph udp;

  rs.connect(adr);

  ip.version = 4;
  ip.ihl = 5;
  ip.tos = 0;
  std::string payload_data = "heyhyehyehyehey!! what's going on!!!";
  ip.tot_len = htons(sizeof(ip) + sizeof(udp) + payload_data.size());
  ip.id = htons(12345);
  ip.frag_off = 0;
  ip.ttl = 64;
  ip.protocol = 17;
  ip.saddr = inet_addr("127.0.0.1");
  ip.daddr = inet_addr("127.0.0.1");
  ip.check = 0;
  ip.check = checksum(&ip, sizeof(ip));

  udp.sPort = htons(22);
  udp.dPort = htons(33);
  udp.len = htons(sizeof(udp) + payload_data.size());
  udp.checksum = 0;

  struct PseudoUdpHeader psh;
  psh.saddr = ip.saddr;
  psh.daddr = ip.daddr;
  psh.zero = 0;
  psh.protocol = ip.protocol;
  psh.udp_len = udp.len;

  std::string udp_checksum_buf;
  udp_checksum_buf.append(reinterpret_cast<const char*>(&psh), sizeof(psh));
  udp_checksum_buf.append(reinterpret_cast<const char*>(&udp), sizeof(udp));
  udp_checksum_buf.append(payload_data);

  udp.checksum = checksum(&udp_checksum_buf, sizeof(udp_checksum_buf));

  std::string request;

  request.append(reinterpret_cast<const char*>(&ip), sizeof(ip));
  request.append(reinterpret_cast<const char*>(&udp), sizeof(udp));
  request.append(payload_data);

  rs.send(request);

  return 0;
}
