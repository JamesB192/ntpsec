/*
 *	digest support for NTP, MD5 and with OpenSSL more
 */
#include <config.h>

#include <string.h>

#include "ntp_fp.h"
#include "ntp_stdlib.h"
#include "ntp.h"
#include "ntp_md5.h"	/* provides OpenSSL digest API */

/*
 * MD5authencrypt - generate message digest
 *
 * Returns length of MAC including key ID and digest.
 */
int
MD5authencrypt(
	int	type,		/* hash algorithm */
	uint8_t	*key,		/* key pointer */
	uint32_t *pkt,		/* packet pointer */
	int	length		/* packet length */
	)
{
	uint8_t	digest[EVP_MAX_MD_SIZE];
	u_int	len;
	EVP_MD_CTX ctx;

	/*
	 * Compute digest of key concatenated with packet. Note: the
	 * key type and digest type have been verified when the key
	 * was created.
	 */
	INIT_SSL();
#if defined(HAVE_OPENSSL)
	if (!EVP_DigestInit(&ctx, EVP_get_digestbynid(type))) {
		msyslog(LOG_ERR,
		    "MAC encrypt: digest init failed");
		return (0);
	}
#else
	EVP_DigestInit(&ctx, EVP_get_digestbynid(type));
#endif
	EVP_DigestUpdate(&ctx, key, cache_secretsize);
	EVP_DigestUpdate(&ctx, (uint8_t *)pkt, (u_int)length);
	EVP_DigestFinal(&ctx, digest, &len);
	memmove((uint8_t *)pkt + length + 4, digest, len);
	return (len + 4);
}


/*
 * MD5authdecrypt - verify MD5 message authenticator
 *
 * Returns one if digest valid, zero if invalid.
 */
int
MD5authdecrypt(
	int	type,		/* hash algorithm */
	uint8_t	*key,		/* key pointer */
	uint32_t	*pkt,		/* packet pointer */
	int	length,	 	/* packet length */
	int	size		/* MAC size */
	)
{
	uint8_t	digest[EVP_MAX_MD_SIZE];
	u_int	len;
	EVP_MD_CTX ctx;

	/*
	 * Compute digest of key concatenated with packet. Note: the
	 * key type and digest type have been verified when the key
	 * was created.
	 */
	INIT_SSL();
#if defined(HAVE_OPENSSL)
	if (!EVP_DigestInit(&ctx, EVP_get_digestbynid(type))) {
		msyslog(LOG_ERR,
		    "MAC decrypt: digest init failed");
		return (0);
	}
#else
	EVP_DigestInit(&ctx, EVP_get_digestbynid(type));
#endif
	EVP_DigestUpdate(&ctx, key, cache_secretsize);
	EVP_DigestUpdate(&ctx, (uint8_t *)pkt, (u_int)length);
	EVP_DigestFinal(&ctx, digest, &len);
	if ((u_int)size != len + 4) {
		msyslog(LOG_ERR,
		    "MAC decrypt: MAC length error");
		return (0);
	}
	return !memcmp(digest, (char *)pkt + length + 4, len);
}

/*
 * Calculate the reference id from the address. If it is an IPv4
 * address, use it as is. If it is an IPv6 address, do a md5 on
 * it and use the bottom 4 bytes.
 * The result is in network byte order.
 */
uint32_t
addr2refid(sockaddr_u *addr)
{
	uint8_t		digest[20];
	uint32_t		addr_refid;
	EVP_MD_CTX	ctx;
	u_int		len;

	if (IS_IPV4(addr))
		return (NSRCADR(addr));

	INIT_SSL();

#if defined(HAVE_OPENSSL)
	EVP_MD_CTX_init(&ctx);
#ifdef EVP_MD_CTX_FLAG_NON_FIPS_ALLOW
	/* MD5 is not used as a crypto hash here. */
	EVP_MD_CTX_set_flags(&ctx, EVP_MD_CTX_FLAG_NON_FIPS_ALLOW);
#endif
	if (!EVP_DigestInit_ex(&ctx, EVP_md5(), NULL)) {
		msyslog(LOG_ERR,
		    "MD5 init failed");
		exit(1);
	}
#else
	EVP_DigestInit(&ctx, EVP_md5());
#endif

	EVP_DigestUpdate(&ctx, (uint8_t *)PSOCK_ADDR6(addr),
	    sizeof(struct in6_addr));
	EVP_DigestFinal(&ctx, digest, &len);
	memcpy(&addr_refid, digest, sizeof(addr_refid));
	return (addr_refid);
}
