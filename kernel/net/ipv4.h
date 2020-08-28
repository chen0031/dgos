#pragma once
#include "ethernet.h"
#include "utility.h"
#include "ipv4.bits.h"

struct ipv4_hdr_t {
    ethernet_hdr_t eth_hdr;
    uint8_t ver_ihl;
    uint8_t dscp_ecn;
    uint16_t len;
    uint16_t id;
    uint16_t flags_fragofs;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t hdr_checksum;
    uint8_t s_ip[4];
    uint8_t d_ip[4];

    size_t version() const
    {
        return IPv4_VERLEN_VER_GET(ver_ihl);
    }

    size_t header_sz() const
    {
        return IPv4_VERLEN_LEN_GET(ver_ihl) * sizeof(uint32_t);
    }
} _packed;

#define IPV4_PROTO_ICMP     0x01
#define IPV4_PROTO_TCP      0x06
#define IPV4_PROTO_UDP      0x11

#define IPV4_ADDR32(a,b,c,d) (((a)<<24)|((b)<<16)|((c)<<8)|(d))

struct ipv4_addr_t {
    uint32_t ip;
    uint16_t port;
    uint16_t align;
};

struct ipv4_addr_pair_t {
    ipv4_addr_t s;
    ipv4_addr_t d;
};

template<>
struct ext::hash<ipv4_addr_t>
{
    size_t operator()(ipv4_addr_t const& __k) const
    {
        return (hash<uint32_t>()(__k.ip) << 16) ^
                hash<uint16_t>()(__k.port);
    }
};

template<>
struct ext::hash<ipv4_addr_pair_t>
{
    size_t operator()(ipv4_addr_pair_t const& __k) const
    {
        return (hash<ipv4_addr_t>()(__k.s) << 16) ^
                hash<ipv4_addr_t>()(__k.d);
    }
};

uint16_t ipv4_checksum(ipv4_hdr_t const *hdr);
void ipv4_ip_get(ipv4_addr_pair_t *addr, ipv4_hdr_t const *hdr);

void const *ipv4_end_get(ipv4_hdr_t const *hdr);

void ipv4_finalize(ipv4_hdr_t *hdr, void const *end);
