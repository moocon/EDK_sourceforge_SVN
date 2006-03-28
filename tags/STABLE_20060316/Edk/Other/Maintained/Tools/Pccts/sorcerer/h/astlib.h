#ifndef astlib_h
#define astlib_h

/*
 * astlib.h -- C ast_* library header
 *
 * SOFTWARE RIGHTS
 *
 * We reserve no LEGAL rights to SORCERER -- SORCERER is in the public
 * domain.  An individual or company may do whatever they wish with
 * source code distributed with SORCERER or the code generated by
 * SORCERER, including the incorporation of SORCERER, or its output, into
 * commerical software.
 *
 * We encourage users to develop software with SORCERER.  However, we do
 * ask that credit is given to us for developing SORCERER.  By "credit",
 * we mean that if you incorporate our source code into one of your
 * programs (commercial product, research project, or otherwise) that you
 * acknowledge this fact somewhere in the documentation, research report,
 * etc...  If you like SORCERER and have developed a nice tool with the
 * output, please mention that you developed it using SORCERER.  In
 * addition, we ask that this header remain intact in our source code.
 * As long as these guidelines are kept, we expect to continue enhancing
 * this system and expect to make other tools available as they are
 * completed.
 *
 * SORCERER 1.00B
 * Terence Parr
 * AHPCRC, University of Minnesota
 * 1992-1994
 */
#include <setjmp.h>
#include "sorcerer.h"
#include "sorlist.h"

#define MaxTreeStackDepth  400

#ifdef __USE_PROTOS
extern SORAST *ast_make(SORAST *rt, ...);
extern SORAST *ast_find_all(SORAST *t, SORAST *u, SORAST **cursor);
extern int ast_match(SORAST *t, SORAST *u);
extern void ast_insert_after(SORAST *a, SORAST *b);
extern void ast_append(SORAST *a, SORAST *b);
extern SORAST *ast_tail(SORAST *a);
extern SORAST *ast_bottom(SORAST *a);
extern SORAST *ast_cut_between(SORAST *a, SORAST *b);
extern SList *ast_to_slist(SORAST *t);
extern SORAST *slist_to_ast(SList *list);
extern void ast_free(SORAST *t);
extern int ast_scan(char *template, SORAST *tree, ...);
extern int ast_nsiblings(SORAST *t);
extern SORAST *ast_sibling_index(SORAST *t, int i);
extern int ast_match_partial(SORAST *t, SORAST *u);
#else
extern SORAST *ast_make();
extern SORAST *ast_find_all();
extern int ast_match();
extern void ast_insert_after();
extern void ast_append();
extern SORAST *ast_tail();
extern SORAST *ast_bottom();
extern SORAST *ast_cut_between();
extern SList *ast_to_slist();
extern SORAST *slist_to_ast();
extern void ast_free();
extern int ast_scan();
extern int ast_nsiblings();
extern SORAST *ast_sibling_index();
extern int ast_match_partial();
#endif

#endif
