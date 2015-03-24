/**
 * =====================================================================================
 *
 *          @file:  lru.h
 *         @brief:  LRU에 대한 라이브러리
 *
 *        Version:  1.0
 *          @date:  2014년 11월 23일 19시 16분 39초
 *       Revision:  none
 *       Compiler:  gcc
 *
 *        @author:  Park jun hyung (), google@dankook.ac.kr
 *       @COMPANY:  Dankopok univ.
 * =====================================================================================
 */

#include <math.h>
#include <stdio.h>
#include "dk_list.h"
// #include "hash.h"
#include "errno.h"

#define list_entry(ptr, type, field) \
  ((type*) (((char*)ptr) - offsetof(type,field)))

#define list_each(pos, head) \
  for (pos = (head)->next; pos != (head); pos = pos->next)

/* READ WRITE FLAGS */
#define READ  1
#define WRITE 2

/* SIZE */
#define KB (1024)
#define MB (KB * KB)
#define GB (MB * KB)

/* CONFIG */
#define CACHE_BLOCK_SIZE (4 * KB)
#define CACHE_SIZE (128 * MB)
#define CACHE_LEN (CACHE_SIZE/ CACHE_BLOCK_SIZE)

#define DEBUG_OPTION 0

struct workload
{/*{{{*/
  char *time;
  char *host;
  int disk_num;
  int type;
  unsigned long long offset;
  long size;
  long respone; //respones time

};/*}}}*/

struct cache_line
{/*{{{*/
  long line;
  struct list_head head;
  struct list_head hash;
};/*}}}*/

struct lru_hash
 {/* {{{*/
  long long size;
  struct list_head *bucket;
};/* }}}*/ 

struct cache_mem
{/*{{{*/
  struct list_head *list;

  long size;
  long max;

  long read;
  long write;
  long hit;

  struct lru_hash hash;
};/*}}}*/

int init_hash_list(struct cache_mem *cm, unsigned long s)
{/* {{{*/
  long i = 0;

  cm->hash.size = s;
  cm->hash.bucket = malloc(s * sizeof(struct list_head));

  for (i = 0; i < s; i++) {
    init_list(&cm->hash.bucket[i]);
  }

  return 0;
}/* }}}*/

/**
 * Init cache memory.
 * @param m : cache size (max list size)
 * @return : cache_mem strcut pointer
 */
struct cache_mem *init_cache_mem(unsigned long size)
{/*{{{*/
  struct cache_mem *cm = NULL;

  if (!(cm = malloc(sizeof(struct cache_mem))))
    return NULL;

  cm->list = malloc(sizeof(struct list_head));
  cm->list = init_lnode();

  if (!cm->list)
    return NULL;

  /* Init */
  cm->size = 0;
  cm->max = size;
  cm->read = 0;
  cm->write = 0;
  cm->hit = 0;

  init_hash_list(cm, size);

  return cm;
}/*}}}*/

/**
 * Report result. 
 * @param cm : cache memory struct
 */
void report_cm(struct cache_mem *cm)
{/*{{{*/
  printf("========== report ==========\n");
  printf("List (%10ld/%10ld)\n", cm->size, cm->max);
  printf("Read (%10ld/%10ld)\n", cm->hit, cm->read);
  printf("Write(%10ld/%10ld)\n", cm->write, cm->write);
  printf("========== report ==========\n");
}/*}}}*/

/**
 * Print cache memory.(cache list)
 * @param cm : cache memory struct
 * @return : error code
 */
int print_cm(struct cache_mem *cm)
{/*{{{*/
  int i = 0;

  struct list_head *tmp = NULL;
  struct cache_line *l = NULL;

  if (!cm)
    return -1;

  if (!DEBUG_OPTION)
    return -2;

  tmp = cm->list;
  if (!tmp)
    return -3;

  // printf("root node : %p \n", tmp);
  // printf("root node : %p \n", tmp->next);

  /* Print list node */
  printf("==========\n");
  while (i < cm->size) {
    tmp = tmp->next;
    l = container_of(tmp, struct cache_line, head);

    printf("LIST : %10d %10ld \n", i, l->line);
    i++;
  }

  return 0;
}/*}}}*/

/**
 * Del cache memory.(cache list)
 * @param cm : cache memory struct
 * @return : error code
 */
int del_cm(struct cache_mem *cm)
{/*{{{*/
  int i = 0;

  struct list_head *tmp = NULL;
  struct cache_line *l = NULL;

  if (!cm)
    return -1;

  tmp = cm->list;
  if (!tmp)
    return -3;

  /* Del list node */
  while (i < cm->size) {
    tmp = tmp->next;
    l = container_of(tmp, struct cache_line, head);

    // del
    list_del(tmp->prev);
    free(l);
    i++;
  }

  free(cm);
  return 0;
}/*}}}*/

/**
 * Lookup cache line. 
 * @param cm : cache memory struct
 * @param line : page number
 * @return : lookup result(line) or NULL
 */
static inline struct cache_line *LRU_lookup(struct cache_mem *cm, long long line)
{/*{{{*/
  struct list_head *tmp = NULL;
  unsigned long hash_num = NULL;

  if (!cm || line < 0)
    return NULL;

  hash_num = (line % cm->max);

  // Get hash.. //
  list_each(tmp, &cm->hash.bucket[hash_num]) {
    struct cache_line *l = list_entry(tmp, struct cache_line, hash);
    if (line == l->line) {
      // printf("lookup ok! %ld %ld \n", l->line, line);
      return l;
    }
    // printf("lookup faill.. %ld %ld \n", l->line, line);
  }

  return NULL;
}/*}}}*/

/**
 * Make new line.
 * @param line : line.
 * @return : new cache line.
 */
static struct cache_line *create_line(long long line)
{/* {{{*/
  struct cache_line *l = malloc(sizeof(struct cache_line));

  if (!l)
    return NULL;

  l->line = line;

  // Init list..//
  init_list(&l->head);
  init_list(&l->hash);

  return l;
}/* }}}*/ 

void hash_insert(struct cache_mem *cm, struct cache_line *l)
{/* {{{*/
  unsigned long hash = (l->line) % (cm->max);
  list_prepend(&l->hash, &cm->hash.bucket[hash]);
}/* }}}*/

/**
 * LRU cache
 * @param cm : cache memory strcut
 * @param line : write memort line
 * @retrun : error code
 */
int LRU_cache(struct cache_mem *cm, long long line)
{/*{{{*/
  struct cache_line *new = NULL;
  struct cache_line *lookup = NULL;

  if (!cm)
    return -1;

  lookup = LRU_lookup(cm, line);

  if (lookup) {
    /* lookup cache */
    list_move(&lookup->head, cm->list);

    /* return Hit */
    return 1;

  } else {
    // not lookup cache
    
    /* make new line node */
    new = create_line(line);
    if (!new)
      return -1;
 
    /* add cache hash list and LRU list */
    hash_insert(cm, new);
    list_add(&new->head, cm->list);

    // list size over..
    if (cm->max <= cm->size) {

      /* lookup tail node and del struct */
      lookup = container_of(cm->list->prev, struct cache_line, head);
      list_remove(&lookup->hash);
      list_del(cm->list->prev);
      free(lookup);

    } else {
      cm->size++;
    }

    // return not hit
    return 2;
  }

}/*}}}*/

/**
 * run cache.
 * @param cm : cache memory info strcut
 * @param wl : target workload struct
 * @return : error code
 */
int run_cache(struct cache_mem *cm, struct workload *wl)
{/*{{{*/
  int i = 0;
  int ret = 0;
  double start = 0;
  double end = 0;

  /* NULL arg */
  if (!cm || !wl) {
    printf("[FAIL] arg NULL, %s \n", __func__);
    return -1;
  }

  /* start sector number and size(end) */
  // printf("1 s : %.2f , c : %.2f \n", wl->offset, wl->size);
  // printf("2 s : %.2f , c : %.2f \n", wl->offset/4096, wl->size/4096);
  start = floor(wl->offset / CACHE_BLOCK_SIZE);
  end = floor((wl->offset + wl->size) / CACHE_BLOCK_SIZE);
  // printf("3 s : %14.2f, c : %14.2f \n", start, end);

  /* print */
  // printf("%d %20s %15ld %10.2f %10ld %5.2f\n", wl->type, wl->time, wl->offset, start, wl->size, end);

   do {
     /* ret is errno or hit */
     ret = LRU_cache(cm, start + i);

     /* count... (statistics) */
     if (wl->type == READ) {
       cm->read++;
       if (ret == 1) {
         // printf("! HIT ! %.2lf \n", start + i);
         cm->hit++;
       }
     } else if (wl->type == WRITE) {
       cm->write++;
     }

     i++;
   } while (start + i <= end);

   return 0;
}/*}}}*/

/**
 * Just open file and return.
 * @param file : target file path
 * @return : file pointer
 */
FILE *open_workload(char *file)
{/*{{{*/
  FILE *fp = NULL;

  /* NULL arg */
  if (!file) {
    printf("[FAIL] arg NULL, %s \n", __func__);
    return NULL;
  }

  /* open FILE */
  if ((fp = fopen(file, "r")) < 0)
    return NULL;

  return fp;
}/*}}}*/

/**
 * read colum. (=Inscribe workload struct)
 * @param wl : workload struct
 * @param buf : buffer string
 * @return : error code
 */
int read_column(struct workload *wl, char *buf)
{/*{{{*/
  int column = 1;
  char *tmp = NULL;

  /* NULL arg */
  if (!wl || !buf) {
    printf("[FAIL] arg NULL, %s \n", __func__);
    return -1;
  }
  
  /* read file and, save workload struct */
  tmp = strtok(buf, ",");
  while (tmp != NULL) {

    switch (column) {
      /* 
       * TODO : strdup is cause of memory leak
       * But, This case is needless..
       */

      // case 1 : wl->time = strdup(tmp); break;
      // case 2 : wl->host = strdup(tmp); break;
      // case 3 : wl->disk_num = atoi(tmp); break;
      case 4 : (wl->type) = (strcmp(tmp, "Read") == 0) ? READ : WRITE; break;
      case 5 : wl->offset = atoll(tmp); break;
      case 6 : wl->size = atol(tmp); break;
      // case 7 : wl->respone = atol(tmp); break;
    }

    /* Next */
    tmp = strtok(NULL, ",");
    column++;
  }

  free(tmp);
  return 0;
}/*}}}*/

/**
 * cache simulator main. read worklosd and analysis..
 * @param fp : file pointer
 * @cache_size : cache size
 * @return : error code
 */
int read_workload(FILE *fp, long cache_size)
{/*{{{*/
  int ret = 0;
  char buf[100];
  struct cache_mem *cm = NULL;
  struct workload *wl = NULL;

  /* NULL arg test */
  if (!fp)
    printf("arg is NULL\n");

  /*  allocation */
  wl = malloc(sizeof(struct workload));
  cm = init_cache_mem(cache_size / CACHE_BLOCK_SIZE);
  if (!wl || !cm)
    goto end;

  /* read line by line */
  while (!feof(fp)){

    /* read line by line the file contain. save buf */
    if (fscanf(fp, "%s", buf) < 0)
      goto end;

    /* read buf and Inscribe wl */
    if (read_column(wl, buf) < 0)
      goto end;

    /* RUN cache mem */
    run_cache(cm, wl);
  }

end:
  /* reprot */
  report_cm(cm);

  /* DEBUG.. PRINT LIST */
  if (DEBUG_OPTION) 
    ret = print_cm(cm);
  if (ret < 0)
    printf("Err\n");

  /* terminate  */
  del_cm(cm);
  printf("END\n");

  return 0;
}/*}}}*/

