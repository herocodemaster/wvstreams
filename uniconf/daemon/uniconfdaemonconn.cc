
#include "uniconfdaemonconn.h"
#include "uniconfdaemon.h"
#include "uniconfiter.h"
#include "wvtclstring.h"


UniConfDaemonConn::UniConfDaemonConn(WvStream *_s, UniConfDaemon *_source)
    : UniConfConn(_s), 
      log(WvString("UniConf to %s", *_s->src()), WvLog::Debug)
{
    print("HELLO UniConf Server ready\n");
    source = _source;
}


UniConfDaemonConn::~UniConfDaemonConn()
{
    WvStringList::Iter i(keys);
    for (i.rewind(); i.next();)
    {
        WvString key = *i;
        if (!key) key = "/";
        del_callback(key);
    }
}

void UniConfDaemonConn::keychanged(void *userdata, UniConf &conf)
{
    // All the following is irrelevant if we have a null pointer, so check
    // it first.
    if (!userdata)
        return;
    
    WvStream *s = (WvStream *)userdata;
    WvString keyname(conf.gen_full_key()); 
//    log(WvLog::Debug2, "Got a callback for key %s.\n", keyname);
    if (s->isok())
        s->write(WvString("FGET %s\n", keyname));
}

/* Functions to look after UniEvents callback setting / removing */

void UniConfDaemonConn::update_callbacks(WvString key)
{
    del_callback(key);
    add_callback(key);
}

void UniConfDaemonConn::del_callback(WvString key)
{
//    log("About to delete callbacks for %s.\n", key);
    source->events.del(wvcallback(UniConfCallback, *this,
                UniConfDaemonConn::keychanged), this, key);

}

void UniConfDaemonConn::add_callback(WvString key)
{
//    log("About to add callbacks for %s.\n", key);
    source->events.add(wvcallback(UniConfCallback,
                *this, UniConfDaemonConn::keychanged), this, key);

    // Now track what keys I know of.
//    log("Now adding key:%s, to the list of keys.\n",key);
    keys.append(new WvString(key), true);
}

/* End of functions to look after UniEvents callback setting / removing */

void UniConfDaemonConn::dook(const WvString cmd, const WvString key)
{
    if (this->isok())
        print("OK %s %s\n", cmd, key); 
}

void UniConfDaemonConn::doget(WvString key)
{
    dook("get", key);
    WvString response;
    if (!!source->mainconf.get(key))
        response = WvString("RETN %s %s\n", wvtcl_escape(key),
                wvtcl_escape(source->mainconf.get(key)));
    else
        response = WvString("RETN %s \\0\n", wvtcl_escape(key));

    if (this->isok())
    {
        print(response);

        // Ensure no duplication of events.
        update_callbacks(key);
    }
}

void UniConfDaemonConn::dosubtree(WvString key)
{
    UniConf *nerf = &source->mainconf[key];
    WvString send("SUBT %s ", wvtcl_escape(key));
    
    dook("subt", key);
    
    if (nerf)
    {
        UniConf::Iter i(*nerf);
        for (i.rewind(); i.next();)
        {
            send.append("{%s %s} ", wvtcl_escape(i->name),
                    wvtcl_escape(*i));
            
            update_callbacks(key);
       }
    }
   
    send.append("\n");
    if (this->isok())
        print(send);
}

void UniConfDaemonConn::dorecursivesubtree(WvString key)
{
    UniConf *nerf = &source->mainconf[key];
    WvString send("SUBT %s ", wvtcl_escape(key));
    
    dook("rsub", key);
    
    if (nerf)
    {
        UniConf::RecursiveIter i(*nerf);
        for (i.rewind(); i.next();)
        {
            send.append("{%s %s} ", wvtcl_escape(i->full_key(nerf)),
                    wvtcl_escape(*i));
            
            update_callbacks(key);
        }
    }
    send.append("\n");
    if (this->isok())
        print(send);
}

void UniConfDaemonConn::doset(WvString key, WvConstStringBuffer &fromline)
{
    WvString newvalue = wvtcl_getword(fromline);
    source->mainconf[key] = wvtcl_unescape(newvalue);
    source->keymodified = true;
    dook("set", key);
}

void UniConfDaemonConn::execute()
{
    WvString line, cmd;
    
    UniConfConn::execute();
    fillbuffer();

    while (!(line = gettclline()).isnull())
    {
        WvConstStringBuffer fromline(line);

//	log(WvLog::Debug5, "Got command: '%s'\n", line);

        while (!(cmd = wvtcl_getword(fromline)).isnull())
        {
            // check the command
	    if (cmd == "help")
	    {
                if (this->isok())
	    	    print("OK I know how to: help, get, subt, quit\n");
		return;	    
	    }
            if (cmd == "quit")
            {
                dook(cmd, "<null>");
                close();
                return;
            }
            WvString key = wvtcl_getword(fromline);
            if (key.isnull())
                break;

            if (cmd == "get") // return the specified value
            {
                doget(key);
            }
            else if (cmd == "subt") // return the subtree(s) of this key
            {
                dosubtree(key);
            }
            else if (cmd == "rsub")
            {
                dorecursivesubtree(key);
            }
            else if (cmd == "set") // set the specified value
            {
                doset(key, fromline);
            }
        }
    }
}
