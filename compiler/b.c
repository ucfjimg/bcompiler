#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *ofname = NULL;
static int runld = 1;

struct linkobj {
    struct linkobj *next;
    char *name;
    int remove;
};

static const char *rtlib = "lib/libbrt.a";
static const char *linkscript = "share/b/blink";

static char *sysroot;
static struct linkobj *linkhead, *linktail;
static const char *bccmd;
static const char *bacmd;
static const char *ascmd;
static const char *ldcmd;
static int debug = 0;
static int listing = 0;
static int packrat = 0;   // don't delete any intermediate files
static int verbose = 0;

static void usage(void);
static int compile(const char *fn, const char *outf);
static int ld(const char *outf);
static const char *ext(const char *fn);
static int isext(const char *fn, const char *ex);
static const char *stripdir(const char *fn);
static char * dirname(const char *fn);
static char *replext(const char *fn, const char *newext);
static void addobj(const char *fn, int remove);
static char *pathlcat(const char *left, int llen, const char *right);
static char *rundir(const char *prog);
static char *findfile(const char *path, const char *srch);
static char *fqcommand(const char *path, const char *cmd);
static char *aprintf(const char *fmt, ...);
static void veprintf(const char *fmt, ...);
static void *safemalloc(size_t n);
static char *safestrdup(const char *fn);

// find the toolchain parts, then run all the needed compiler, assembler
// and linker commands to build a b program
//
int 
main(int argc, char **argv)
{
    int ch, i, rc, files = 0, bfiles = 0;
    char *fn, *out;
    struct linkobj *lobj;

    sysroot = findfile(rundir(argv[0]), rtlib);

    bccmd = fqcommand(sysroot, "bin/bc");
    bacmd = fqcommand(sysroot, "bin/ba");
    ascmd = fqcommand(rundir("as"), "as");
    ldcmd = fqcommand(rundir("ld"), "ld");

    while ((ch = getopt(argc, argv, "cglo:ps:v")) != -1) {
        switch (ch) {
        case 'c':
            runld = 0;
            break;

        case 'g':
            debug = 1;
            break;

        case 'l':
            listing = 1;
            break;

        case 'o':
            ofname = optarg;
            break;

        case 'p':
            packrat = 1;
            break;

        case 's':
            sysroot = optarg;
            break;

        case 'v':
            verbose = 1;
            break;

        default:
            usage();
            break;
        }
    }

    veprintf("sysroot=%s\n", sysroot);
    veprintf("bc=%s\n", bccmd);
    veprintf("ba=%s\n", bacmd);
    veprintf("as=%s\n", ascmd);
    veprintf("ld=%s\n", ldcmd);

    if (optind >= argc) {
        fprintf(stderr, "b: no input files\n");
        return 1;
    }

    if (runld && !sysroot) {
        fprintf(stderr, "b: cannot find libbrt.a. specify the sysroot with -s\n");
        return 1;
    }

    files = argc - optind;

    for (i = optind; i < argc; i++) {
        fn = argv[i];
        if (!(isext(fn, "b") || isext(fn, "o") || isext(fn, "a"))) {
            fprintf(stderr, "b: don't know how to compile %s\n", fn);
            return 1;
        }

        if (isext(fn, "b")) {
            bfiles++;
        } else if (!runld) {
            fprintf(stderr, "b: with -c, linker input %s will be ignored\n", fn);
        }
    } 

    if (!runld && ofname && bfiles > 1) {
        fprintf(stderr, "b: cannot specify multiple source files with -c and -o\n");
        return 1;
    }

    for (i = optind, rc = 0; i < argc; i++) {
        fn = argv[i];
        if (isext(fn, "b")) {
            if (runld || !ofname) {
                out = replext(stripdir(fn), "o");
            } else {
                out = ofname;
            } 
                  
            if (!compile(fn, out)) {
                rc = 1;
            }          

            addobj(out, 1);
            if (out != ofname) {
                free(out);
            }
        } else {
            addobj(fn, 0);
        }
    }

    addobj(pathlcat(sysroot, -1, rtlib), 0);
    
    if (rc) {
        return rc;
    }

    if (runld) {
        ld(ofname ? ofname : "a.out");

        if (!packrat) {
            for (lobj = linkhead; lobj; lobj = lobj->next) {
                if (lobj->remove) {
                    veprintf("removing %s\n", lobj->name);
                    remove(lobj->name);
                }
            }
        }
    }

    return 0;
}

// print usage and exit
//
void 
usage(void)
{
    fprintf(stderr, "b: -c -v -g -s sysroot -o outfile srcfile [srcfile ...]\n");    
    exit(1);
}

// compile the given b file
//
int
compile(const char *fn, const char *out)
{
    int l, rc;
    char *ifile = replext(out, "i");
    char *sfile = replext(out, "s");
    char *lstfile = NULL;
    char *lstflag = NULL;
    char *cmd;


    cmd = aprintf("%s -o %s %s", bccmd, ifile, fn);
    veprintf("%s\n", cmd);
    rc = system(cmd);
    free(cmd);
    
    if (rc) {
        return 0;
    }

    cmd = aprintf("%s -o %s %s", bacmd, sfile, ifile);
    veprintf("%s\n", cmd);
    rc = system(cmd);
    free(cmd);

    if (!packrat) {
        veprintf("removing %s\n", ifile);
        remove(ifile);
    }
    
    if (rc) {
        return 0;
    }

    if (listing) {
        lstfile = replext(out, "lst");
        lstflag = aprintf("-aghlms=%s ", lstfile);
        free(lstfile);
    }

    cmd = aprintf("%s %s%s-o %s -march=i386 --32 %s", 
        ascmd, 
        debug ? "-g " : "",
        lstflag ? lstflag : "",
        out, 
        sfile);
    veprintf("%s\n", cmd);
    rc = system(cmd);
    free(cmd);

    if (!packrat) {
        veprintf("removing %s\n", sfile);
        remove(sfile);
    }

    free(ifile);
    free(sfile);
    free(lstflag);
    
    return rc == 0; 
}

// run the linker
//
int
ld(const char *out)
{
    int l = 0, sl, rc;
    struct linkobj *obj;
    char *cmd;
    char *names;
    char *mfile = NULL;
    char *mflag = NULL;
    char *blink = NULL;
    
    if (linkhead == NULL) {
        fprintf(stderr, "b: nothing to link\n");
        return 1;
    }

    for (obj = linkhead; obj; obj = obj->next) {
        l += strlen(obj->name) + 1;
    }

    names = safemalloc(l);


    for (obj = linkhead, l = 0; obj; obj = obj->next) {
        sl = strlen(obj->name);
        memcpy(names + l, obj->name, sl);
        l += sl;
        names[l++] = obj->next ? ' ' : '\0';
    }

    if (listing) {
        mfile = replext(out, "map");
        mflag = aprintf("-Map %s ", mfile);
        free(mfile);
    }

    blink = pathlcat(sysroot, -1, linkscript);

#if 1
    cmd = aprintf("%s %s-m elf_i386 -o %s -T %s %s", 
            ldcmd, 
            mflag ? mflag : "",
            out, 
            blink,
            names);
#else
    cmd = aprintf("%s %s-m elf_i386 -o %s %s", 
            ldcmd, 
            mflag ? mflag : "",
            out, 
            names);
#endif
    veprintf("%s\n", cmd);

    rc = system(cmd);
    free(names);
    free(cmd);
    free(mflag);
    free(blink);

    return rc == 0;
}

// Return the file extension of fn, which is always a 
// pointer into fn.
//
const char *
ext(const char *fn)
{
    char *dot = strrchr(fn, '.');
    char *sep = strrchr(fn, '/');

    if (dot && (sep == NULL || dot > sep)) {
        return dot + 1;
    }
    return fn + strlen(fn);
}

// Return non-zero if fn has file extension ex.
//
static int 
isext(const char *fn, const char *ex)
{
    return strcmp(ext(fn), ex) == 0;
}

// Given a filename, return a pointer to the base name
// part.
//
const char *
stripdir(const char *fn)
{
    char *sep = strrchr(fn, '/');
    return sep ? sep + 1 : fn;
}

// Given a filename, return an allocated string containing
// just the path part. The string will contain a trailing
// path separator.
//
char *
dirname(const char *fn)
{
    char *ret;

    char *sep = strrchr(fn, '/');
    if (sep) {
        int l = sep + 1 - fn;
        ret = safemalloc(l + 1);
        memcpy(ret, fn, l);
        ret[l] = '\0';
    } else {
        ret = safestrdup("./");
    }

    return ret;
}

// Given a filename, return the same filename with
// the extension replaced by 'newext'. The returned
// string is allocated and must be free'd.
//
char *
replext(const char *fn, const char *newext)
{
    const char *ex = ext(fn);
    int l = ex - fn;
    int needdot = l == 0 || fn[l-1] != '.';
    int toalloc = l + needdot + strlen(newext) + 1;
    char *ret = safemalloc(toalloc);

    sprintf(ret, "%.*s%s%s", l, fn, needdot ? "." : "", newext);

    return ret; 
}

// Add an object to the list of files to link
//
void 
addobj(const char *fn, int remove)
{
    struct linkobj *obj = safemalloc(sizeof(struct linkobj));

    obj->name = safestrdup(fn);
    obj->remove = remove;
    obj->next = NULL;

    if (linkhead == NULL) {
        linkhead = obj;
    } else {
        linktail->next = obj;
    }
    linktail = obj;
}

// concatenate the two path, inserting a path sep 
// as needed. the returned string is allocated.
//
// if llen is not -1, it specifies how much of left
// to use in the path.
//
char *
pathlcat(const char *left, int llen, const char *right)
{
    char *cat;
    int lleft = strlen(left), lright = strlen(right);
    if (llen < 0 || llen > lleft) {
        llen = lleft;
    }

    if (!llen) {
        return safestrdup(right);
    } else if (!lright) {
        return safestrdup(left);
    }

    cat = safemalloc(llen + lright + 2);
    sprintf(cat, "%.*s%s%s", llen, left, left[llen - 1] == '/' ? "" : "/", right);

    return cat;
}

// Return the base path used to run this program.
// The returned string has been allocated.
//
char *rundir(const char *prog)
{
    int lprog;
    char *path, *colon, *fqname, *ret;

    if (strchr(prog, '/') && access(prog, X_OK) == 0) {
        return dirname(prog);
    }

    lprog = strlen(prog);

    ret = NULL;
    for (path = getenv("PATH"); !ret && path && *path;) {
        colon = strchr(path, ':');
        if (!colon) {
            colon = path + strlen(path);
        }

        if (colon == path) {
            path++;
            continue;
        }

        fqname = pathlcat(path, colon - path, prog);
        if (access(fqname, X_OK) == 0) {
            ret = dirname(fqname);
        }

        free(fqname);

        if (*colon) {
            path = colon + 1;
        } else {
            path = colon;
        }
    }

    if (!ret) {
        fprintf(stderr, "b: fatal: cannot figure out where sysroot is\n");
        exit(2);
    }

    return ret;
}

// find 'srch' in 'path' and return the path containing it
// by moving upwards in the file system.
//
// 'srch' may contain relative paths. 
//
// returns an allocated string, or NULL if 'srch' was not
// found.
//
char *
findfile(const char *path, const char *srch)
{
    int l = strlen(path);
    char *cat, *dir = NULL;

    while (l > 0) {
        cat = pathlcat(path, l, srch);
        if (access(cat, R_OK) == 0) {
            cat[l] = '\0';
            dir = cat;
            break;
        }

        if (path[l-1] == '/') {
            l--;
        }

        while (l > 0 && path[l-1] != '/') {
            l--;
        }
    }

    return dir;
}

// ensure the given command exists at the given location, and 
// return an allocated string containing its fully qualified
// path.
//
char *
fqcommand(const char *path, const char *cmd)
{
    char *cat = pathlcat(path, -1, cmd);
    if (access(cat, X_OK) == -1) {
        fprintf(stderr, "b: ");
        perror(cat);
        exit(1);
    }
    return cat; 
}

// like sprintf, but returns an allocated string containing
// the result.
//
char *
aprintf(const char *fmt, ...)
{
    char start[256], *p;
    va_list arg;
    size_t wrote;

    va_start(arg, fmt);
    wrote = vsnprintf(start, sizeof(start), fmt, arg);
    va_end(arg);

    if (wrote < sizeof(start)) {
        return safestrdup(start);
    }

    p = safemalloc(wrote + 1);
    va_start(arg, fmt);
    vsnprintf(start, wrote + 1, fmt, arg);
    va_end(arg);

    return p;
}

// printf if the verbose flag is set 
//
void
veprintf(const char *fmt, ...)
{
    va_list arg;

    if (!verbose) {
        return;
    }

    va_start(arg, fmt);
    vprintf(fmt, arg);
    va_end(arg);
}

// Return a checked malloc pointer; exits on 
// out of memory
//
void *
safemalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) {
        fprintf(stderr, "b: out of memory\n");
        exit(1);
    }
    return p;
}

// Return a checked strdup pointer; exits on 
// out of memory
//
char *
safestrdup(const char *fn)
{
    char *p = strdup(fn);
    if (!p) {
        fprintf(stderr, "b: out of memory\n");
        exit(1);
    }
    return p;
}

