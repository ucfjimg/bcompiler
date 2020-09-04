#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bif.h"
#include "b.h"

static int err = 0;
static char *srcfname;
static char *outfname;
static FILE *fin;
static FILE *fout;

static char *mksname(const char *srcname);
static void wrheader(void);
static void wrdata(void);
static void wrcode(void);
static void wrstrp(void);
static void wrname(const char *name);
static unsigned rdbytes(int bytes);
static void rdname(char *name);

#define RDBYTE() rdbytes(1)
#define RDINT() rdbytes(INTSIZE)

int
main(int argc, char **argv)
{
    int outfail;

    if (argc != 2) {
        fprintf(stderr, "ba: ifilename\n");
        return 1;
    }

    srcfname = argv[1];
    outfname = mksname(srcfname);

    if ((fin = fopen(srcfname, "rb")) == NULL) {
        perror(srcfname);
        return 1;
    }

    if ((fout = fopen(outfname, "w")) == NULL) {
        perror(outfname);
        return 1;
    }

    if (RDINT() != BIFMAGIC) {
        fprintf(stderr, "%s: not an intermediate file\n", srcfname);
        err = 1;
    }

    wrheader();
    wrdata();
    wrcode();
    wrstrp();

    if (feof(fin)) {
        fprintf(stderr, "premature end of file on %s\n", srcfname);
        err = 1;
    }

    fclose(fin);

    outfail = ferror(fout);
    if (fclose(fout) == -1 || outfail) {
        fprintf(stderr, "I/O error on %s (disk full?)\n", outfname);
        err = 1;
    }

    if (err) {
        remove(outfname);
        return 1;
    }

    return 0;
}

// turn the input filename into an asm file name
//
char *
mksname(const char *srcname)
{
    char *pdot = strrchr(srcname, '.');
    char *psep = strrchr(srcname, '/');
    int basel = strlen(srcname);
    char *sname;

    if (pdot && (psep == NULL || pdot > psep)) {
        basel = pdot - srcname;
    }

    sname = malloc(basel + 3);
    memcpy(sname, srcname, basel);
    sname[basel++] = '.';
    sname[basel++] = 's';
    sname[basel++] = '\0';

    return sname;
}

// write out data declarations
//
void
wrdata(void)
{
    int i, j;
    int fl, type;
    int ndata = RDINT();
    int ninit;
    char name[MAXNAM];
    char exname[MAXNAM];
    int vecsize;

    if (feof(fin)) {
        return;
    } 
    
    for (i = 0; i < ndata; i++) {
        rdname(name);

        fl = RDBYTE();
        if (fl & BIFVEC) {
            vecsize = RDINT();
        } else {
            wrname(name);
        }

        ninit = RDINT();
        for (j = 0; j < ninit; j++) {
            type = RDBYTE();
            switch (type) {
            case BIFINAM:
                rdname(exname);
                fprintf(fout, "    .int .%s\n", exname);
                break;

            case BIFIINT:
                fprintf(fout, "    .int %u\n", RDINT());
                break;

            case BIFISTR:
                fprintf(fout, "    .int strp + %u\n", RDINT());
                break;
            }
        }

        if (fl & BIFVEC) {
            wrname(name);
            fprintf(fout, "    .int .-4\n");
        }
    }
}

// TODO could share this w/ bc
// 
static struct {
    enum codeop op;
    const char *text;
} simpleops[] = {
    { ONAMDEF,"NAMDEF" },
    { OPOP,   "POP" },
    { OPOPT,  "POPT" },
    { OPUSHT, "PUSHT" },
    { OROT,   "ROT" },
    { ODUP,   "DUP" },
    { ODEREF, "DEREF" },
    { OSTORE, "STORE" },
    { OLEAVE, "LEAVE" },
    { OCALL,  "CALL" },
    { ORET,   "RET" },
    { OADD,   "ADD" },
    { OSUB,   "SUB" },
    { OMUL,   "MUL" },
    { ODIV,   "DIV" },
    { OMOD,   "MOD" },
    { OSHL,   "SHL" },
    { OSHR,   "SHR" },
    { ONEG,   "NEG" },
    { ONOT,   "NOT" },
    { OAND,   "AND" },
    { OOR,    "OR" },
    { OEQ,    "EQ" },
    { ONE,    "NE" },
    { OLT,    "LT" },
    { OLE,    "LE" },
    { OGT,    "GT" },
    { OGE,    "GE" },
};
static int nsimpleops = sizeof(simpleops) / sizeof(simpleops[0]);

// find a simple op (no args)
//
static const char *
simpop(int n)
{
    int i;
    for (i = 0; i < nsimpleops; i++) {
        if (simpleops[i].op == n) {
            return simpleops[i].text;
        }
    }
    return NULL;
}

// Adjust an offset for an automatic variable or arg based on
// the stack frame layout
//   |   arg n   |
//   |  arg n-1  |
//   |    ...    |
//   |   arg 0   | +8
//   |return addr| + 4
//   |  old ebp  | <- ebp
//   | local -1  | -4
//   | local -2  | -8
//   |    etc.   |
//
static unsigned
adjauto(unsigned offs)
{
    int soffs = offs;

    // arg pointers have to skip over saved EBP and return address
    //
    if (soffs >= 0) {
        soffs += 2;
    }

    // scale to target INTSIZE
    //
    soffs *= INTSIZE;

    return soffs;
}

// Write out functions
//
void
wrcode(void)
{
    int ncode = RDINT();
    int i;
    int j, nex, ninst, op, n;
    char fn[MAXNAM + 1], name[MAXNAM + 1];
    const char *opcode;
    char *extrns;

    for (i = 0; i < ncode; i++) {
        rdname(fn);
        wrname(fn);
        
        // TODO don't really need this?
        nex = RDINT();
        extrns = malloc(nex * (MAXNAM + 1));
        if (!extrns) {
            err = 1;
            fprintf(stderr, "out of memory\n");
            return;
        }
        for (j = 0; j < nex; j++) {
            rdname(extrns + j * (MAXNAM + 1));
        }

        ninst = RDINT();
        for (j = 0; j < ninst; j++) {
            op = RDBYTE();
            if ((opcode = simpop(op)) != NULL) {
                fprintf(fout, "    .int %s\n", opcode);
                continue;
            }

            switch (op) {
            case OJMP:
            case OBZ:
                fprintf(fout, "    .int %s, .%s+%u\n", (op == OJMP) ? "JMP" : "BZ", fn, RDINT());
                break;

            case OPOPN:
                fprintf(fout, "    .int POPN, %u\n", INTSIZE * RDINT());
                break;

            case ODUPN:
                fprintf(fout, "    .int DUPN, %u\n", INTSIZE * RDINT());
                break;

            case OENTER:
                fprintf(fout, "    .int ENTER, %u\n", RDINT());
                break;

            case OPSHCON:
                if (RDBYTE()) {
                    // strcon
                    fprintf(fout, "    .int PSHCON, strp + %u\n", RDINT());
                } else {
                    // intcon
                    fprintf(fout, "    .int PSHCON, %u\n", RDINT());
                }
                break;

            case OPSHSYM:
                if (RDBYTE() == 0) {
                    // extrn
                    fprintf(fout, "    .int PSHSYM, .%s\n", extrns + (MAXNAM + 1) * RDINT());
                } else {
                    fprintf(fout, "    .int PSHAUTO, %d\n", RDINT());    
                }
                break;

            default:
                fprintf(stderr, "internal error: intermediate op %d not handled\n", op);
                assert(0);
            }
        }
    }

    free(extrns);
}

// Write the string pool
// 
void
wrstrp()
{
    const int perline = 8;
    int i, j, m;
    int n = RDINT();
    

    if (n == 0) {
        return;
    }

    fprintf(fout, "    .local strp\nstrp:\n");

    for (i = 0; i < n; i += perline) {
        fprintf(fout, "    .byte ");
        m = n - i;
        if (m > perline) {
            m = perline;
        }

        for (j = 0; j < m; j++) {
            fprintf(fout, "0x%02x%c", RDBYTE() & 0xff, (j < m-1) ? ',' : '\n');
        }
    }
}

// write the asm file header
//
void
wrheader(void)
{
    fprintf(fout, "# %s\n", srcfname);
    fprintf(fout, "    .text\n\n");
}

static void
chkinerr(void)
{
    if (feof(fin)) {
        fprintf(stderr, "premature eof or I/O error on %s\n", srcfname);
        fclose(fin);
        fclose(fout);
        remove(outfname);
        exit(1);
    }
}

// write a gloal name
//
void 
wrname(const char *name)
{
    fprintf(
        fout,
            "    .align 4\n"
            "    .global .%s\n"
            ".%s:\n",
        name,
        name);
}

// read an n byte integer
// TODO some things should be unsigned
//
unsigned
rdbytes(int n)
{
    int i, ch;
    unsigned long ul = 0;

    for (i = 0; i < n; i++) {
        ch = fgetc(fin);
        ul |= ((ch & 0xff) << (8*i));
    }

    chkinerr();
    return ul;
}

// read a name
//
void
rdname(char *name)
{
    name[0] = '\0';

    int len = RDBYTE();
    if (len <= MAXNAM) {
        fread(name, 1, len, fin);
        name[len] = '\0';
    }
    chkinerr();
}


