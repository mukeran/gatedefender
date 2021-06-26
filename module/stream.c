#include "stream.h"

#include <linux/slab.h>

#include "utils.h"

#define STREAMS_CAPACITY_LEVEL 50
#define BUFFER_CAPACITY_LEVEL 1024

struct stream **streams;
int stream_count;
int stream_capacity;

void init_streams(void) {
  stream_capacity = STREAMS_CAPACITY_LEVEL;
  stream_count = 0;
  streams = (struct stream **)kzalloc(sizeof(struct stream *) * stream_capacity, GFP_KERNEL);
  printk(KERN_INFO "capacity %d, count %d, pointer %p\n", stream_capacity, stream_count, streams);
}

static stream_t* alloc_stream(void) {
  int id;
  if (stream_count >= stream_capacity) {
    stream_capacity += STREAMS_CAPACITY_LEVEL;
    streams = (struct stream **)krealloc(streams, sizeof(struct stream *) * stream_capacity, GFP_KERNEL | __GFP_ZERO);
  }
  id = stream_count;
  streams[id] = (struct stream *)kzalloc(sizeof(struct stream), GFP_ATOMIC);
  streams[id]->id = id;
  ++stream_count;
  return streams[id];
}

static void append_data(struct stream *stream, const char *data, int length, u64 timestamp) {
  struct stream_data_list *entry = (struct stream_data_list *)kzalloc(sizeof(struct stream_data_list), GFP_ATOMIC);
  entry->data = memdup(data, length);
  entry->length = length;
  entry->timestamp = timestamp;
  if (stream->list != NULL) {
    stream->list->prev->next = entry;
    entry->prev = stream->list->prev;
    stream->list->prev = entry;
    entry->next = stream->list;
  } else {
    entry->prev = entry;
    entry->next = entry;
    stream->list = entry;
  }
}

stream_t *identify_stream(u8 dir, struct sk_buff *skb, int type, int saddr, int daddr, int sport, int dport, const char *data, int data_length) {
  int i;
  struct tcphdr *tcp_header;
  struct stream *stream = NULL;
  u64 timestamp = ktime_to_ns(skb->tstamp);
  if (dir == OUT && (type == IPPROTO_TCP || type == IPPROTO_RAW)) {
    swap(saddr, daddr);
    swap(sport, dport);
  }
  /* Update stream */
  for (i = 0; i < stream_count; ++i) {
    if (!streams[i]->ended && streams[i]->addr1 == saddr && streams[i]->addr2 == daddr) {
      if (type == IPPROTO_TCP && streams[i]->type == TCP_STREAM && streams[i]->port1 == sport && streams[i]->port2 == dport) {
        tcp_header = tcp_hdr(skb);
        append_data(streams[i], data, data_length, timestamp);
        if (tcp_header->fin && dir == OUT)
          streams[i]->ended = 1;
        return streams[i];
      } else if (type == IPPROTO_UDP && streams[i]->type == UDP_LOGICAL_STREAM && streams[i]->port1 == sport && streams[i]->port2 == dport) {
        /* Not implemented */
      } else if (type == IPPROTO_UDP && streams[i]->type == DNS_BACKDOOR_STREAM && streams[i]->port1 == sport && streams[i]->port2 == dport) {
        /* Not implemented */
      } else if (type == IPPROTO_ICMP && streams[i]->type == ICMP_SEQUENCE) {
        /* Not implemented */
      } else if (type == IPPROTO_ICMP && streams[i]->type == ICMP_BACKDOOR_STREAM) {
        /* Not implemented */
      }
    }
  }
  /* Create stream */
  if (type == IPPROTO_TCP) {
    tcp_header = tcp_hdr(skb);
    if (!tcp_header->fin && !tcp_header->rst) {
      stream = alloc_stream();
      stream->addr1 = saddr;
      stream->addr2 = daddr;
      stream->port1 = sport;
      stream->port2 = dport;
      stream->inbound = dir;
      stream->type = TCP_STREAM;
      append_data(stream, data, data_length, timestamp);
    }
  }
  return stream;
}

static inline const char *get_stream_string(int type) {
  if (type == TCP_STREAM)
    return "TCP_STREAM";
  else if (type == UDP_LOGICAL_STREAM)
    return "UDP_LOGICAL_STREAM";
  else if (type == ICMP_SEQUENCE)
    return "ICMP_SEQUENCE";
  else if (type == DNS_BACKDOOR_STREAM)
    return "DNS_BACKDOOR_STREAM";
  else if (type == ICMP_BACKDOOR_STREAM)
    return "ICMP_BACKDOOR_STREAM";
  return "UNKNOWN_STREAM";
}

int display_streams(char **_buffer) {
  int i, capacity = BUFFER_CAPACITY_LEVEL, length = 0, append_length;
  char *buffer;
  char *temp_buffer = (char *)kzalloc(sizeof(char) * BUFFER_CAPACITY_LEVEL, GFP_KERNEL);
  *_buffer = (char *)kzalloc(sizeof(char) * capacity, GFP_KERNEL);
  buffer = *_buffer;
  length = sprintf(buffer,
    "|Stream ID|Stream Type|End1|End2|First Seen|Last Seen|Is Ended|\n"
    "|---|---|---|---|---|---|---|\n");
  for (i = 0; i < stream_count; ++i) {
    append_length = sprintf(temp_buffer,
    "|%d|%s|%pI4:%hu|%pI4:%hu|%lld|%lld|%hu|\n",
    streams[i]->id, get_stream_string(streams[i]->type), &streams[i]->addr1, ntohs(streams[i]->port1), &streams[i]->addr2, ntohs(streams[i]->port2), streams[i]->list->timestamp, streams[i]->list->prev->timestamp, streams[i]->ended);
    if (length + append_length >= capacity) {
      capacity += BUFFER_CAPACITY_LEVEL;
      buffer = (char *)krealloc(buffer, sizeof(char) * capacity, GFP_KERNEL | __GFP_ZERO);
    }
    memcpy(buffer + length, temp_buffer, append_length);
    length += append_length;
    buffer[length] = '\0';
  }
  return length;
}
