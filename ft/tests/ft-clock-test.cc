/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"



enum ftnode_verify_type {
    read_all=1,
    read_compressed,
    read_none
};

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

static int
string_key_cmp(DB *UU(e), const DBT *a, const DBT *b)
{
    char *CAST_FROM_VOIDP(s, a->data);
    char *CAST_FROM_VOIDP(t, b->data);
    return strcmp(s, t);
}

static int omt_cmp(OMTVALUE p, void *q)
{
    LEAFENTRY CAST_FROM_VOIDP(a, p);
    LEAFENTRY CAST_FROM_VOIDP(b, q);
    void *ak, *bk;
    uint32_t al, bl;
    ak = le_key_and_len(a, &al);
    bk = le_key_and_len(b, &bl);
    int l = MIN(al, bl);
    int c = memcmp(ak, bk, l);
    if (c < 0) { return -1; }
    if (c > 0) { return +1; }
    int d = al - bl;
    if (d < 0) { return -1; }
    if (d > 0) { return +1; }
    else       { return  0; }
}

static LEAFENTRY
le_fastmalloc(const char *key, int keylen, const char *val, int vallen)
{
    LEAFENTRY CAST_FROM_VOIDP(r, toku_malloc(sizeof(r->type) + sizeof(r->keylen) + sizeof(r->u.clean.vallen) +
                                             keylen + vallen));
    resource_assert(r);
    r->type = LE_CLEAN;
    r->keylen = keylen;
    r->u.clean.vallen = vallen;
    memcpy(&r->u.clean.key_val[0], key, keylen);
    memcpy(&r->u.clean.key_val[keylen], val, vallen);
    return r;
}

static LEAFENTRY
le_malloc(const char *key, const char *val)
{
    int keylen = strlen(key) + 1;
    int vallen = strlen(val) + 1;
    return le_fastmalloc(key, keylen, val, vallen);
}


static void
test1(int fd, FT brt_h, FTNODE *dn) {
    int r;
    struct ftnode_fetch_extra bfe_all;
    brt_h->compare_fun = string_key_cmp;
    fill_bfe_for_full_read(&bfe_all, brt_h);
    FTNODE_DISK_DATA ndd = NULL;
    r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, dn, &ndd, &bfe_all);
    bool is_leaf = ((*dn)->height == 0);
    assert(r==0);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    // should sweep and NOT get rid of anything
    PAIR_ATTR attr;
    memset(&attr,0,sizeof(attr));
    toku_ftnode_pe_callback(*dn, attr, &attr, brt_h);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    // should sweep and get compress all
    toku_ftnode_pe_callback(*dn, attr, &attr, brt_h);
    for (int i = 0; i < (*dn)->n_children; i++) {
        if (!is_leaf) {
            assert(BP_STATE(*dn,i) == PT_COMPRESSED);
        }
        else {
            assert(BP_STATE(*dn,i) == PT_ON_DISK);
        }
    }
    PAIR_ATTR size;
    bool req = toku_ftnode_pf_req_callback(*dn, &bfe_all);
    assert(req);
    toku_ftnode_pf_callback(*dn, ndd, &bfe_all, fd, &size);
    toku_ftnode_pe_callback(*dn, attr, &attr, brt_h);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    // should sweep and get compress all
    toku_ftnode_pe_callback(*dn, attr, &attr, brt_h);
    for (int i = 0; i < (*dn)->n_children; i++) {
        if (!is_leaf) {
            assert(BP_STATE(*dn,i) == PT_COMPRESSED);
        }
        else {
            assert(BP_STATE(*dn,i) == PT_ON_DISK);
        }
    }    

    req = toku_ftnode_pf_req_callback(*dn, &bfe_all);
    assert(req);
    toku_ftnode_pf_callback(*dn, ndd, &bfe_all, fd, &size);
    toku_ftnode_pe_callback(*dn, attr, &attr, brt_h);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    (*dn)->dirty = 1;
    toku_ftnode_pe_callback(*dn, attr, &attr, brt_h);
    toku_ftnode_pe_callback(*dn, attr, &attr, brt_h);
    toku_ftnode_pe_callback(*dn, attr, &attr, brt_h);
    toku_ftnode_pe_callback(*dn, attr, &attr, brt_h);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    toku_free(ndd);
    toku_ftnode_free(dn);
}


static int search_cmp(struct ft_search* UU(so), DBT* UU(key)) {
    return 0;
}

static void
test2(int fd, FT brt_h, FTNODE *dn) {
    struct ftnode_fetch_extra bfe_subset;
    DBT left, right;
    DB dummy_db;
    memset(&dummy_db, 0, sizeof(dummy_db));
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    ft_search_t search_t;
    
    brt_h->compare_fun = string_key_cmp;
    fill_bfe_for_subset_read(
        &bfe_subset,
        brt_h,
        ft_search_init(
            &search_t, 
            search_cmp, 
            FT_SEARCH_LEFT, 
            NULL, 
            NULL
            ),
        &left,
        &right,
        true,
        true,
        false,
        false
        );
    FTNODE_DISK_DATA ndd = NULL;
    int r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, dn, &ndd, &bfe_subset);
    assert(r==0);
    bool is_leaf = ((*dn)->height == 0);
    // at this point, although both partitions are available, only the 
    // second basement node should have had its clock
    // touched
    assert(BP_STATE(*dn, 0) == PT_AVAIL);
    assert(BP_STATE(*dn, 1) == PT_AVAIL);
    assert(BP_SHOULD_EVICT(*dn, 0));
    assert(!BP_SHOULD_EVICT(*dn, 1));
    PAIR_ATTR attr;
    memset(&attr,0,sizeof(attr));
    toku_ftnode_pe_callback(*dn, attr, &attr, brt_h);
    assert(BP_STATE(*dn, 0) == (is_leaf) ? PT_ON_DISK : PT_COMPRESSED);
    assert(BP_STATE(*dn, 1) == PT_AVAIL);
    assert(BP_SHOULD_EVICT(*dn, 1));
    toku_ftnode_pe_callback(*dn, attr, &attr, brt_h);
    assert(BP_STATE(*dn, 1) == (is_leaf) ? PT_ON_DISK : PT_COMPRESSED);

    bool req = toku_ftnode_pf_req_callback(*dn, &bfe_subset);
    assert(req);
    toku_ftnode_pf_callback(*dn, ndd, &bfe_subset, fd, &attr);
    assert(BP_STATE(*dn, 0) == PT_AVAIL);
    assert(BP_STATE(*dn, 1) == PT_AVAIL);
    assert(BP_SHOULD_EVICT(*dn, 0));
    assert(!BP_SHOULD_EVICT(*dn, 1));

    toku_free(ndd);
    toku_ftnode_free(dn);
}

static void
test3_leaf(int fd, FT brt_h, FTNODE *dn) {
    struct ftnode_fetch_extra bfe_min;
    DBT left, right;
    DB dummy_db;
    memset(&dummy_db, 0, sizeof(dummy_db));
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    
    brt_h->compare_fun = string_key_cmp;
    fill_bfe_for_min_read(
        &bfe_min,
        brt_h
        );
    FTNODE_DISK_DATA ndd = NULL;
    int r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, dn, &ndd, &bfe_min);
    assert(r==0);
    //
    // make sure we have a leaf
    //
    assert((*dn)->height == 0);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn, i) == PT_ON_DISK);
    }
    toku_ftnode_free(dn);
    toku_free(ndd);
}

static void
test_serialize_nonleaf(void) {
    //    struct ft_handle source_ft;
    struct ftnode sn, *dn;

    int fd = open(TOKU_TEST_FILENAME, O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    //    source_ft.fd=fd;
    sn.max_msn_applied_to_node_on_disk.msn = 0;
    char *hello_string;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = FT_LAYOUT_VERSION;
    sn.layout_version_original = FT_LAYOUT_VERSION;
    sn.height = 1;
    sn.n_children = 2;
    sn.dirty = 1;
    sn.oldest_referenced_xid_known = TXNID_NONE;
    hello_string = toku_strdup("hello");
    MALLOC_N(2, sn.bp);
    MALLOC_N(1, sn.childkeys);
    toku_fill_dbt(&sn.childkeys[0], hello_string, 6);
    sn.totalchildkeylens = 6;
    BP_BLOCKNUM(&sn, 0).b = 30;
    BP_BLOCKNUM(&sn, 1).b = 35;
    BP_STATE(&sn,0) = PT_AVAIL;
    BP_STATE(&sn,1) = PT_AVAIL;
    set_BNC(&sn, 0, toku_create_empty_nl());
    set_BNC(&sn, 1, toku_create_empty_nl());
    //Create XIDS
    XIDS xids_0 = xids_get_root_xids();
    XIDS xids_123;
    XIDS xids_234;
    r = xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);
    r = xids_create_child(xids_123, &xids_234, (TXNID)234);
    CKERR(r);

    toku_bnc_insert_msg(BNC(&sn, 0), "a", 2, "aval", 5, FT_NONE, next_dummymsn(), xids_0, true, NULL, string_key_cmp);
    toku_bnc_insert_msg(BNC(&sn, 0), "b", 2, "bval", 5, FT_NONE, next_dummymsn(), xids_123, false, NULL, string_key_cmp);
    toku_bnc_insert_msg(BNC(&sn, 1), "x", 2, "xval", 5, FT_NONE, next_dummymsn(), xids_234, true, NULL, string_key_cmp);
    //Cleanup:
    xids_destroy(&xids_0);
    xids_destroy(&xids_123);
    xids_destroy(&xids_234);

    FT_HANDLE XMALLOC(brt);
    FT XCALLOC(brt_h);
    toku_ft_init(brt_h,
                 make_blocknum(0),
                 ZERO_LSN,
                 TXNID_NONE,
                 4*1024*1024,
                 128*1024,
                 TOKU_DEFAULT_COMPRESSION_METHOD);
    brt->ft = brt_h;
    
    toku_blocktable_create_new(&brt_h->blocktable);
    { int r_truncate = ftruncate(fd, 0); CKERR(r_truncate); }
    //Want to use block #20
    BLOCKNUM b = make_blocknum(0);
    while (b.b < 20) {
        toku_allocate_blocknum(brt_h->blocktable, &b, brt_h);
    }
    assert(b.b == 20);

    {
        DISKOFF offset;
        DISKOFF size;
        toku_blocknum_realloc_on_disk(brt_h->blocktable, b, 100, &offset, brt_h, fd, false);
        assert(offset==BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);

        toku_translate_blocknum_to_offset_size(brt_h->blocktable, b, &offset, &size);
        assert(offset == BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
        assert(size   == 100);
    }
    FTNODE_DISK_DATA ndd = NULL;
    r = toku_serialize_ftnode_to(fd, make_blocknum(20), &sn, &ndd, true, brt->ft, false);
    assert(r==0);

    test1(fd, brt_h, &dn);
    test2(fd, brt_h, &dn);

    toku_free(hello_string);
    destroy_nonleaf_childinfo(BNC(&sn, 0));
    destroy_nonleaf_childinfo(BNC(&sn, 1));
    toku_free(sn.bp);
    toku_free(sn.childkeys);
    toku_free(ndd);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h->h);
    toku_free(brt_h);
    toku_free(brt);

    r = close(fd); assert(r != -1);
}

static void
test_serialize_leaf(void) {
    //    struct ft_handle source_ft;
    struct ftnode sn, *dn;

    int fd = open(TOKU_TEST_FILENAME, O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = FT_LAYOUT_VERSION;
    sn.layout_version_original = FT_LAYOUT_VERSION;
    sn.height = 0;
    sn.n_children = 2;
    sn.dirty = 1;
    sn.oldest_referenced_xid_known = TXNID_NONE;
    LEAFENTRY elts[3];
    elts[0] = le_malloc("a", "aval");
    elts[1] = le_malloc("b", "bval");
    elts[2] = le_malloc("x", "xval");
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(1, sn.childkeys);
    toku_memdup_dbt(&sn.childkeys[0], "b", 2);
    sn.totalchildkeylens = 2;
    BP_STATE(&sn,0) = PT_AVAIL;
    BP_STATE(&sn,1) = PT_AVAIL;
    set_BLB(&sn, 0, toku_create_empty_bn());
    set_BLB(&sn, 1, toku_create_empty_bn());
    r = toku_omt_insert(BLB_BUFFER(&sn, 0), elts[0], omt_cmp, elts[0], NULL); assert(r==0);
    r = toku_omt_insert(BLB_BUFFER(&sn, 0), elts[1], omt_cmp, elts[1], NULL); assert(r==0);
    r = toku_omt_insert(BLB_BUFFER(&sn, 1), elts[2], omt_cmp, elts[2], NULL); assert(r==0);
    BLB_NBYTESINBUF(&sn, 0) = 2*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 0));
    BLB_NBYTESINBUF(&sn, 1) = 1*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 1));

    FT_HANDLE XMALLOC(brt);
    FT XCALLOC(brt_h);
    toku_ft_init(brt_h,
                 make_blocknum(0),
                 ZERO_LSN,
                 TXNID_NONE,
                 4*1024*1024,
                 128*1024,
                 TOKU_DEFAULT_COMPRESSION_METHOD);
    brt->ft = brt_h;
    
    toku_blocktable_create_new(&brt_h->blocktable);
    { int r_truncate = ftruncate(fd, 0); CKERR(r_truncate); }
    //Want to use block #20
    BLOCKNUM b = make_blocknum(0);
    while (b.b < 20) {
        toku_allocate_blocknum(brt_h->blocktable, &b, brt_h);
    }
    assert(b.b == 20);

    {
        DISKOFF offset;
        DISKOFF size;
        toku_blocknum_realloc_on_disk(brt_h->blocktable, b, 100, &offset, brt_h, fd, false);
        assert(offset==BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);

        toku_translate_blocknum_to_offset_size(brt_h->blocktable, b, &offset, &size);
        assert(offset == BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
        assert(size   == 100);
    }
    FTNODE_DISK_DATA ndd = NULL;
    r = toku_serialize_ftnode_to(fd, make_blocknum(20), &sn, &ndd, true, brt->ft, false);
    assert(r==0);

    test1(fd, brt_h, &dn);
    test3_leaf(fd, brt_h,&dn);

    for (int i = 0; i < sn.n_children-1; ++i) {
        toku_free(sn.childkeys[i].data);
    }
    for (int i = 0; i < 3; ++i) {
        toku_free(elts[i]);
    }
    for (int i = 0; i < sn.n_children; i++) {
        struct mempool * mp = &(BLB_BUFFER_MEMPOOL(&sn, i));
	toku_mempool_destroy(mp);
        destroy_basement_node(BLB(&sn, i));
    }
    toku_free(sn.bp);
    toku_free(sn.childkeys);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h->h);
    toku_free(brt_h);
    toku_free(brt);
    toku_free(ndd);
    r = close(fd); assert(r != -1);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    initialize_dummymsn();
    test_serialize_nonleaf();
    test_serialize_leaf();

    return 0;
}
