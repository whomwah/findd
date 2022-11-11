/* findd - show the parent dir of some named dir e.g .git */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <fts.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

bool Flag_Null;                 /* -0 */
bool Flag_Quiet;                /* -q */
bool Flag_Recurse;              /* -r */

void emit_help(void);

int main(int argc, char *argv[])
{
    int ch;
    char **pathlist, **plp, *pof;
    FTS *filetree;
    FTSENT *filedat;
    /* get to the files, limit filesystem calls as much as possible */
    int fts_options = FTS_LOGICAL | FTS_COMFOLLOW | FTS_NOSTAT;

    int ret = EXIT_SUCCESS;

#ifdef __OpenBSD__
    if (pledge("rpath stdio", NULL) == -1)
        err(1, "pledge failed");
#endif

    while ((ch = getopt(argc, argv, "h0qrv")) != -1) {
        switch (ch) {
        case '0':
            Flag_Null = true;
            break;
        case 'q':
            Flag_Quiet = true;
            break;
        case 'r':
            Flag_Recurse = true;
            break;
        case 'v':
            printf("findd: %s\n", VERSION);
            exit(ret);
            break;
        case 'h':
        case '?':
        default:
            emit_help();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc == 0 || **argv == '\0') {
        emit_help();
    }

    pof = *argv;

    if (!(pathlist =
         calloc((size_t) (argc == 1 ? 2 : argc), sizeof(char *))))
        err(EX_OSERR, "calloc path list failed");
    plp = pathlist;
    if (argc == 1) {
        *plp++ = (char *) ".";
    } else {
        argv++;
        while (*argv) {
            if (!(*plp++ = realpath(*argv, NULL)))
                err(EX_IOERR, "realpath failed on '%s'", *argv);
            argv++;
        }
    }
    *plp = (char *) 0;

    if (!(filetree = fts_open(pathlist, fts_options, NULL)))
        err(EX_OSERR, "could not fts_open()");

    while ((filedat = fts_read(filetree)) != NULL) {
        char *filep;
        struct stat statbuf;
        switch (filedat->fts_info) {
        case FTS_F:
            break;
        case FTS_D:
            if (asprintf(&filep, "%s/%s", filedat->fts_accpath, pof) == -1)
                err(EX_OSERR, "could not asprintf() file path");

            if (stat(filep, &statbuf) == 0) {
                printf("%s%c", filedat->fts_path, Flag_Null ? '\0' : '\n');
                if (!Flag_Recurse)
                    fts_set(filetree, filedat, FTS_SKIP);
            } else {
                if (!Flag_Quiet && errno != ENOENT) {
                    warn("could not stat %s", filep);
                    ret = EX_IOERR;
                }
            }
            free(filep);
            break;
        case FTS_DNR:
            ret = EX_NOPERM;
            if (!Flag_Quiet)
                warnx("failed to open %s directory: %s", filedat->fts_path,
                      strerror(filedat->fts_errno));
            break;
        case FTS_DC:
            if (!Flag_Quiet)
                warnx("filesystem cycle from '%s' to '%s'\n",
                      filedat->fts_accpath, filedat->fts_cycle->fts_accpath);
            break;
        case FTS_ERR:
            err(1, "fts_read failed??");
        default:
            ;
        }
    }
    if (errno != 0)
        err(EX_OSERR, "fts_read() error");

    exit(ret);
}

void emit_help(void)
{
    fputs("Usage: findd [-0] [-r] [-q] filename [dir ..]\n", stderr);
    fputs("   [-q] Query string to search for e.g .git\n", stderr);
    fputs("   [-r] Recursive (Much slower)\n", stderr);
    fputs("   [-0] Join output into a single string\n", stderr);
    fputs("\n", stderr);
    fputs("Examples\n", stderr);
    fputs("   findd -q .git $HOME\n", stderr);
    fputs("   findd -q .git $HOME/foo $HOME/bar baz\n", stderr);
    exit(EX_USAGE);
}
