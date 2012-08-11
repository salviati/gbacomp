/*
   Copyright (c) Utkan Güngördü <utkan@freeconsole.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of

   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the

   GNU General Public License for more details


   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


/*----------------------------------------------------------------------------*/
/*--  huffman.c - Huffman coding for Nintendo GBA/DS                        --*/
/*--  Copyright (C) 2011 CUE                                                --*/
/*--                                                                        --*/
/*--  This program is free software: you can redistribute it and/or modify  --*/
/*--  it under the terms of the GNU General Public License as published by  --*/
/*--  the Free Software Foundation, either version 3 of the License, or     --*/
/*--  (at your option) any later version.                                   --*/
/*--                                                                        --*/
/*--  This program is distributed in the hope that it will be useful,       --*/
/*--  but WITHOUT ANY WARRANTY; without even the implied warranty of        --*/
/*--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          --*/
/*--  GNU General Public License for more details.                          --*/
/*--                                                                        --*/
/*--  You should have received a copy of the GNU General Public License     --*/
/*--  along with this program. If not, see <http://www.gnu.org/licenses/>.  --*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cprs.h"

/*----------------------------------------------------------------------------*/
#define CMD_DECODE    0x00       // decode
#define CMD_CODE_20   0x20       // Huffman magic number (to find best mode)
#define CMD_CODE_28   0x28       // 8-bits Huffman magic number
#define CMD_CODE_24   0x24       // 4-bits Huffman magic number
#define CMD_CODE_22   0x22       // 2-bits Huffman magic number (test mode)
#define CMD_CODE_21   0x21       // 1-bit  Huffman magic number (test mode)

#define HUF_LNODE     0          // left node
#define HUF_RNODE     1          // right node

#define HUF_SHIFT     1          // bits to shift
#define HUF_MASK      0x80       // first bit to check (1 << 7)
#define HUF_MASK4     0x80000000 // first bit to check (HUF_RNODE << 31)

#define HUF_LCHAR     0x80       // next lnode is a char, bit 7, (1 << 7)
#define HUF_RCHAR     0x40       // next rnode is a char, bit 6, (1 << 6)
#define HUF_NEXT      0x3F       // inc to next node/char (nwords+1), bits 5-0
                                 // * (0xFF & ~(HUF_LCHAR | HUF_RCHAR))

#define RAW_MINIM     0x00000000 // empty file, 0 bytes
#define RAW_MAXIM     0x00FFFFFF // 3-bytes length, 16MB - 1

#define HUF_MINIM     0x00000004 // empty RAW file (header only)
#define HUF_MAXIM     0x01400000 // 0x01000203, padded to 20MB:
                                 // * header, 4
                                 // * tree, 2 * 256
                                 // * length, RAW_MAXIM
                                 // 4 + 0x00000200 + 0x00FFFFFF + padding

/*----------------------------------------------------------------------------*/
typedef struct _huffman_node {
  unsigned int          symbol;
  unsigned int          weight;
  unsigned int          leafs;
  struct _huffman_node *dad;
  struct _huffman_node *lson;
  struct _huffman_node *rson;
} huffman_node;

typedef struct _huffman_code {
  unsigned int   nbits;
  unsigned char *codework;
} huffman_code;

unsigned int   *freqs;
huffman_node  **tree;
unsigned char  *codetree, *codemask;
huffman_code  **codes;
unsigned int    num_bits, max_symbols, num_leafs, num_nodes;

/*----------------------------------------------------------------------------*/
#define BREAK(text) { printf(text); return; }
#define EXIT(text)  { printf(text); exit(-1); }

/*----------------------------------------------------------------------------*/
void  Title(void);
void  Usage(void);
char *Load(char *file, int filelen);
void  Save(char *filename, char *buffer, int length);
char *Memory(int length, int size);

char* HUF_Decode(char *file, int filelen, unsigned int *outsize);
char* HUF_Encode(char *file, int filelen, int *outsize, int cmd);
char *HUF_Code(unsigned char *raw_buffer, int raw_len, int *new_len);

void  HUF_InitFreqs(void);
void  HUF_CreateFreqs(unsigned char *raw_buffer, int raw_len);
void  HUF_FreeFreqs(void);
void  HUF_InitTree(void);
void  HUF_CreateTree(void);
void  HUF_FreeTree(void);
void  HUF_InitCodeTree(void);
void  HUF_CreateCodeTree(void);
int   HUF_CreateCodeBranch(huffman_node *root, unsigned int p, unsigned int q);
void  HUF_UpdateCodeTree(void);
void  HUF_FreeCodeTree(void);
void  HUF_InitCodeWorks(void);
void  HUF_CreateCodeWorks(void);
void  HUF_FreeCodeWorks(void);


/*----------------------------------------------------------------------------*/
char *Load(char *file, int filelen) {
  char *fb;

  fb = Memory(filelen + 3, sizeof(char));
  memcpy(fb,file,filelen);
  return(fb);
}

/*----------------------------------------------------------------------------*/
char *Memory(int length, int size) {
  char *fb;

  fb = (char *) calloc(length * size, size); // FIXME(utkan): This doesn't look quite correct.
  if (fb == NULL) EXIT("\nMemory error\n");

  return(fb);
}

/*----------------------------------------------------------------------------*/
uint huffman_decode(RECORD *dst, const RECORD *src) {
	unsigned int outsize;
	dst->data = HUF_Decode(src->data, src->width*src->height, &outsize);
	dst->width = 1;
	dst->height = outsize;
	
	return dst->height;
}

char* HUF_Decode(char *file, int filelen, unsigned int *outsize) {
  unsigned char *pak_buffer, *raw_buffer, *pak, *raw, *pak_end, *raw_end;
  unsigned int   pak_len, raw_len, header;
  unsigned char *tree;
  unsigned int   pos, next, mask4, code, ch, nbits;

  *outsize = 0;

  pak_buffer = Load(file, filelen);
  pak_len = filelen;

  header = *pak_buffer;

  if ((header != CMD_CODE_24) && (header != CMD_CODE_28)) {
    free(pak_buffer);
    return NULL;
  }

  num_bits = header & 0xF;

  raw_len = *(unsigned int *)pak_buffer >> 8;
  raw_buffer = (unsigned char *) Memory(raw_len, sizeof(char));

  pak = pak_buffer + 4;
  raw = raw_buffer;
  pak_end = pak_buffer + pak_len;
  raw_end = raw_buffer + raw_len;

  tree = pak;
  pak += (*pak + 1) << 1;

  nbits = 0;

  pos = *(tree + 1);
  next = 0;

  mask4 = 0;
  while (raw < raw_end) {
    if (!(mask4 >>= HUF_SHIFT)) {
      if (pak + 3 >= pak_end) break;
      code = *(unsigned int *)pak;
      pak += 4;
      mask4 = HUF_MASK4;
    }

    next += ((pos & HUF_NEXT) + 1) << 1;

    if (!(code & mask4)) {
      ch = pos & HUF_LCHAR;
      pos = *(tree + next);
    } else {
      ch = pos & HUF_RCHAR;
      pos = *(tree + next + 1);
    }

    if (ch) {
      *raw = (*raw << num_bits) | pos;
      if (!(nbits = (nbits + num_bits) & 7)) raw++;

      pos = *(tree + 1);
      next = 0;
    }    
  }

  raw_len = raw - raw_buffer;

  if (raw != raw_end) {
	  //printf("unexpected end of encoded file!");
  }

  *outsize = raw_len;

  free(pak_buffer);

  return raw_buffer;
}

/*----------------------------------------------------------------------------*/
uint huffman_encode(RECORD *dst, const RECORD *src, int data_len) {
	if (data_len != 4 && data_len != 8) {
		return 0;
	}
	
	int cmd = data_len == 8 ? CMD_CODE_28 : CMD_CODE_24;
	unsigned int outsize;
	dst->data = HUF_Encode(src->data, src->width*src->height, &outsize, cmd);
	dst->width = 1;
	dst->height = outsize;
	
	return dst->height;
}


char* HUF_Encode(char *file, int filelen, int *outsize, int cmd) {
  unsigned char *raw_buffer, *pak_buffer, *new_buffer;
  unsigned int   raw_len, pak_len, new_len;


  num_bits = cmd & 0xF;

  raw_buffer = Load(file, filelen);
  raw_len = filelen;

  pak_buffer = NULL;
  pak_len = HUF_MAXIM + 1;

  if (!num_bits) {
    num_bits = CMD_CODE_28 - CMD_CODE_20;
    new_buffer = HUF_Code(raw_buffer, raw_len, &new_len);
    if (new_len < pak_len) {
      if (pak_buffer != NULL) free(pak_buffer);
      pak_buffer = new_buffer;
      pak_len = new_len;
    }
    num_bits = CMD_CODE_24 - CMD_CODE_20;
  }
  new_buffer = HUF_Code(raw_buffer, raw_len, &new_len);
  if (new_len < pak_len) {
    if (pak_buffer != NULL) free(pak_buffer);
    pak_buffer = new_buffer;
    pak_len = new_len;
  }

  free(raw_buffer);

  *outsize = pak_len;
  return pak_buffer;
}

/*----------------------------------------------------------------------------*/
char *HUF_Code(unsigned char *raw_buffer, int raw_len, int *new_len) {
  unsigned char *pak_buffer, *pak, *raw, *raw_end, *cod;
  unsigned int   pak_len, len;
  huffman_code  *code;
  unsigned char *cwork, mask;
  unsigned int  *pk4, mask4, ch, nbits;

  max_symbols = 1 << num_bits;

  pak_len = 4 + (max_symbols << 1) + raw_len + 3;
  pak_buffer = (unsigned char *) Memory(pak_len, sizeof(char));

  *(unsigned int *)pak_buffer = (CMD_CODE_20 + num_bits) | (raw_len << 8);

  pak = pak_buffer + 4;
  raw = raw_buffer;
  raw_end = raw_buffer + raw_len;

  HUF_InitFreqs();
  HUF_CreateFreqs(raw_buffer, raw_len);

  HUF_InitTree();
  HUF_CreateTree();

  HUF_InitCodeTree();
  HUF_CreateCodeTree();

  HUF_InitCodeWorks();
  HUF_CreateCodeWorks();

  cod = codetree;
  len = (*cod + 1) << 1;
  while (len--) *pak++ = *cod++;

  mask4 = 0;
  while (raw < raw_end) {
    ch = *raw++;

    for (nbits = 8; nbits; nbits -= num_bits) {
      code = codes[ch >> (8 - num_bits)];
      if (code == NULL) EXIT(", ERROR: code without codework!"); // never!

      len   = code->nbits;
      cwork = code->codework;

      mask = HUF_MASK;
      while (len--) {
        if (!(mask4 >>= HUF_SHIFT)) {
          mask4 = HUF_MASK4;
          *(pk4 = (unsigned int *)pak) = 0;
          pak += 4;
        }
        if (*cwork & mask) *pk4 |= mask4;
        if (!(mask >>= HUF_SHIFT)) {
          mask = HUF_MASK;
          cwork++;
        }
      }

      ch = (ch << num_bits) & 0xFF;
    }
  }

  pak_len = pak - pak_buffer;


  HUF_FreeCodeWorks();
  HUF_FreeCodeTree();
  HUF_FreeTree();
  HUF_FreeFreqs();

  *new_len = pak_len;

  return(pak_buffer);
}

/*----------------------------------------------------------------------------*/
void HUF_InitFreqs(void) {
  unsigned int i;

  freqs = (unsigned int *) Memory(max_symbols, sizeof(int));

  for (i = 0; i < max_symbols; i++) freqs[i] = 0;
}

/*----------------------------------------------------------------------------*/
void HUF_CreateFreqs(unsigned char *raw_buffer, int raw_len) {
  unsigned int ch, nbits;
  unsigned int i;

  for (i = 0; i < raw_len; i++) {
    ch = *raw_buffer++;
    for (nbits = 8; nbits; nbits -= num_bits) {
      freqs[ch >> (8 - num_bits)]++;
      ch = (ch << num_bits) & 0xFF;
    }
  }

  num_leafs = 0;
  for (i = 0; i < max_symbols; i++) if (freqs[i]) num_leafs++;


  if (num_leafs < 2) {
    if (num_leafs == 1) {
      for (i = 0; i < max_symbols; i++) {
        if (freqs[i]) {
          freqs[i] = 1;
          break;
        }
      }
    }

    while (num_leafs++ < 2) {
      for (i = 0; i < max_symbols; i++) {
        if (!freqs[i]) {
          freqs[i] = 2;
          break;
        }
      }
    }
  }

  num_nodes = (num_leafs << 1) - 1;
}

/*----------------------------------------------------------------------------*/
void HUF_FreeFreqs(void) {
  free(freqs);
}

/*----------------------------------------------------------------------------*/
void HUF_InitTree(void) {
  unsigned int i;

  tree = (huffman_node **) Memory(num_nodes, sizeof(huffman_node *));

  for (i = 0; i < num_nodes; i++) tree[i] = NULL;
}

/*----------------------------------------------------------------------------*/
void HUF_CreateTree(void) {
  huffman_node *node, *lnode, *rnode;
  unsigned int  lweight, rweight, num_node;
  unsigned int  i;

  num_node = 0;
  for (i = 0; i < max_symbols; i++) {
    if (freqs[i]) {
      node = (huffman_node *) Memory(1, sizeof(huffman_node));
      tree[num_node++] = node;

      node->symbol = i;
      node->weight = freqs[i];
      node->leafs  = 1;
      node->dad    = NULL;
      node->lson   = NULL;
      node->rson   = NULL;
    }
  }

  while (num_node < num_nodes) {
    lnode = rnode = NULL;
    lweight = rweight = 0;

    for (i = 0; i < num_node; i++) {
      if (tree[i]->dad == NULL) {
        if (!lweight || (tree[i]->weight < lweight)) {
          rweight = lweight;
          rnode   = lnode;
          lweight = tree[i]->weight;
          lnode   = tree[i];
        } else if (!rweight || (tree[i]->weight < rweight)) {
          rweight = tree[i]->weight;
          rnode   = tree[i];
        }
      }
    }

    node = (huffman_node *) Memory(1, sizeof(huffman_node));
    tree[num_node++] = node;

    node->symbol = num_node - num_leafs + max_symbols;
    node->weight = lnode->weight + rnode->weight;
    node->leafs  = lnode->leafs + rnode->leafs;
    node->dad    = NULL;
    node->lson   = lnode;
    node->rson   = rnode;

    lnode->dad = rnode->dad = node;
  }
}

/*----------------------------------------------------------------------------*/
void HUF_FreeTree(void) {
  unsigned int i;

  for (i = 0; i < num_nodes; i++) free(tree[i]);
  free(tree);
}

/*----------------------------------------------------------------------------*/
void HUF_InitCodeTree(void) {
  unsigned int max_nodes;
  unsigned int i;

  max_nodes = (((num_leafs - 1) | 1) + 1) << 1;

  codetree = (unsigned char *) Memory(max_nodes, sizeof(char));
  codemask = (unsigned char *) Memory(max_nodes, sizeof(char));

  for (i = 0; i < max_nodes; i++) {
    codetree[i] = 0;
    codemask[i] = 0;
  }
}

/*----------------------------------------------------------------------------*/
void HUF_CreateCodeTree(void) {
  unsigned int i;

  i = 0;

  codetree[i] = (num_leafs - 1) | 1;
  codemask[i] = 0;

  HUF_CreateCodeBranch(tree[num_nodes - 1], i + 1, i + 2);
  HUF_UpdateCodeTree();

  i = (codetree[0] + 1) << 1;
  while (--i) if (codemask[i] != 0xFF) codetree[i] |= codemask[i];
}

/*----------------------------------------------------------------------------*/
int HUF_CreateCodeBranch(huffman_node *root, unsigned int p, unsigned int q) {
  huffman_node **stack, *node;
  unsigned int  r, s, mask;
  unsigned int  l_leafs, r_leafs;

  if (root->leafs <= HUF_NEXT + 1) {
    stack = (huffman_node **) Memory(root->leafs, sizeof(huffman_node *));

    s = r = 0;
    stack[r++] = root;

    while (s < r) {
      if ((node = stack[s++])->leafs == 1) {
        if (s == 1) { codetree[p] = node->symbol; codemask[p]   = 0xFF; }
        else        { codetree[q] = node->symbol; codemask[q++] = 0xFF; }
      } else {
        mask = 0;
        if (node->lson->leafs == 1) mask |= HUF_LCHAR;
        if (node->rson->leafs == 1) mask |= HUF_RCHAR;

        if (s == 1) { codetree[p] = (r - s) >> 1; codemask[p]   = mask; }
        else        { codetree[q] = (r - s) >> 1; codemask[q++] = mask; }

        stack[r++] = node->lson;
        stack[r++] = node->rson;
      }
    }

    free(stack);
  } else {
    mask = 0;
    if (root->lson->leafs == 1) mask |= HUF_LCHAR;
    if (root->rson->leafs == 1) mask |= HUF_RCHAR;

    codetree[p] = 0; codemask[p] = mask;

    if (root->lson->leafs <= root->rson->leafs) {
      l_leafs = HUF_CreateCodeBranch(root->lson, q,     q + 2);
      r_leafs = HUF_CreateCodeBranch(root->rson, q + 1, q + (l_leafs << 1));
      codetree[q + 1] = l_leafs - 1;
    } else {
      r_leafs = HUF_CreateCodeBranch(root->rson, q + 1, q + 2);
      l_leafs = HUF_CreateCodeBranch(root->lson, q,     q + (r_leafs << 1));
      codetree[q] = r_leafs - 1;
    }
  }

  return(root->leafs);
}

/*----------------------------------------------------------------------------*/
void HUF_UpdateCodeTree(void) {
  unsigned int max, inc, n0, n1, l0, l1, tmp0, tmp1;
  unsigned int i, j, k;

  max = (codetree[0] + 1) << 1;

  for (i = 1; i < max; i++) {
    if ((codemask[i] != 0xFF) && (codetree[i] > HUF_NEXT)) {
      if ((i & 1) && (codetree[i-1] == HUF_NEXT)) {
        i--;
        inc = 1;
      } else if (!(i & 1) && (codetree[i+1] == HUF_NEXT)) {
        i++;
        inc = 1;
      } else {
        inc = codetree[i] - HUF_NEXT;
      }

      n1 = (i >> 1) + 1 + codetree[i];
      n0 = n1 - inc;

      l1 = n1 << 1;
      l0 = n0 << 1;

      tmp0 = *(short *)(codetree + l1);
      tmp1 = *(short *)(codemask + l1);
      for (j = l1; j > l0; j -= 2) {
        *(short *)(codetree + j) = *(short *)(codetree + j - 2);
        *(short *)(codemask + j) = *(short *)(codemask + j - 2);
      }
      *(short *)(codetree + l0) = tmp0;
      *(short *)(codemask + l0) = tmp1;

      codetree[i] -= inc;

      for (j = i + 1; j < l0; j++) {
        if (codemask[j] != 0xFF) {
          k = (j >> 1) + 1 + codetree[j];
          if ((k >= n0) && (k < n1)) codetree[j]++;
        }
      }

      if (codemask[l0 + 0] != 0xFF) codetree[l0 + 0] += inc;
      if (codemask[l0 + 1] != 0xFF) codetree[l0 + 1] += inc;

      for (j = l0 + 2; j < l1 + 2; j++) {
        if (codemask[j] != 0xFF) {
          k = (j >> 1) + 1 + codetree[j];
          if (k > n1) codetree[j]--;
        }
      }

      i = (i | 1) - 2;
    }
  }
}

/*----------------------------------------------------------------------------*/
void HUF_FreeCodeTree(void) {
  free(codemask);
  free(codetree);
}

/*----------------------------------------------------------------------------*/
void HUF_InitCodeWorks(void) {
  unsigned int i;

  codes = (huffman_code **) Memory(max_symbols, sizeof(huffman_code *));

  for (i = 0; i < max_symbols; i++) codes[i] = NULL;
}

/*----------------------------------------------------------------------------*/
void HUF_CreateCodeWorks(void) {
  huffman_node  *node;
  huffman_code  *code;
  unsigned int   symbol, nbits, maxbytes, nbit;
  unsigned char  scode[100], mask;
  unsigned int   i, j;

  for (i = 0; i < num_leafs; i++) {
    node   = tree[i];
    symbol = node->symbol;

    nbits = 0;
    while (node->dad != NULL) {
      scode[nbits++] = node->dad->lson == node ? HUF_LNODE : HUF_RNODE;
      node = node->dad;
    }
    maxbytes = (nbits + 7) >> 3;

    code = (huffman_code *) Memory(1, sizeof(huffman_code));

    codes[symbol]  = code;
    code->nbits    = nbits;
    code->codework = (unsigned char *) Memory(maxbytes, sizeof(char));

    for (j = 0; j < maxbytes; j++) code->codework[j] = 0;

    mask = HUF_MASK;
    j = 0;
    for (nbit = nbits; nbit; nbit--) {
      if (scode[nbit-1]) code->codework[j] |= mask;
      if (!(mask >>= HUF_SHIFT)) {
        mask = HUF_MASK;
        j++;
      }
    }
  }
}

/*----------------------------------------------------------------------------*/
void HUF_FreeCodeWorks(void) {
  unsigned int i;

  for (i = 0; i < max_symbols; i++) {
    if (codes[i] != NULL) {
      free(codes[i]->codework);
      free(codes[i]);
    }
  }
  free(codes);
}

/*----------------------------------------------------------------------------*/
/*--  EOF                                           Copyright (C) 2011 CUE  --*/
/*----------------------------------------------------------------------------*/
