/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 2002 Net Integration Technologies, Inc.
 *
 * UniListGen is a UniConf generator to allow multiple generators to be
 * stacked in a priority sequence for get/set/etc.
 * 
 */

#include "unilistgen.h"
#include "wvmoniker.h"
#include "wvtclstring.h"


// if 'obj' is non-NULL and is a UniConfGen then whoever invoked this is being
// silly. we'll make a list and add the single generator to it anyways, for the
// sake of not breaking things
//
// otherwise, treat the moniker as a tcl list of monikers and add the generator
// made by each moniker to the generator list
static UniConfGen *creator(WvStringParm s, IObject *obj, void *)
{
    UniConfGenList *l = new UniConfGenList();
    UniConfGen *gen = NULL;

    if (obj)
    {
        gen = mutate<UniConfGen>(obj);
        if (gen)
            l->append(gen, true);
    }

    if (!gen)
    {
        WvStringList gens;
        wvtcl_decode(gens, s);
        WvStringList::Iter i(gens);

        for (i.rewind(); i.next();)
        {
            gen = wvcreate<UniConfGen>(i());
            if (gen)
                l->append(gen, true);
        }
    }

    return new UniListGen(l);
}
 
static WvMoniker<UniConfGen> reg("list", creator);


void UniListGen::commit()
{
    for (i.rewind(); i.next();)
        i().commit();
}

bool UniListGen::refresh()
{
    bool result = true;

    for (i.rewind(); i.next();)
        result = result && i().refresh();
    return result;
}

WvString UniListGen::get(const UniConfKey &key)
{
    for (i.rewind(); i.next();)
    {
        if (i().exists(key))
            return i().get(key);
    }

    return WvString::null;
}

void UniListGen::set(const UniConfKey &key, WvStringParm value)
{
    for (i.rewind(); i.next();)
        i().set(key, value);
}

bool UniListGen::exists(const UniConfKey &key)
{
    for (i.rewind(); i.next();)
    {
        if (i().exists(key))
            return true;
    }
    return false;
}

bool UniListGen::haschildren(const UniConfKey &key)
{
    for (i.rewind(); i.next();)
    {
        if (i().haschildren(key))
            return true;
    }
    return false;
}

bool UniListGen::isok()
{
    for (i.rewind(); i.next();)
    {
        if (!i().isok())
            return false;
    }
    return true;
}

UniConfGen::Iter *UniListGen::iterator(const UniConfKey &key)
{
    return i().iterator(key);
}


/***** UniListGen::IterIter *****/
