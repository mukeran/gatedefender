#include "stream.h"

#include <linux/slab.h>

#include "dns.h"
#include "proc.h"
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

static void append_data(struct stream *stream, const char *data, int length, u8 dir, u64 timestamp) {
  struct stream_data_list *entry = (struct stream_data_list *)kzalloc(sizeof(struct stream_data_list), GFP_ATOMIC);
  entry->data = memdup(data, length);
  entry->length = length;
  entry->timestamp = timestamp;
  entry->dir = dir;
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
  struct icmphdr *icmp_header;
  struct stream *stream = NULL;
  u64 timestamp = ktime_to_ms(skb->tstamp);
  if (dir == OUT && (type == IPPROTO_TCP || type == IPPROTO_UDP || type == IPPROTO_ICMP || type == IPPROTO_RAW)) {
    swap(saddr, daddr);
    swap(sport, dport);
  }
  /* Update stream */
  for (i = 0; i < stream_count; ++i) {
    if (!streams[i]->ended && streams[i]->addr1 == saddr && streams[i]->addr2 == daddr) {
      if (type == IPPROTO_TCP && streams[i]->type == TCP_STREAM && streams[i]->port1 == sport && streams[i]->port2 == dport) {
        tcp_header = tcp_hdr(skb);
        append_data(streams[i], data, data_length, dir, timestamp);
        if (tcp_header->fin && dir == OUT)
          streams[i]->ended = 1;
        return streams[i];
      } else if (type == IPPROTO_UDP && streams[i]->type == UDP_LOGICAL_STREAM && streams[i]->port1 == sport) {
        append_data(streams[i], data, data_length, dir, timestamp);
        return streams[i];
      } else if (type == IPPROTO_UDP && (streams[i]->type == DNS_LOGICAL_STREAM || streams[i]->type == DNS_TUNNELING_STREAM) && ntohs(sport) == 53 && check_dns_sld(streams[i], data, data_length)) {
        if (streams[i]->type == DNS_LOGICAL_STREAM && dir == OUT && check_dns_request_backdoor(streams[i], data, data_length))
          streams[i]->type = DNS_TUNNELING_STREAM;
        append_data(streams[i], data, data_length, dir, timestamp);
        return streams[i];
      } else if (type == IPPROTO_ICMP && streams[i]->type == ICMP_SEQUENCE) {
        icmp_header = icmp_hdr(skb);
        if (icmp_header->type == ICMP_ECHO || icmp_header->type == ICMP_ECHOREPLY) {
          if (streams[i]->meta.icmp.code != icmp_header->code || streams[i]->list->length != data_length || (data_length >= 32 && memcmp(streams[i]->list->data + 8, data + 8, data_length - 8) != 0) || (data_length < 32 && memcmp(streams[i]->list->data, data, data_length) != 0)) {
              /* Change type to icmp backdoor stream */
              streams[i]->type = ICMP_TUNNELING_STREAM;
              append_data(streams[i], data, data_length, dir, timestamp);
              return streams[i];
          } else if ((icmp_header->type == ICMP_ECHO && streams[i]->meta.icmp.sequence + 1 == ntohs(icmp_header->un.echo.sequence)) || (icmp_header->type == ICMP_ECHOREPLY && streams[i]->meta.icmp.sequence == ntohs(icmp_header->un.echo.sequence))) {
            append_data(streams[i], data, data_length, dir, timestamp);
            streams[i]->meta.icmp.sequence = (icmp_header->type == ICMP_ECHO) ? (streams[i]->meta.icmp.sequence + 1) : streams[i]->meta.icmp.sequence;
            return streams[i];
          }
        }
      } else if (type == IPPROTO_ICMP && streams[i]->type == ICMP_TUNNELING_STREAM) {
        append_data(streams[i], data, data_length, dir, timestamp);
        return streams[i];
      }
    }
  }
  /* Create stream */
  if (type == IPPROTO_TCP) {
    tcp_header = tcp_hdr(skb);
    if (!tcp_header->fin && !tcp_header->rst) {
      stream = alloc_stream();
      stream->addr1 = saddr; stream->addr2 = daddr;
      stream->port1 = sport; stream->port2 = dport;
      stream->inbound = dir;
      stream->type = TCP_STREAM;
      append_data(stream, data, data_length, dir, timestamp);
    }
  } else if (type == IPPROTO_UDP) {
    if (dir == OUT) {
      stream = alloc_stream();
      stream->addr1 = saddr; stream->addr2 = daddr;
      stream->port1 = sport; stream->port2 = dport;
      stream->inbound = dir;
      if (ntohs(sport) == 53) {
        stream->type = DNS_LOGICAL_STREAM;
        stream->meta.dns.hl = alloc_hash_list();
        if (dir == OUT)
          check_dns_request_backdoor(streams[i], data, data_length);
      } else
        stream->type = UDP_LOGICAL_STREAM;
      append_data(stream, data, data_length, dir, timestamp);
    }
  } else if (type == IPPROTO_ICMP) {
    icmp_header = icmp_hdr(skb);
    if (icmp_header->code == ICMP_ECHO || icmp_header->code == ICMP_ECHOREPLY) {
      stream = alloc_stream();
      stream->addr1 = saddr; stream->addr2 = daddr;
      stream->inbound = dir;
      stream->type = ICMP_SEQUENCE;
      stream->meta.icmp.sequence = ntohs(icmp_header->un.echo.sequence);
      append_data(stream, data, data_length, dir, timestamp);
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
  else if (type == DNS_LOGICAL_STREAM)
    return "DNS_LOGICAL_STREAM";
  else if (type == DNS_TUNNELING_STREAM)
    return "DNS_TUNNELING_STREAM";
  else if (type == ICMP_TUNNELING_STREAM)
    return "ICMP_TUNNELING_STREAM";
  return "UNKNOWN_STREAM";
}

int display_streams(char **_buffer) {
  int i, capacity = BUFFER_CAPACITY_LEVEL, length = 0, append_length;
  char *buffer, *temp_buffer = (char *)kzalloc(sizeof(char) * BUFFER_CAPACITY_LEVEL, GFP_KERNEL);
  buffer = (char *)kzalloc(sizeof(char) * capacity, GFP_KERNEL);
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
  *_buffer = buffer;
  kfree(temp_buffer);
  return length;
}

static inline const char* get_direction_string(u8 dir) {
  if (dir == IN)
    return "in";
  return "out";
}

int get_stream_data(int id, char **_buffer) {
  int capacity = BUFFER_CAPACITY_LEVEL, length = 0, append_length, total_length;
  char *buffer, *temp_buffer = (char *)kzalloc(sizeof(char) * BUFFER_CAPACITY_LEVEL, GFP_KERNEL);
  struct stream *stream = streams[id];
  struct stream_data_list *cur = stream->list;
  if (cur == NULL) {
    kfree(temp_buffer);
    return 0;
  }
  buffer = (char *)kzalloc(sizeof(char) * capacity, GFP_KERNEL);
  do {
    if (cur->length == 0) {
      cur = cur->next;
      continue;
    }
    append_length = sprintf(temp_buffer, "direction %s, length %d, time %lld\n-----data begin-----\n", get_direction_string(cur->dir), cur->length, cur->timestamp);
    total_length = length + append_length + cur->length + 21 /* Length of "\n-----data end-----\n" */;
    if (total_length >= capacity) {
      capacity = ((total_length - 1) / BUFFER_CAPACITY_LEVEL + 1) * BUFFER_CAPACITY_LEVEL;
      buffer = (char *)krealloc(buffer, sizeof(char) * capacity, GFP_KERNEL | __GFP_ZERO);
    }
    memcpy(buffer + length, temp_buffer, append_length);
    memcpy(buffer + length + append_length, cur->data, cur->length);
    memcpy(buffer + length + append_length + cur->length, "\n-----data end-----\n", 21);
    length = total_length;
    cur = cur->next;
  } while (cur != stream->list);
  kfree(temp_buffer);
  *_buffer = buffer;
  return length;
}
