/*
 * Worldvisions Tunnel Vision Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 *
 * Manages a connection between the UniConf client and daemon.
 */
#ifndef __UNICONFCONN_H
#define __UNICONFCONN_H

#include "uniconfkey.h"
#include "wvstreamclone.h"
#include "wvbuffer.h"
#include "wvlog.h"

#define DEFAULT_UNICONF_DAEMON_TCP_PORT 4111

/**
 * Represents a connection to a UniConf daemon via any WvStream.
 * Makes several operations much simpler, such as TCL
 * encoding/decoding of lists, filling of the operation buffer and
 * comparison for UniConf operations.
 */
class UniConfConn : public WvStreamClone
{
    WvDynamicBuffer msgbuf;

protected:
    WvLog log;
    bool closed;
    
public:
    WvConstStringBuffer payloadbuf; /*!< holds the previous command payload */
    enum Command
    {
        NONE = -2, /*!< used to signal no command received */
        INVALID = -1, /*!< used to signal invalid command */
        
        // requests
        REQ_NOOP, /*!< noop ==> OK */
        REQ_GET, /*!< get <key> ==> VAL ... OK / FAIL */
        REQ_SET, /*!< set <key> <value> ==> OK / FAIL */
        REQ_REMOVE, /*!< del <key> ==> OK / FAIL */
        REQ_ZAP, /*! zap <key> ==> OK / FAIL */
        REQ_SUBTREE, /*!< subt <key> ==> VAL ... OK / FAIL <*/
        REQ_ADDWATCH, /*!< reg <key> <depth> ==> OK / FAIL */
        REQ_DELWATCH, /*!< ureg <key> <depth> ==> OK / FAIL */
        REQ_QUIT, /*!< quit ==> OK */
        REQ_HELP, /*!< help ==> TEXT ... OK / FAIL */

        // command completion replies
        REPLY_OK, /*!< OK */
        REPLY_FAIL, /*!< FAIL */

        // partial replies
        PART_VALUE, /*!< VAL <key> <value> */
        PART_TEXT, /*!< TEXT <text> */

        // events
        EVENT_HELLO, /*!< HELLO <message> */
        EVENT_CHANGED, /*!< CHG <key> <depth> */
    };
    static const int NUM_COMMANDS = EVENT_CHANGED + 1;
    struct CommandInfo
    {
        const char *name;
        const char *description;
    };
    static const CommandInfo cmdinfos[NUM_COMMANDS];

    /**
     * Create a wrapper around the supplied WvStream.
     */
    UniConfConn(IWvStream *_s);
    virtual ~UniConfConn();

    virtual bool isok() const;
    virtual void close();

    /**
     * Reads a command from the connection.
     * The payload is stored in UniConfConn::payloadbuf.
     * Returns: the command string
     */
    Command readcmd();

    /**
     * Writes a command to the connection.
     * "command" is the command
     * "payload" is the payload
     */
    void writecmd(Command command, WvStringParm payload);

    /**
     * Writes a REPLY_OK message.
     * "payload" is the payload, defaults to ""
     */
    void writeok(WvStringParm payload = "");

    /**
     * Writes a REPLY_FAIL message.
     * "payload" is the payload, defaults to ""
     */
    void writefail(WvStringParm payload = "");

    /**
     * Writes a PART_VALUE message.
     * "key" is the key
     * "value" is the value
     */
    void writevalue(const UniConfKey &key, WvStringParm value);
    
    /**
     * Writes a PART_TEXT message.
     * "text" is the text
     */
    void writetext(WvStringParm text);

private:
    /**
     * Reads a message from the connection.
     */
    WvString readmsg();

    /**
     * Writes a message to the connection.
     */
    void writemsg(WvStringParm message);
};

#endif // __UNICONFCONN_H
