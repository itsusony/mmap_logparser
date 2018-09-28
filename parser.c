#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_COLS_LEN 1024

void usage() {
    fprintf(stderr, "this tool can convert jsv (json csv) to tsv"
                    "\n\nusage:\n\n\t./parser -f LOGFILE_PATH -c COLUMNS -s SEP"
                    "\n\n\tCOLUMNS: col1,col2,col3"
                    "\n\n\tSEP: , (default is TAB), maxlen need be 1"
                    "\n\n"
    );
    exit(1);
}

int main(int argc, char *argv[]) {
    char *mem;
    struct stat sb;
    int fd;
    long page_size, map_size;

    int c,i;
    char* file_path = NULL;
    char* columns = NULL;

    char** arr_cols;
    int arr_collen;
    char* tp;
    char sep[2] = "\t";

    char *p1, *p2, *ptmp, *_col, *ptmpcol, *pctmp, *pctmp2, *pctmp_end;
    unsigned long plen;

    while ((c=getopt(argc, argv, "f:c:s:")) != -1) {
        switch(c) {
            case 'f':
                file_path = strdup(optarg);
                break;
            case 'c':
                columns = strdup(optarg);
                break;
            case 's':
                strncpy(sep, optarg, 1);
                break;
        }
    }

    if (!file_path || !columns || strlen(sep)!=1) usage();

    arr_cols = calloc(MAX_COLS_LEN+1, sizeof(char*)); // no need to free. will be gc by exit
    arr_collen = 0;
    tp = strtok(columns, ",");
    while (tp != NULL && arr_collen < MAX_COLS_LEN) {
        arr_cols[arr_collen++] = strdup(tp);
        tp = strtok(NULL, ",");
    }

    if (stat(file_path, &sb) == 0) {
        fd = open(file_path, O_RDONLY);

        page_size = getpagesize();
        map_size = (sb.st_size / page_size + 1) * page_size;

        if ((mem = mmap(NULL, map_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
            fprintf(stderr, "mmap open error!\n");
            exit(1);
        } else { // mmap ok
            p1 = mem;
            p2 = p1 ? strchr(p1, '\n') : NULL;
            plen = p2 ? p2 - p1 : 0;

            while (p1 && plen) {
                ptmp = calloc(plen+1, sizeof(char));
                strncpy(ptmp, p1, plen);

                for (i=0;i<arr_collen;i++) {
                    _col = arr_cols[i];

                    ptmpcol = calloc(strlen(_col)+3+1, sizeof(char));
                    sprintf(ptmpcol,"\"%s\":", _col);

                    pctmp = strstr(ptmp, ptmpcol);
                    if (pctmp) {
                        pctmp2 = pctmp + strlen(ptmpcol);
                        pctmp_end = strstr(pctmp2,",");
                        if (!pctmp_end) pctmp_end = strstr(pctmp2,"}");
                        if (pctmp_end) {
                            int _len = pctmp_end - pctmp2;
                            char* val = _len ? calloc(_len+1,sizeof(char)) : NULL;
                            if (val) {
                                strncat(val, pctmp2, _len);
                                if (strchr(val, '"') != NULL) {
                                    char* val2 = strdup(val+1);
                                    free(val);
                                    val2[_len-2] = '\0';
                                    val = val2;
                                }
                                fprintf(stdout,"%s",val);
                                free(val);
                            }
                        } else {
                            fprintf(stderr, "json parse error: %s\n", ptmp);
                            free(ptmpcol);
                            break; // json error, goto next row
                        }
                    }
                    free(ptmpcol);
                    fprintf(stdout,"%s",(i!=arr_collen-1)?sep:"\n");
                }

                free(ptmp);

                p1 += plen+1;
                p2 = p1 ? strchr(p1, '\n') : NULL;
                plen = p2 ? p2 - p1 : 0;
            }
        }
    } else {
        fprintf(stderr, "file size error!\n");
        exit(1);
    }

    return 0;
}
