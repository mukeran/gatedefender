/**
 * dns.h - Header for dns.c
 * @author mukeran
 */
#ifndef _MODULE_DNS_H
#define _MODULE_DNS_H

#include <linux/module.h>

#include "stream.h"

u8 check_dns_sld(struct stream *stream, const char *data, int data_length);
u8 check_dns_request_tunneling(struct stream *stream, const char *data, int data_length);

#endif
