#ifndef BIF_H_
#define BIF_H_

#define BIFMAGIC 0x4642   /* BF */

#define BIFVEC   0x01     /* data flag - is vector */

#define BIFINAM  0x00     /* initializer element is name */
#define BIFIINT  0x01     /* initializer element is int const */
#define BIFISTR  0x02     /* initializer element is string const */
#define BIFIVEC  0x03     /* initializer element is a vector */

struct stabent;
extern int bifwrite(const char *fn, struct stabent *syms);

#endif

