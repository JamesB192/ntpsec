/*
 * nts.h - NTS (Network Time Security) declarations
 */
#ifndef GUARD_NTS_H
#define GUARD_NTS_H

#include <openssl/ssl.h>

#define NTS_MAX_COOKIES	8	/* RFC 4.1.6 */
#define NTS_COOKIELEN	128	/* placeholder - see RFC 6 */

#define FLAG_NTS	0x01u	/* use NTS (network time security) */
#define FLAG_NTS_ASK	0x02u	/* NTS, ask for specified server */
#define FLAG_NTS_REQ	0x04u	/* NTS, ask for specified server */
#define FLAG_NTS_NOVAL	0x08u	/* do not validate the server certificate */

/* Configuration data for an NTS association */
struct ntscfg_t {
    char *server;	/* if NULL, use the peer itself (normal case) */
    char *ca;		/* if NULL, use the site default (normal case) */
    char *cert;		/* if NULL, use the site default (normal case) */
    uint32_t flags;
    uint32_t expire;
};

// FIXME AEAD_AES_SIV_CMAC_256
// We are using AEAD_AES_SIV_CMAC_256, from RFC 5297
// There is no clean API yet
#define IANA_AEAD_AES_SIV_CMAC_256 15
#define AEAD_AES_SIV_CMAC_256_KEYLEN 32

#define NTS_MAX_KEYLEN 64
/* Client-side state per connection to server */
struct ntsstate_t {
    int aead;
    int keylen;
    int next_cookie;
    int cookie_count;
    int cookie_length;
    bool valid[NTS_MAX_COOKIES];
    uint8_t cookies[NTS_MAX_COOKIES][NTS_COOKIELEN];
    uint8_t c2s[NTS_MAX_KEYLEN], s2c[NTS_MAX_KEYLEN];
};

/* Configuration data for an NTS server or client instance */
struct ntsconfig_t {
    bool ntsenable; 		/* enable NTS KE server on this ntpd */
    const char * mintls;	/* minimum TLS version allowed */
    const char * maxtls;	/* maximum TLS version allowed */
    const char *tlsciphers;	/* allowed TLS 1.2 ciphers */
    const char *tlsciphersuites;/* allowed TLS 1.3 ciphersuites */
    const char *cert;		/* server certificate key */
    const char *key;		/* server private key */
    const char *ca;		/* root cert dir/file */
};

/* NTS protocol constants */

#define NTS_CRITICAL 0x8000
enum nts_record_type {
  nts_end_of_message = 0,		/* CRITICAL */
  nts_next_protocol_negotiation = 1,	/* CRITICAL */
  nts_error = 2,			/* CRITICAL */
  nts_warning = 3,
  nts_algorithm_negotiation = 4,
  nts_new_cookie = 5,
  nts_server_negotiation = 6,
  nts_port_negotiation = 7
};

enum nts_errors_type {
  nts_unrecognized_critical_section = 0,
  nts_bad_request = 1
};

enum aead_ciphers {
  AEAD_AES_128_GCM = 1,
  AEAD_AES_256_GCM = 2,
  AEAD_AES_128_CCM = 3,
  AEAD_AES_256_CCM = 4,

  AEAD_AES_128_GCM_8 = 5,
  AEAD_AES_256_GCM_8 = 6,
  AEAD_AES_128_GCM_12 = 7,
  AEAD_AES_256_GCM_12 = 8,

  AEAD_AES_128_CCM_SHORT = 9,
  AEAD_AES_256_CCM_SHORT = 10,
  AEAD_AES_128_CCM_SHORT_8 = 11,
  AEAD_AES_256_CCM_SHORT_8 = 12,
  AEAD_AES_128_CCM_SHORT_12 = 13,
  AEAD_AES_256_CCM_SHORT_12 = 14,

  AEAD_AES_SIV_CMAC_256 = 15,
  AEAD_AES_SIV_CMAC_384 = 16,
  AEAD_AES_SIV_CMAC_512 = 17,

  AEAD_AES_128_CCM_8 = 18,
  AEAD_AES_256_CCM_8 = 19,

  AEAD_AES_128_OCB_TAGLEN128 = 20,
  AEAD_AES_128_OCB_TAGLEN96 = 21,
  AEAD_AES_128_OCB_TAGLEN64 = 22,
  AEAD_AES_192_OCB_TAGLEN128 = 23,
  AEAD_AES_192_OCB_TAGLEN96 = 24,
  AEAD_AES_192_OCB_TAGLEN64 = 25,
  AEAD_AES_256_OCB_TAGLEN128 = 26,
  AEAD_AES_256_OCB_TAGLEN96 = 27,
  AEAD_AES_256_OCB_TAGLEN64 = 28,

  AEAD_CHACHA20_POLY1305 = 29
};



extern struct ntsconfig_t ntsconfig;

bool nts_server_init(void);
bool nts_client_init(void);

int nts_get_key_length(int aead);
bool nts_load_ciphers(SSL_CTX *ctx);
bool nts_load_versions(SSL_CTX *ctx);
bool nts_make_keys(SSL *ssl, uint8_t *c2s, uint8_t *s2c, int keylen);
int nts_make_cookie(uint8_t *cookie, uint16_t aead,
  uint8_t *c2s, uint8_t *s2c, int keylen);

#define NO_OLD_VERSIONS SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3|SSL_OP_NO_TLSv1|SSL_OP_NO_TLSv1_1


/* buffer packing/unpacking routines.
 * These aren't NTS specific, but I'll put them here for now.
 * They work with NTP extensions and NTS-KE data streams.
 *   names should probably be changed too
 * NTP extensions are padded to word (4 byte) boundaries.
 * NTS-KE has no padding.
 * Maybe versions with padding should have names starting with ntp_?
 * and use extn rather than record
 * Note that data on the wire is big endian.
 * buffer is wire format, not host format.
 * Hal, 2019-Feb-09
 */


/* working finger into a buffer - updated by append/unpack routines */
struct BufCtl_t {
  uint8_t *next;  /* pointer to next data/space */
  int left;       /* data left or  space available */
};
typedef struct BufCtl_t BufCtl;

/* maybe should return bool to indicate overflow */
/* nts_append_record_foo makes whole record with one foo */
/* ntp_append_foo appends foo to existing partial record */
void nts_append_record_null(BufCtl* buf, uint16_t type);
void nts_append_record_uint16(BufCtl* buf, uint16_t type, uint16_t data);
void nts_append_record_bytes(BufCtl* buf, uint16_t type, uint8_t *data, int length);

void nts_append_header(BufCtl* buf, uint16_t type, uint16_t length);
void nts_append_uint16(BufCtl* buf, uint16_t data);
void nts_append_bytes(BufCtl* buf, uint8_t *data, int length);

uint16_t nts_next_record(BufCtl* buf, int *length);
uint16_t nts_next_uint16(BufCtl* buf);
uint16_t nts_next_bytes(BufCtl* buf, uint8_t *data, int length);


#endif /* GUARD_NTS_H */
