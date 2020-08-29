#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME 8
#define MAX_STR 256 // max length of string constant
#define STREOF (-1)

#define INTCONST (-1)

enum storclas {
    NEW,
    EXTERN,
    AUTO,
    INTERNAL,
};

struct codenode;

struct stabent {
    struct stabent *next;           // next in chain
    char name[MAX_NAME + 1];        // symbol name
    enum storclas sc;               // storage class
    struct codenode *exdef;         // if extrn and we have a definition
            

    // if AUTO, offset into stack
    // if INTERNAL, offset into code
    int offset;
    // if string constant, string value
    char *strcon;
    // if not string, how many words?
    unsigned size;
};

struct constant {
    int strlen;
    union {
        char *strcon;
        unsigned intcon;
    } v;
};

enum codeop {
    TNAMDEF,
    TIVAL,
    TZERO,
};

struct codenode {
    struct codenode *next;
    enum codeop op;
    union {
        char name[MAX_NAME+1];
        struct constant ival;
        unsigned rept;
    } arg;
};

struct codefrag {
    struct codenode *head, *tail;
};

static char *srcfn;
static FILE *fp;
static int line = 1;
static int errf = 0;
static int currch = '\n';
static struct stabent *global;

static void program(struct codefrag *prog);
static void definition(struct codefrag *prog);
static void datadef(struct codefrag *prog);
static void constant(struct constant *con);
static void charcon(struct constant *con);
static void strcon(struct constant *con);
static int escape(int ch);
static char *name(void);

static struct stabent *stabget(struct stabent **root, const char *name);

static struct codenode *cnalloc(void);
static void cnpush(struct codefrag *frag, struct codenode *node);
static void cfprint(struct codefrag *frag);

static int advskipws(void);
static int skipws(void);
static void advance(void);
static void advraw(void);
static void err(int line, const char *fmt, ...);

int 
main(int argc, char **argv)
{
    struct codefrag prog = { NULL, NULL };
    
    if (argc != 2) {
        fprintf(stderr, "b: source-file\n");
        return 1;
    }

    if (strcmp(srcfn = argv[1], "-") == 0) {
        srcfn = "<stdin>";
        fp = stdin;
    } else if ((fp = fopen(srcfn, "r")) == NULL) {
        perror(srcfn);
        return 1;
    }
    
    program(&prog);
    cfprint(&prog);
}

void 
program(struct codefrag *prog)
{
    int ch;

    while (!errf) {
        if (skipws() == EOF) {
            return;
        }
        definition(prog);
    }
}

void
definition(struct codefrag *prog)
{
    int ch;
    int here = line;
    struct codenode *cn = cnalloc();
    struct stabent *sym;


    cn->op = TNAMDEF;
    strcpy(cn->arg.name, name());
    cnpush(prog, cn);

    sym = stabget(&global, cn->arg.name);
    if (sym->sc == NEW) {
        sym->sc = EXTERN;
        sym->exdef = cn;
    } else {
        err(here, "'%s' is previously defined", cn->arg.name);
    }

    skipws();
    if (currch == '(') {
        // TODO function def;
        return;
    }

    datadef(prog);
}

void
datadef(struct codefrag *prog)
{
    int here = line;
    struct constant con;
    struct codenode *cn;
    int isvec = 0;
    unsigned size = 0;
    unsigned emitted = 0;
    int ch;

    if (currch == '[') {
        advskipws();
        if (currch != ']') {
            constant(&con);
            if (con.strlen != INTCONST) {
                err(here, "vector size constant must not be string");
                free(con.v.strcon);
            } else {
                size = con.v.intcon;
            }
        }

        if (skipws() != ']') {
            err(here, "']' expected");
        } else {
            advskipws();
        }
        isvec = 1;
    }

    if (currch == ';') {
        if (!isvec) {
            cn = cnalloc();
            cn->op = TIVAL;
            cn->arg.ival.strlen = INTCONST;
            cn->arg.ival.v.intcon = 0;
            cnpush(prog, cn);
            emitted++;
        }
        advskipws();
    } else {
        for(;;) {
            cn = cnalloc();
            cn->op = TIVAL;
            constant(&cn->arg.ival);
            cnpush(prog, cn);
            emitted++;

            skipws();
            if (currch == ';') {
                advskipws();
                break;
            } else if (currch == ',') {
                advskipws();
                continue;
            }

            err(here, "';' or ',' expected");
            break; 
        }
    }

    if (size > emitted) {
        cn = cnalloc();
        cn->op = TZERO;
        cn->arg.rept = size - emitted;
        cnpush(prog, cn);
    }
}

// parse a constant
//
void 
constant(struct constant *con)
{
    int ch;
    int base;

    con->strlen = INTCONST;
    con->v.intcon = 0;
    
    skipws();

    switch (currch) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        base = (currch == '0') ? 8 : 10;
            
        do {
            con->v.intcon = con->v.intcon * base + currch - '0';
            advraw();
        } while (isdigit(currch));
        break;

    case '\'':
        advraw();
        charcon(con);
        break;

    case '"':
        advraw();
        strcon(con);
        break;

    default:
        err(line, "constant expected");
        break;
    }
}

// parse a character constant
//
void 
charcon(struct constant *con)
{
    int i, ch;
    
    con->strlen = INTCONST;
    con->v.intcon = 0;

    skipws();

    for (i = 0; i < 2; i++, advraw()) {
        if (currch == EOF) {
            err(line, "unterminated char constant");
            break;
        }

        if (currch == '\'') {
            if (i == 0) {
                err(line, "empty char constant");
            }
            break;
        }

        if (currch == '*') {
            advraw();
            currch = escape(currch);
        }

        con->v.intcon = (con->v.intcon << 8) | (currch & 0xff);
    }

    if (i == 2 && currch != '\'') {
        err(line, "unterminated char constant");
        return;
    }

    advskipws();
}

// parse a string constant
//
void 
strcon(struct constant *con)
{
    static char buf[MAX_STR];
    int idx = 0;
    int here = line;

    for(;; advraw()) {
        if (currch == EOF) {
            err(here, "unterminated string constant");
            break;
        }

        if (currch == '"') {
            if (idx < MAX_STR) {
                buf[idx] = STREOF;
            }
            idx++;
            advskipws();
            break;
        }

        if (currch == '*') {
            advraw();
            currch = escape(currch);
        }

        if (idx < MAX_STR) {
            buf[idx] = currch;
        }
        idx++;
    }

    if (idx > MAX_STR) {
        err(here, "string constant too long (max %d).", MAX_STR);
    }

    con->strlen = idx;
    con->v.strcon = malloc(idx);
    memcpy(con->v.strcon, buf, idx);
}

// translate an escape sequence into the character it
// represents.
//
int 
escape(int ch)
{
    switch (ch) {
    case '0': return 0;
    case 'e': return STREOF;
    case '(': return '{';
    case ')': return '}';
    case 't': return '\t';
    case 'n': return '\n';
    case '*': case '=': case '\'':
        return ch;

    default:
        err(line, "invalid escape '%c'", ch);
        break;
    }

    return ch;
}

// parse and return a name. the pointer is to a static
// buffer which will be overwritten on the next call 
// to name()
//
char *
name(void)
{
    static char id[MAX_NAME + 1];
    int i = 0;
    int here = line;

    skipws();

    if (!isalpha(currch) && currch != '_') {
        err(here, "name expected");
    } else {
        do {
            if (i < MAX_NAME) {
                id[i] = currch;
            }
            i++;
            advance();
        } while (isalnum(currch) || currch == '_');
    }

    id[i <= MAX_NAME ? i : MAX_NAME] = 0;
    if (i > MAX_NAME) {
        err(here, "name too long");
    }

    return id;
}

/******************************************************************************
 *
 * Symbol table routines
 *
 */

// get or create a symbol of the given name
//
struct stabent *
stabget(struct stabent **root, const char *name)
{
    struct stabent *p;

    for (p = *root; p; p = p->next) {
        if (strcmp(p->name, name) == 0) {
            return p;
        }
    }

    p = calloc(1, sizeof(struct stabent));
    strcpy(p->name, name);
    p->sc = NEW;
    p->next = *root;
    *root = p;

    return p;
}

/******************************************************************************
 *
 * Code fragment routines
 *
 */

// Allocate a new code node
//
struct codenode *
cnalloc(void)
{
    struct codenode *cn = (struct codenode *)calloc(1, sizeof(struct codenode));
    if (cn == NULL) {
        fprintf(stderr, "\n\nFATAL: out of memory\n");
        exit(1);
    }
    return cn;
}

// Push a code node onto the end of a code fragment
//
void 
cnpush(struct codefrag *frag, struct codenode *node)
{
    node->next = NULL;
    if (frag->head == NULL) {
        frag->head = frag->tail = node;
    } else {
        frag->tail->next = node;
        frag->tail = node;
    }
}

// Print a code fragment to stdout
//
void 
cfprint(struct codefrag *frag) 
{
    static const char spaces[] = "          ";
    struct codenode *n;

    const int XPERLINE = 16;
    int i, j, l;
    char ch;

    for (n = frag->head; n; n = n->next) {
        switch (n->op) {
        case TNAMDEF:
            printf("%s:\n", n->arg.name);
            break;

        case TIVAL:
            if (n->arg.ival.strlen == INTCONST) {
                printf("%sIVAL %u\n", spaces, n->arg.ival.v.intcon);
            } else {
                printf("%sIVAL strcon\n", spaces);
                for (i = 0; i < n->arg.ival.strlen; i += XPERLINE) {
                    printf("%s", spaces);

                    l = n->arg.ival.strlen - i;
                    l = l > XPERLINE ? XPERLINE : l;

                    for (j = 0; j < l; j++) {
                        printf("%02x ", n->arg.ival.v.strcon[i+j] & 0xff);
                    }

                    for (; j < XPERLINE; j++) {
                        printf("   ");
                    }

                    printf(" |");
                    for (j = 0; j < l; j++) {
                        ch = n->arg.ival.v.strcon[i+j];
                        putchar(isprint(ch) ? ch : '.'); 
                    }

                    for (; j < XPERLINE; j++) {
                        putchar(' ');
                    }
                    printf("|\n");
                }
            }
            break;

        case TZERO:
            printf("%sZERO %d\n", spaces, n->arg.rept);
            break;
        }
    }

}

/******************************************************************************
 *
 * Low level lexical routines
 *
 */

static int get(void);
static void unget(int ch);

// advance to the next character and consume any whitespace
// after it, landing on the first non-whitespace character
// and returning it
//
int
advskipws(void)
{
    advance();
    return skipws();
}

// skip white space, including comments, and return the next
// non white space character.
//
int 
skipws(void)
{
    while (isspace(currch)) {
        advance();
    }
    return currch;
}

// Advance to the next character, replacing comments w/ a single space.
//
void 
advance()
{
    int here = line;
    int peekch;
    int star;

    if ((currch = get()) != '/') {
        return;
    }

    if ((peekch = get()) != '*') {
        unget(peekch);
        return;
    }
    
    for(star = 0;;) {
        if ((currch = get()) == EOF) {
            err(here, "unterminated comment.");
            break;
        }

        if (star && currch == '/') {
            currch = ' ';
            break;
        }

        star = currch == '*';
    }
}

// Advance to the next character, ignoring comments
//
void
advraw()
{
    currch = get();
}

// get a raw character, counting lines
//
int 
get(void)
{
    int ch = fgetc(fp);
    if (ch == '\n') {
        line++;
    }
    return ch;
}

// put back a raw character, counting lines
//
void 
unget(int ch)
{
    if (ch == '\n') {
        line--;
    }
    ungetc(ch, fp);
}

// report an error, and set a flag that the compilation
// has failed.
//
void 
err(int l, const char *fmt, ...)
{
    fprintf(stderr, "%s:%d: ", srcfn, l);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    errf = 1;
}
