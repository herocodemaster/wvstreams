#include "wvtest.h"
#include "uniconfroot.h"
#include "unitempgen.h"
#include "uniretrygen.h"
#include "uniconfdaemon.h"
#include "uniconfgen-sanitytest.h"
#include "uniclientgen.h"
#include "uniinigen.h"
#include "wvunixsocket.h"
#include "wvfileutils.h"
#include "wvfile.h"
#include "uniwatch.h"
#include "wvstrutils.h"

#include <signal.h>

class WvDebugUnixConn : public WvUnixConn
{
    WvLog log;
    WvString socket;
public:
    WvDebugUnixConn(WvString _name, WvStringParm _socket) :
        WvUnixConn(_socket),
        log(_name, WvLog::Debug),
        socket(_socket)
    {
        log("Created UNIX connection on %s\n", socket);
    }

    ~WvDebugUnixConn()
    {
        log("Destroyed UNIX connection on %s\n", socket);
    }

    size_t uread(void *buf, size_t count)
    {
        size_t result = WvUnixConn::uread(buf, count);
        if (result > 0)
            log("<- %s\n", cstr_escape(buf, result));
        return result;
    }

    size_t uwrite(const void *buf, size_t count)
    {
        size_t result = WvUnixConn::uwrite(buf, count);
        if (result > 0)
            log("-> %s\n", cstr_escape(buf, result));
        return result;
    }
};


static int delta_count;
static void delta_callback(const UniConf &, const UniConfKey &)
{
    ++delta_count;
}

typedef WvCallback<UniClientGen*, WvStringParm, WvStringParm, 
        WvUnixConn *&> client_factory_t;

// Returns a new UniClientGen.  unix_conn is set to a pointer to the
// underlying unix stream (icky, but required).
static UniClientGen *debug_client_factory(WvStringParm name, 
        WvStringParm sockname, WvUnixConn *&unix_conn)
{
    return new UniClientGen(
                unix_conn = new WvDebugUnixConn(name, sockname));
}


// Returns a new UniClientGen, specifying a restrict key.  unix_conn is set to
// a pointer to the underlying unix stream (icky, but required).
static UniClientGen *restrict_client_factory(WvStringParm name, 
        WvStringParm sockname, WvUnixConn *&unix_conn)
{
    return new UniClientGen(
                unix_conn = new WvDebugUnixConn(name, sockname), 
                sockname, name);
}


// Returns a new UniClientGen, using a non-logging underlying stream.
// unix_conn is set to a pointer to the underlying unix stream.
static UniClientGen *quiet_client_factory(WvStringParm name, 
        WvStringParm sockname, WvUnixConn *&unix_conn)
{
    return new UniClientGen(unix_conn = new WvUnixConn(sockname));
}


// Set the default client_factory to debug_client_factory if you want to see
// what goes through the connection.
static UniClientGen *create_client_conn(WvString name, WvString sockname,
       client_factory_t client_factory = quiet_client_factory) 
{
    UniClientGen *client_gen = NULL;
    while (true)
    {
        WvUnixConn *unix_conn;
        client_gen = client_factory(name, sockname, unix_conn);
        if (!unix_conn || !unix_conn->isok()
                || !client_gen || !client_gen->isok())
        {
            WVRELEASE(client_gen);
            wvout->print("Failed to connect, retrying...\n");
            sleep(1);
        }
        else break;
    }

    return client_gen;
}


WVTEST_MAIN("UniClientGen Sanity Test")
{
    WvString sockname = wvtmpfilename("uniclientgen.t-sock");
    UniConfTestDaemon daemon(sockname, "temp:");

    UniClientGen *gen = create_client_conn("sanity", sockname);

    UniConfGenSanityTester::sanity_test(gen, WvString("unix:%s", sockname));
    WVRELEASE(gen);
}


WVTEST_MAIN("deltas")
{
    UniConfRoot uniconf;

    signal(SIGPIPE, SIG_IGN);

    WvString sockname = wvtmpfilename("uniclientgen.t-sock");
    unlink(sockname);

    UniConfTestDaemon daemon(sockname, "temp:", 
            UniConfTestDaemon::autoinc_server_cb);
    UniClientGen *client_gen = create_client_conn("deltas", sockname);
    uniconf.mountgen(client_gen); 

    int old_delta_count = delta_count = 0;
    uniconf.add_callback(client_gen, "", delta_callback, false);

    int i;
    for (i=0; i<100; ++i)
    {
        client_gen->refresh();
        usleep(1000);
    }

    WVPASS(delta_count > old_delta_count);
}


static time_t file_time(WvStringParm filename)
{
    struct stat st;
    if (stat(filename, &st) == 0)
        return st.st_mtime;
    else
    {
        wverr->print("stat(%s) failed: %s\n", filename,
                strerror(errno));
        return -1;
    }
}


WVTEST_MAIN("commit")
{
    UniConfRoot uniconf;
    WvString ini_file("/tmp/wvtest-uniclientgen-commit-%s.ini", getpid());
    unlink(ini_file);
    
    signal(SIGPIPE, SIG_IGN);

    WvString sockname = wvtmpfilename("uniclientgen.t-sock");

    UniConfTestDaemon daemon(sockname, WvString("ini:%s", ini_file));

    UniClientGen *client_gen = create_client_conn("commit", sockname);
    uniconf.mountgen(client_gen); 

    uniconf.setmeint(time(NULL));
    uniconf.commit();
    time_t old_file_time = file_time(ini_file);
    WVPASS(old_file_time != -1);
    WVPASS(old_file_time <= time(NULL));

    // Wait until ini file is old
    while (time(NULL) <= old_file_time)
        sleep(1);

    uniconf.setmeint(time(NULL));
    uniconf.commit();

    time_t new_file_time = file_time(ini_file);
    WVPASS(new_file_time != -1);
    WVPASS(new_file_time > old_file_time);

    unlink(ini_file);
}


WVTEST_MAIN("refresh")
{
    UniConfRoot uniconf;
    WvString ini_file("/tmp/wvtest-uniclientgen-refresh-%s.ini", getpid());
    unlink(ini_file);
    
    signal(SIGPIPE, SIG_IGN);

    WvString sockname = wvtmpfilename("uniclientgen.t-sock");

    UniConfTestDaemon daemon(sockname, WvString("ini:%s", ini_file));

    UniClientGen *client_gen = create_client_conn("refresh", sockname);
    uniconf.mountgen(client_gen); 

    WVPASS(uniconf.xget("key") != "value");
    WvFile ini_fd(ini_file, O_WRONLY | O_APPEND | O_CREAT);
    ini_fd.print("[]\nkey = value\n");
    ini_fd.close();
    WVPASS(uniconf.refresh());
    WVPASS(uniconf.xget("key") == "value");

    unlink(ini_file);
}

int save_callback_calls;
static void save_callback()
{
    ++save_callback_calls;
}

WVTEST_MAIN("SaveCallback")
{
    UniConfRoot uniconf;

    WvString ini_file("/tmp/wvtest-uniclientgen-refresh-%s.ini", getpid());
    unlink(ini_file);

    UniIniGen *ini_gen = new UniIniGen(ini_file, 0600, save_callback);
    uniconf.mountgen(ini_gen);
    
    int items = 0;
    uniconf["Items"].xsetint(items, items++);
    uniconf["Items"].xsetint(items, items++);
    uniconf["Items"].xsetint(items, items++);
    uniconf["Items"].xsetint(items, items++);
    uniconf["Items"].xsetint(items, items++);

    save_callback_calls = 0;
    uniconf.commit();
    WVPASS(save_callback_calls >= items);
    
    unlink(ini_file);
}
        
int restrict_callback_count;
static void restrict_callback(const UniConf &uniconf, const UniConfKey &key)
{
    wverr->print("restrict_callback: %s = %s (count now %s)\n",
            uniconf[key].fullkey(), uniconf[key].getme(),
            ++restrict_callback_count);
}

int sub_restrict_callback_count;
static void sub_restrict_callback(const UniConf &uniconf, const UniConfKey &key)
{
    wverr->print("sub_restrict_callback: %s = %s (count now %s)\n",
            uniconf[key].fullkey(), uniconf[key].getme(),
            ++sub_restrict_callback_count);
}

WVTEST_MAIN("restrict")
{
    signal(SIGPIPE, SIG_IGN);

    WvString sockname = wvtmpfilename("unisubtreegen.t-sock");
    unlink(sockname);

    UniConfTestDaemon daemon(sockname, "temp:");
    
    UniConfRoot uniconf;
    UniClientGen *client_gen = create_client_conn("top", sockname, 
            debug_client_factory);
    uniconf.mountgen(client_gen); 

    UniConfRoot sub_uniconf;
    UniClientGen *sub_client_gen = create_client_conn("sub", sockname, 
            restrict_client_factory);
    sub_uniconf.mountgen(sub_client_gen);

    UniWatchList watches;
    watches.add(uniconf, restrict_callback, true);
    watches.add(sub_uniconf, sub_restrict_callback, true);
    
    uniconf.getme();
    sub_uniconf.getme();

    uniconf["sub"].setme("");
    uniconf["another-sub"].setme("");
    uniconf.getme();
    sub_uniconf.getme();

    restrict_callback_count = 0;
    sub_restrict_callback_count = 0;
    uniconf["sub/foo"].setme("bar");
    uniconf.getme();
    WVPASSEQ(restrict_callback_count, 1);
    sub_uniconf.getme();
    WVPASSEQ(sub_restrict_callback_count, 1);

    restrict_callback_count = 0;
    sub_restrict_callback_count = 0;
    uniconf["another-sub/foo"].setme("bar");
    uniconf.getme();
    WVPASSEQ(restrict_callback_count, 1);
    sub_uniconf.getme();
    WVPASSEQ(sub_restrict_callback_count, 0); // Should not have received callbacks
}


WVTEST_MAIN("timeout")
{
    signal(SIGPIPE, SIG_IGN);

    WvString sockname = wvtmpfilename("unisubtreegen.t-sock");
    unlink(sockname);

    UniConfTestDaemon daemon(sockname, "temp:");
    
    UniConfRoot uniconf;
    UniClientGen *client_gen = create_client_conn("timeout", sockname);
    uniconf.mountgen(client_gen); 

    client_gen->set_timeout(1000);

    uniconf.getme();
    WVPASS(client_gen->isok());
    kill(daemon.get_pid(), SIGSTOP);
    uniconf.getme();
    WVPASS(!client_gen->isok());

    kill(daemon.get_pid(), SIGCONT);
}
