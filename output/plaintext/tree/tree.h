#if !defined(OUTPUT_PLAINTEXT_TREE_H)
#define OUTPUT_PLAINTEXT_TREE_H

#include "utilities/boolean.h"
#include "stipulation/slice.h"

/* Instrument the stipulation structure with slices that implement
 * plaintext tree mode output.
 * @param si identifies slice where to start
 * @param is_setplay is si part of set play?
 */
void stip_insert_output_plaintext_tree_slices(slice_index si, boolean is_setplay);

/* Write a possibly pending move decoration
 */
void output_plaintext_tree_write_pending_move_decoration(void);

/* Write a move in battle play
 */
void output_plaintext_tree_write_move(void);

#endif
