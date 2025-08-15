/*********************************************************************/
/*
 *
 */
/*********************************************************************/

#ifndef SBFMT_H
#define SBFMT_H

/*********************************************************************/

struct sbfmt_buffer
{
   char			*buf;
   char			*p;
   char			*end;
   int			flags;
};

#define SBFMT_BUFFER(name, b, f)	\
   struct sbfmt_buffer name = {		\
      .buf = (b),			\
      .p = (b),				\
      .end = (b) + sizeof(b),		\
      .flags = (f),			\
   }

inline const char * sbfmt_buf(const struct sbfmt_buffer *sb) {
   return sb->buf;
}

inline int sbfmt_len(const struct sbfmt_buffer *sb) {
   return sb->p - sb->buf;
}

void sbfmt_putc(struct sbfmt_buffer *sb, char c);
void sbfmt_puts(struct sbfmt_buffer *sb, const char *s);
void sbfmt_int(struct sbfmt_buffer *sb,
	       int value, int width, char pad);
void sbfmt_hex(struct sbfmt_buffer *sb,
	       int value, int nbits);

/*********************************************************************/
#endif
