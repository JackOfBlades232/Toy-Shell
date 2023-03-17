/* Toy-Shell/src/input.c */
#include "input.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>

enum {
    ctrl_w = 23,
    del = 127
};

enum { 
    read_bufsize = 1024,
    output_bufsize = 4096,
    match_bufsize = 128 
};

enum arrow_type { left, right, invalid };

struct positional_buffer {
    char *buf, *bufpos, *bufend;
    size_t bufsize;
};

static void putchar_now(int c)
{
    putchar(c);
    fflush(stdout);
}

static void putstr_now(const char *s)
{
    printf("%s", s);
    fflush(stdout);
}

static int char_is_separator(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void reset_buf(struct positional_buffer *pbuf)
{
    pbuf->bufpos = pbuf->buf;
    pbuf->bufend = pbuf->buf;
    *(pbuf->buf) = '\0';
}

static void init_buf(struct positional_buffer *pbuf, size_t bufsize)
{
    pbuf->bufsize = bufsize;
    pbuf->buf = malloc(bufsize);
    reset_buf(pbuf);
}

static size_t add_char_to_buf(struct positional_buffer *pbuf, char c)
{
    char *shift_p;
    size_t res;

    if (pbuf->bufend - pbuf->buf >= pbuf->bufsize - 1)
        return -1;

    res = pbuf->bufend - pbuf->bufpos;
    for (shift_p = pbuf->bufend; shift_p - pbuf->bufpos >= 0; shift_p--)
        *(shift_p+1) = *shift_p;

    *(pbuf->bufpos) = c;
    pbuf->bufpos++;
    pbuf->bufend++;
    return res;
}

static size_t remove_char_from_buf(struct positional_buffer *pbuf)
{
    char *shift_p;
    size_t res;

    if (pbuf->bufpos - pbuf->buf <= 0)
        return -1;

    res = pbuf->bufend - pbuf->bufpos;
    for (shift_p = pbuf->bufpos; (pbuf->bufend) - shift_p >= 0; shift_p++)
        *(shift_p-1) = *shift_p;

    pbuf->bufpos--;
    pbuf->bufend--;
    return res;
}

static size_t remove_word_from_buf(struct positional_buffer *pbuf)
{
    char *shift_p;
    size_t shift;

    if (pbuf->bufpos - pbuf->buf <= 0)
        return -1;

    shift_p = pbuf->bufpos - 1;
    while (char_is_separator(*shift_p) && shift_p - pbuf->buf > 0)
        shift_p--;
    while (!char_is_separator(*shift_p) && shift_p - pbuf->buf > 0)
        shift_p--;
    if (char_is_separator(*shift_p))
        shift_p++;

    shift = pbuf->bufpos - shift_p;
    for (; (pbuf->bufend) - shift_p >= shift; shift_p++)
        *shift_p = *(shift_p+shift);

    pbuf->bufpos -= shift;
    pbuf->bufend -= shift;
    return shift;
}

static void redraw_buf(char *bufp, int num_spaces)
{
    size_t i;
    size_t redraw_part_len = strlen(bufp) + num_spaces;
    printf("%s", bufp);
    for (i = 0; i < num_spaces; i++)
        putchar(' ');
    for (i = 0; i < redraw_part_len; i++)
        putchar('\b');
}

static void add_char(struct positional_buffer *pbuf, char c)
{
    size_t add_res = add_char_to_buf(pbuf, c);
    if (add_res == -1)
        return;

    putchar(c);
    if (add_res > 0)
        redraw_buf(pbuf->bufpos, 0);
}

static void rm_char(struct positional_buffer *pbuf)
{
    size_t rm_res = remove_char_from_buf(pbuf);
    if (rm_res == -1)
        return;

    putchar('\b');
    redraw_buf(pbuf->bufpos, 1);
}

static void rm_word(struct positional_buffer *pbuf)
{
    size_t i;
    size_t rm_shift = remove_word_from_buf(pbuf);
    if (rm_shift == -1)
        return;

    for (i = 0; i < rm_shift; i++)
        putchar('\b');
    redraw_buf(pbuf->bufpos, rm_shift);
}

/* static void output_to_eol(FILE *f, struct positional_buffer *pbuf)
{
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n')
            break;

        if (pbuf != NULL)
            add_char(pbuf, c);
        else
            putchar(c);
    }
}

static size_t look_up_word_by_prefix(FILE *dict, 
        const char *prefix, size_t prefix_len,
        long (*match_positions)[match_bufsize], long *last_match_end)
{
    int c;
    int line_may_match;
    const char *prefix_p = prefix;

    size_t num_matches = 0;
   
    fseek(dict, 0, SEEK_SET);

    line_may_match = 1;
    **match_positions = ftell(dict);
    *last_match_end = -1;
    while ((c = fgetc(dict)) != EOF) {
        if (line_may_match && prefix_p-prefix >= prefix_len) {
            if (num_matches < match_bufsize-1) {
                *last_match_end = ftell(dict)-1;
                num_matches++;
            } else
                return -1;
            
            line_may_match = 0;
        } 

        if (c == '\n') {
            prefix_p = prefix;
            line_may_match = 1;
            (*match_positions)[num_matches] = ftell(dict);
        } else if (line_may_match) {
            if (c == *prefix_p)
                prefix_p++;
            else
                line_may_match = 0;
        }
    }

    return num_matches;
}

static void complete_word(long dict_pos, FILE *dict_f,
        struct positional_buffer *pbuf)
{
    fseek(dict_f, dict_pos, SEEK_SET);
    output_to_eol(dict_f, pbuf);
}

static void output_multiple_matches(long *matches, size_t match_cnt, 
        FILE *dict_f, struct positional_buffer *pbuf)
{
    int i;
    putchar('\n');
    for (i = 0; i < match_cnt; i++) {
        fseek(dict_f, matches[i], SEEK_SET);
        output_to_eol(dict_f, NULL);
        putchar(' ');
    }
    putchar('\n');
    printf("%s", pbuf->buf);
    for (i = 0; i < pbuf->bufend - pbuf->bufpos; i++)
        putchar('\b');
}

static void perform_lookup(struct positional_buffer *pbuf, FILE *dict_f)
{
    char *prefix = pbuf->bufpos;
    size_t prefix_len;

    size_t match_cnt;
    long match_positions[match_bufsize];
    long last_match_end;

    while (prefix - pbuf->buf > 0) {
        prefix--;
        if (char_is_separator(*prefix)) {
            prefix++;
            break;
        }
    }

    prefix_len = pbuf->bufpos - prefix;
    match_cnt = look_up_word_by_prefix(
            dict_f, prefix, prefix_len,
            &match_positions, &last_match_end
            );

    if (match_cnt == -1)
        putstr_now("\nToo many options to display\n");
    else if (match_cnt == 1)
        complete_word(last_match_end, dict_f, pbuf);
    else if (match_cnt > 1)
        output_multiple_matches(match_positions, match_cnt, dict_f, pbuf);
}
*/

static void add_char_with_esc(
        struct positional_buffer *out_pbuf, char c, int *acc_esc)
{
    if (*acc_esc > 0) {
        if (*acc_esc >= 2)
            add_char(out_pbuf, '[');
        *acc_esc = 0;
    }

    add_char(out_pbuf, c);
}

static void process_esc_bracket(struct positional_buffer *out_pbuf, int *acc_esc)
{
    if (*acc_esc == 1)
        (*acc_esc)++;
    else
        add_char_with_esc(out_pbuf, '[', acc_esc);
}

static void execute_arrow(struct positional_buffer *out_pbuf, enum arrow_type type)
{
    if (type == left && out_pbuf->bufpos - out_pbuf->buf > 0) {
        out_pbuf->bufpos--;
        putchar_now('\b');
    } else if (type == right && out_pbuf->bufend - out_pbuf->bufpos > 0) {
        putchar_now(*out_pbuf->bufpos);
        out_pbuf->bufpos++;
    }
}

static void process_esc_letter(struct positional_buffer *out_pbuf, 
        char c, int *acc_esc)
{
    enum arrow_type type = c == 'D' ? left : (c == 'C' ? right : invalid);
    if (type != invalid && *acc_esc == 2) {
        execute_arrow(out_pbuf, type);
        *acc_esc = 0;
    } else
        add_char_with_esc(out_pbuf, c, acc_esc);
}

static char *parse_input()
{
    char read_buf[read_bufsize];
    struct positional_buffer out_pbuf;

    int read_res;
    char *r_bufp;

    int arrow_chars_acc;

    init_buf(&out_pbuf, output_bufsize);
    arrow_chars_acc = 0;
    while ((read_res = read(0, read_buf, sizeof(read_buf))) > 0) {
        for (r_bufp = read_buf; r_bufp-read_buf < read_res; r_bufp++) {
            switch (*r_bufp) {
                case '\4': /* CTRL_D */
                    if (out_pbuf.bufend == out_pbuf.buf) {
                        free(out_pbuf.buf);
                        out_pbuf.buf = NULL;
                        goto end_of_file;
                    }
                    break;
                case '\n':
                    putchar_now('\n');
                    goto end_of_file;
                case '\t':
                    /* perform_lookup(&out_pbuf, dict_f); */
                    break;
                case '\b':
                    rm_char(&out_pbuf);
                    break;
                case del:
                    rm_char(&out_pbuf);
                    break;
                case ctrl_w:
                    rm_word(&out_pbuf);
                    break;

                /* escape seq for arrows */    
                case '\e': 
                    arrow_chars_acc = 1;
                    break;
                case '[':
                    process_esc_bracket(&out_pbuf, &arrow_chars_acc);
                    break;
                case 'D':
                    process_esc_letter(&out_pbuf, *r_bufp, &arrow_chars_acc);
                    break;
                case 'C':
                    process_esc_letter(&out_pbuf, *r_bufp, &arrow_chars_acc);
                    break;

                default:
                    add_char_with_esc(&out_pbuf, *r_bufp, &arrow_chars_acc);
                    break;
            }

            fflush(stdout);
        }
    }

end_of_file:
    return out_pbuf.buf;
}

char *read_input_line()
{
    char *res;
    struct termios ts1, ts2;

    tcgetattr(0, &ts1); 
    memcpy(&ts2, &ts1, sizeof(ts1)); 

    ts1.c_lflag &= ~(ICANON | IEXTEN | ECHO);
    ts1.c_lflag |= ISIG;
    ts1.c_cc[VMIN] = 1;
    ts1.c_cc[VTIME] = 0;
    ts1.c_cc[VEOF] = 0; /* CTRL_D seems not to work */

    tcsetattr(0, TCSANOW, &ts1); 

    putstr_now("> ");
    res = parse_input();

    tcsetattr(0, TCSANOW, &ts2); 
    return res;
}

char lgetc(char **line)
{
    char c = **line;
    if (c != '\0')
        (*line)++;
    return c;
}

void lungetc(char **line, char *l_base, char c)
{
    if (*line - l_base > 0) {
        (*line)--;
        **line = c;
    }
}
