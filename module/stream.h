#ifndef _MODULE_STREAM_H
#define _MODULE_STREAM_H

#include <linux/net.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>

#include "identify.h"
#include "stream.h"

enum {
  TCP_STREAM = 1, /* Describe TCP stream */
#define TCP_STREAM TCP_STREAM
  UDP_LOGICAL_STREAM = 2, /* Describe UDP stream logically (while UDP is stateless) */
#define UDP_LOGICAL_STREAM UDP_LOGICAL_STREAM
  RAW_STREAM = 3, /* IPPROTO_RAW without port */
#define RAW_STREAM RAW_STREAM
  ICMP_SEQUENCE = 4, /* Describe ICMP echo/reply sequence */
#define ICMP_SEQUENCE ICMP_SEQUENCE
  DNS_LOGICAL_STREAM = 5, /* Describe DNS logical stream (same sld) */
#define DNS_LOGICAL_STREAM DNS_LOGICAL_STREAM
  DNS_TUNNELING_STREAM = 6, /* Describe DNS backdoor stream */
#define DNS_TUNNELING_STREAM DNS_TUNNELING_STREAM
  ICMP_TUNNELING_STREAM = 7,
#define ICMP_TUNNELING_STREAM ICMP_TUNNELING_STREAM /* Describe special ICMP backdoor/exploit stream */
};

struct stream_data_list {
  struct stream_data_list *next, *prev;
  char *data;
  int length;
  u8 dir;
  u64 timestamp;
};

struct stream_status {
  u32 status;
};

typedef struct stream {
  int id, type;
  int addr1, addr2;
  int port1, port2;
  u8 ended, inbound;
  struct stream_data_list *list;
  union {
    struct {
      u16 sequence;
      u8 code, reserved;
    } icmp;
    struct {
      struct hash_list *hl;
    } dns;
  } meta;
} stream_t;

extern struct stream **streams;
extern int stream_count;

void init_streams(void);
stream_t* identify_stream(u8 dir, struct sk_buff *skb, int type, int saddr, int daddr, int sport, int dport, const char *data, int data_length);
int display_streams(char **_buffer);
int get_stream_data(int id, char **_buffer);

#endif
