/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

/* Do test_log1, except abort instead of commit. */


#include <db.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/stat.h>
#include <unistd.h>


// ENVDIR is defined in the Makefile

DB_ENV *env;
DB *db;
DB_TXN *tid;

int
test_main (int UU(argc), char UU(*const argv[])) {
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);
    r=db_env_create(&env, 0); assert(r==0);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_PRIVATE|DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    {
        DBT key,data;
        dbt_init(&key, "hello", sizeof "hello");
        dbt_init(&data, "there", sizeof "there");
	r=db->put(db, tid, &key, &data, 0);
	CKERR(r);
    }
    r=db->close(db, 0);       
    assert(r==0);
    r=tid->abort(tid);    
    assert(r==0);
    r=env->close(env, 0);
#ifdef USE_BDB
#if DB_VERSION_MAJOR >= 5
    assert(r==0);
#else
    assert(r==ENOENT);
#endif
#else
    assert(r==0);
#endif
    {
	toku_struct_stat statbuf;
	r = toku_stat(ENVDIR "/foo.db", &statbuf);
	assert(r==-1);
	assert(errno==ENOENT);
    }
    return 0;
}