/*
 * Worldvisions Tunnel Vision Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 * 
 * MD5, SHA-1 and HMAC digest abstractions.
 *
 * Message digest encoders have the following semantics:
 *   Input data to the encoder is collected and a digest function
 *   is applied to it internally.  When finish() is called, the
 *   current value of the digest is written to the output.
 *   No output is ever generated by encode(), or flush().
 *   The digest state is discarded on reset() allowing a new
 *   stream of data to be processed.
 */
#ifndef __WVDIGEST_H
#define __WVDIGEST_H

#include "wvencoder.h"

/**
 * Superclass for all digests.
 */
class WvDigest : public WvEncoder
{
public:
    /**
     * Returns the number of bytes in the message digest.
     */
    virtual size_t digestsize() const = 0;
};

// internal implementation class do not reference this directly
struct env_md_st;
struct env_md_ctx_st;
class WvEVPMDDigest : public WvDigest
{
    friend class WvHMACDigest;
    env_md_st *evpmd;
    env_md_ctx_st *evpctx;
    bool active;

public:
    virtual ~WvEVPMDDigest();
    virtual size_t digestsize() const;

protected:
    WvEVPMDDigest(env_md_st *_evpmd);
    virtual bool _encode(WvBuffer &inbuf, WvBuffer &outbuf,
        bool flush); // consumes input
    virtual bool _finish(WvBuffer &outbuf); // outputs digest
    virtual bool _reset(); // supported: resets digest value
    
    env_md_st *getevpmd()
        { return evpmd; }

private:
    void cleanup();
};


/**
 * MD5 Digest
 * Has a digest length of 128 bits.
 */
class WvMD5Digest : public WvEVPMDDigest
{
public:
    /**
     * Creates an MD5 digest encoder.
     */
    WvMD5Digest();
    virtual ~WvMD5Digest() { }
};


/**
 * SHA-1 Digest
 * Has a digest length of 160 bits.
 */
class WvSHA1Digest : public WvEVPMDDigest
{
public:
    /**
     * Creates an MD5 digest encoder.
     */
    WvSHA1Digest();
    virtual ~WvSHA1Digest() { }
};


/**
 * HMAC Message Authentication Code
 * Has a digest length that varies according to the underlying
 * digest encoder.
 */
struct hmac_ctx_st;
class WvHMACDigest : public WvDigest
{
    WvEVPMDDigest *digest;
    unsigned char *key;
    size_t keysize;
    hmac_ctx_st *hmacctx;
    bool active;

public:
    /**
     * Creates an HMAC digest encoder.
     *   digest - specifies the digest algorithm to use as a
     *            hash function
     *   key     - the authentication key
     *   keysize - the key size in bytes
     */
    WvHMACDigest(WvEVPMDDigest *_digest, const void *_key,
        size_t _keysize);
    virtual ~WvHMACDigest();
    virtual size_t digestsize() const;

protected:
    virtual bool _encode(WvBuffer &inbuf, WvBuffer &outbuf,
        bool flush); // consumes input
    virtual bool _finish(WvBuffer &outbuf); // outputs digest
    virtual bool _reset(); // supported: resets digest value

private:
    void cleanup();
};

#endif // __WVDIGEST_H
