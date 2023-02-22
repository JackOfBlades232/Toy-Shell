/* Toy-Shell/src/word_list.h */
#ifndef WORD_LIST_SENTRY
#define WORD_LIST_SENTRY

#include "word.h"

struct word_item;
struct word_list;

struct word_list *word_list_create();
void word_list_add_item(struct word_list *lst, struct word *wrd);
struct word *word_list_pop_first(struct word_list *lst);
void word_list_free(struct word_list *lst);

#endif
