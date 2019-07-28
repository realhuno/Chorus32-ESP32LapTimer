#include "UDP.h"

#include "HardwareConfig.h"
#include "Output.h"

#include <WiFi.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

static int udp_server = -1;

#define UDP_BUF_LEN 1500
static uint8_t packetBuffer[UDP_BUF_LEN];

static struct udp_source_s {
  IPAddress addr;
  uint16_t port;
} udpClients[MAX_UDP_CLIENTS];

void add_ip_port(uint32_t addr, uint16_t port) {
  IPAddress remoteIp = IPAddress(addr);
  // if current ip is already known move it to the front. or if on the last entry delete it to make room
  for(int i = 0; i < MAX_UDP_CLIENTS; ++i) {
    if((udpClients[i].addr == remoteIp && udpClients[i].port == port) || (i+1 == MAX_UDP_CLIENTS)) {
      memmove(udpClients + 1, udpClients, i * sizeof(udpClients[0]));
      break;
    }
  }
  udpClients[0].addr = remoteIp;
  udpClients[0].port = port;
}

void udp_init(void* output) {
  if ((udp_server=socket(AF_INET, SOCK_DGRAM, 0)) == -1){
    return;
  }

  int yes = 1;
  if (setsockopt(udp_server,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) < 0) {
      close(udp_server);
      udp_server = -1;
      return;
  }

  struct sockaddr_in addr;
  memset((char *) &addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(9000);
  addr.sin_addr.s_addr = INADDR_ANY;
  if(bind(udp_server , (struct sockaddr*)&addr, sizeof(addr)) == -1){
    close(udp_server);
    udp_server = -1;
    return;
  }
  fcntl(udp_server, F_SETFL, O_NONBLOCK);  
  Serial.println("Created UDP socket!");
}

void IRAM_ATTR udp_send_packet(void* output, uint8_t* buf, uint32_t size) {
  if(udp_server < 0) return;
  if (buf != NULL && size != 0) {
    for(int i = 0; i < MAX_UDP_CLIENTS; ++i) {
      if(udpClients[i].addr != 0) {
        struct sockaddr_in recipient;
        recipient.sin_addr.s_addr = (uint32_t)udpClients[i].addr;
        recipient.sin_family = AF_INET;
        recipient.sin_port = udpClients[i].port;
        int len = sendto(udp_server, buf, size, 0, (struct sockaddr*) &recipient, sizeof(recipient));
        Serial.print("Sent ");
        Serial.print(len);
        Serial.print(" to ");
        Serial.print(udpClients[i].addr);
        Serial.print(":");
        Serial.print(udpClients[i].port);
        Serial.print(" out of ");
        Serial.print(size);
        Serial.print(" content: ");
        Serial.println((char*)buf);
      }
    }
  }
}

void IRAM_ATTR udp_update(void* output) {
  if(udp_server < 0) return;
  struct sockaddr_in si_other;
  int slen = sizeof(si_other) , len;
  if ((len = recvfrom(udp_server, packetBuffer, UDP_BUF_LEN - 1, MSG_DONTWAIT, (struct sockaddr *) &si_other, (socklen_t *)&slen)) < 1){
    return;
  }
  Serial.println("Got data!");
  add_ip_port(si_other.sin_addr.s_addr, si_other.sin_port);
  packetBuffer[len] = 0;
  output_t* out = (output_t*)output;
  out->handle_input_callback(packetBuffer, len);
}
