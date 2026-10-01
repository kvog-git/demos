// ================================================================================================
// Huffman encoding demo
//
// ref: https://en.wikipedia.org/wiki/Huffman_coding#Basic_technique
//
// Changelog:
//     9/29/2026: Initial release
//     10/1/2026: Added decode + round-trip check
//
// License:
//     SPDX-License-Identifier: 0BSD
//     Copyright (c) 2026 Hunter Kvalevog
//
//     Permission to use, copy, modify, and/or distribute this software for any
//     purpose with or without fee is hereby granted.
//
//     THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
//     WITH REGARD TO THIS SOFTWARE.
// ================================================================================================

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define DEBUG

#ifdef __GNUC__
#  define ATTR_NORETURN       __attribute__((noreturn))
#  define ATTR_PRINTF(A1, A2) __attribute__((format(printf, A1, A2)))
#else
#  define ATTR_NORETURN
#  define ATTR_PRINTF(A1, A2)
#endif

ATTR_NORETURN
ATTR_PRINTF(1, 2)
static void die(const char *fmt, ...)
{
    va_list va; va_start(va, fmt);
    vprintf(fmt, va);
    va_end(va);

    exit(1);
}

// bit writer
typedef struct BitW BitW;
struct BitW
{
    uint8_t *bbuf;
    size_t   blen;
    size_t   cbyte;
    size_t   cbit;
};

static void write_bit(BitW *bw, uint8_t val)
{
    assert(!(val & ~1));
    assert(bw->cbyte < bw->blen);
    bw->bbuf[bw->cbyte] |= (val << (7 - bw->cbit));
    bw->cbit += 1;
    if (bw->cbit >= 8)
    {
        bw->cbyte += 1;
        bw->cbit = 0;
    }
}

static void write_bits(BitW *bw, uint64_t val, uint8_t len)
{
    for (uint8_t i = 0; i < len; ++i)
        write_bit(bw, (val >> (len - i - 1)) & 1);
}

// bit reader
typedef struct BitR BitR;
struct BitR
{
    const uint8_t *bbuf;
    size_t         blen;
    size_t         cbyte;
    size_t         cbit;
};

static uint8_t read_bit(BitR *br)
{
    assert(br->cbyte < br->blen);
    uint8_t bit = (br->bbuf[br->cbyte] >> (7 - br->cbit)) & 1;
    br->cbit += 1;
    if (br->cbit >= 8)
    {
        br->cbyte += 1;
        br->cbit = 0;
    }
    return bit;
}

static uint64_t read_bits(BitR *br, uint8_t len)
{
    uint64_t r = 0;
    for (size_t i = 0; i < len; ++i)
    {
        r <<= 1;
        r |= read_bit(br);
    }
    return r;
}

// This demo encodes raw bytes (domain [0, 255])
#define MAX_SYMBOLS 256

// In a Huffman tree, the number of internal nodes is always one less than the number of symbols.
// ...according to Wikipedia. I don't know how to prove this.
#define MAX_INTERNAL_NODES (MAX_SYMBOLS - 1)

#define MAX_NODES (MAX_SYMBOLS + MAX_INTERNAL_NODES)

typedef struct Node Node;
struct Node
{
    uint8_t sym;
    int32_t weight;

    Node *tl; // tree left
    Node *tr; // tree right

    Node *ln; // linked list next
};

static int sort_node_freq(const void *p1, const void *p2)
{
    const Node *n1 = p1;
    const Node *n2 = p2;
    return (n1->weight > n2->weight) - (n1->weight < n2->weight);
}

// arena allocator for internal nodes
static Node *new_node(void)
{
    static Node   buf[MAX_NODES * 2];
    static size_t idx = 0;
    assert(idx < MAX_NODES * 2);
    return &buf[idx++];
}

typedef struct Code Code;
struct Code
{
    uint64_t bits;
    uint8_t  len;
};

static void build_dictionary(Node *node, uint64_t bits, uint8_t len, Code *dictionary)
{
    // Huffman tree nodes always have 0 or 2 children
    if (!node->tl)
    {
        // leaf
        dictionary[node->sym].bits = bits;
        dictionary[node->sym].len  = len;
        return;
    }

    // internal
    build_dictionary(node->tl, (bits << 1) | 0, len + 1, dictionary); // left  = 0
    build_dictionary(node->tr, (bits << 1) | 1, len + 1, dictionary); // right = 1
}

static void write_tree(BitW *bw, Node *n)
{
    if (!n->tl)
    {
        // leaf
        write_bit(bw, 1);
        write_bits(bw, n->sym, 8);
        return;
    }

    // internal
    write_bit(bw, 0);
    write_tree(bw, n->tl);
    write_tree(bw, n->tr);
}

uint8_t *decompress(const uint8_t *bbuf, size_t blen, size_t *olen);

int main(int argc, char **argv)
{
    if (argc < 2)
        die("supply a file\n");

    FILE *f = fopen(argv[1], "rb");
    if (!f)
        die("failed to open file\n");

    fseek(f, 0, SEEK_END);
    const size_t ilen = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *ibuf = malloc(ilen);
    fread(ibuf, 1, ilen, f);

    // Set of all possible symbols
    Node syms[MAX_SYMBOLS] = { 0 };
    for (size_t i = 0; i < MAX_SYMBOLS; ++i)
        syms[i].sym = i & 0xFF;

    // Count frequencies of symbols in input data
    for (size_t i = 0; i < ilen; ++i)
        syms[ibuf[i]].weight += 1;

    // Sort by frequency
    qsort(syms, MAX_SYMBOLS, sizeof(Node), sort_node_freq);
#ifdef DEBUG
    for (size_t i = 0; i < MAX_SYMBOLS; ++i)
        if (syms[i].weight)
            printf("'%c': %d\n", (char)syms[i].sym, syms[i].weight);
#endif

    // Linked list-ify
    Node *queue = 0;
    for (size_t i = 1; i <= MAX_SYMBOLS; ++i)
    {
        Node *n = &syms[MAX_SYMBOLS - i];
        if (n->weight)
        {
            n->ln = queue;
            queue = n;
        }
    }
#ifdef DEBUG
    for (Node *n = queue; n; n = n->ln)
        printf("'%c':%d->", (char)n->sym, n->weight);
    printf("0\n");
#endif
    assert(queue);

    // Single-node trees do not generate valid Huffman codes. Add a dummy leaf.
    if (!queue->ln)
    {
        Node *n = new_node();
        n->sym = queue->sym ^ 1; // some value other than the existing one
        n->ln = queue;
        queue = n;
    }

    // Combine the two lightest nodes
    while (queue->ln)
    {
        Node *n1 = queue;
        Node *n2 = queue->ln;
        Node *n3 = new_node();
        // n3 adopts n1 and n2
        n3->tl = n1;
        n3->tr = n2;
        n3->weight = n1->weight + n2->weight;
        // n1 and n2 are removed from the list
        queue = queue->ln->ln;
        n1->ln = 0;
        n2->ln = 0;
        // n3 is inserted back into the sorted queue
        Node **p = &queue;
        while (*p && (*p)->weight <= n3->weight)
            p = &(*p)->ln;
        n3->ln= *p;
        *p = n3;
    }

    // Traverse tree to build dictionary
    Code dictionary[MAX_SYMBOLS] = { 0 };
    build_dictionary(queue, 0, 0, dictionary);
#ifdef DEBUG
    for (size_t i = 0; i < MAX_SYMBOLS; ++i)
    {
        Code *c = &dictionary[i];
        if (!c->len)
            continue;
        printf("%c: ", (char)i);
        for (size_t j = 0; j < c->len; ++j)
        {
            uint8_t bit = (c->bits >> (c->len - j - 1)) & 1;
            printf(bit ? "1" : "0");
        }
        printf("\n");
    }
#endif
    // Calculate maximum possible tree payload len
    uint64_t max_tree_bits = 10 * MAX_SYMBOLS;

    // Calculate data payload len
    uint64_t data_bits = 0;
    for (size_t i = 0; i < ilen; ++i)
        data_bits += dictionary[ibuf[i]].len;

    BitW bw = { 0 };
    bw.blen = (32 + data_bits + max_tree_bits + 7) / 8; // in bytes, rounded up
    bw.bbuf = calloc(1, bw.blen); assert(bw.bbuf);

    write_bits(&bw, ilen, 32);
    write_tree(&bw, queue);
    for (size_t i = 0; i < ilen; ++i)
    {
        Code *c = &dictionary[ibuf[i]];
        write_bits(&bw, c->bits, c->len);
    }


    size_t   olen = 0;
    uint8_t *obuf = decompress(bw.bbuf, bw.blen, &olen);

    printf("input size:      %zu bits\n", ilen * 8);
    printf("compressed size: %zu bits\n", bw.cbyte * 8 + bw.cbit);
    printf("output size:     %zu bits\n", olen * 8);

    if (olen != ilen || memcmp(obuf, ibuf, ilen))
        die("round trip mismatch");

    printf("round-trip success\n");
}

static Node *read_tree(BitR *br)
{
    Node *n = new_node();

    if (read_bit(br))
    {
        // leaf
        n->sym = read_bits(br, 8);
        return n;
    }

    // internal
    n->tl = read_tree(br);
    n->tr = read_tree(br);
    return n;
}

static Node *traverse(BitR *br, Node *n)
{
    Node *p = read_bit(br) ? n->tr : n->tl;
    if (!p->tl)
        return p;
    return traverse(br, p);
}

uint8_t *decompress(const uint8_t *bbuf, size_t blen, size_t *olen)
{
    BitR br = { bbuf, blen, 0, 0 };

    *olen = read_bits(&br, 32);
    uint8_t *obuf = malloc(*olen);

    Node *root = read_tree(&br);

    for (size_t i = 0; i < *olen; ++i)
    {
        obuf[i] = traverse(&br, root)->sym;
    }

#ifdef DEBUG
    printf("%.*s\n", (int)*olen, (const char *)obuf);
#endif
    return obuf;
}

