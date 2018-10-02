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
                    "\n\nusage:\n\n\t./parser -f LOGFILE_PATH -c COLUMNS -s SEP -a AND_cond -n NOT_cond"
                    "\n\n\tCOLUMNS: col1,col2,col3"
                    "\n\n\tSEP: , (default is TAB), maxlen need be 1"
                    "\n\n\tAND_cond: ip=8.8.8.8 (will list rows by ip=8.8.8.8)"
                    "\n\n\tNOT_cond: ip=1.1.1.1 (will list rows by ip!=1.1.1.1)"
                    "\n\n"
    );
    exit(1);
}

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

char *and_key = NULL, *and_val = NULL, *and_match1 = NULL, *and_match2 = NULL;
char *not_key = NULL, *not_val = NULL, *not_match1 = NULL, *not_match2 = NULL;

void parse_cond(const char* arg, const char* split, char** _key, char** _val, char** _match1, char** _match2) {
    char* tmp_and = strdup(arg);
    char* ptr = strtok(tmp_and, split);
    if (ptr) {
        *_key = strdup(ptr);
        ptr = strtok(NULL, split);
        if (!ptr) {
            fprintf(stderr, "and condition parameter wrong!\n");
            exit(1);
        }
        *_val = strdup(ptr);
        *_match1 = calloc(strlen(*_key) + strlen(*_val) + 32, sizeof(char));
        *_match2 = calloc(strlen(*_key) + strlen(*_val) + 32, sizeof(char));
        sprintf(*_match1, "\"%s\":\"%s", *_key, *_val);
        sprintf(*_match2, "\"%s\":%s",   *_key, *_val);
    }
    free(tmp_and);
}

int main(int argc, char *argv[]) {
    while ((c=getopt(argc, argv, "f:c:s:a:n:")) != -1) {
        switch(c) {
            case 'f':
                if (!file_path) file_path = strdup(optarg);
                break;
            case 'c':
                if (!columns) columns = strdup(optarg);
                break;
            case 's':
                strncpy(sep, optarg, 1);
                break;
            case 'a':
                if (!and_key && !and_val) {
                    parse_cond(optarg, "=", &and_key, &and_val, &and_match1, &and_match2);
                }
                break;
            case 'n':
                if (!not_key && !not_val) {
                    parse_cond(optarg, "=", &not_key, &not_val, &not_match1, &not_match2);
                }
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

                char **vals = calloc(arr_collen, sizeof(char*));
                int vals_len = 0;
                for (i=0;i<arr_collen;i++) {
                    _col = arr_cols[i];

                    ptmpcol = calloc(strlen(_col)+3+1, sizeof(char));
                    sprintf(ptmpcol,"\"%s\":", _col);

                    pctmp = strstr(ptmp, ptmpcol);
                    if (pctmp) {
                        pctmp2 = pctmp + strlen(ptmpcol);
                        pctmp_end = pctmp2[0] == '"' ? (strstr(pctmp2+1,"\"")+1) : strstr(pctmp2,",");
                        if (!pctmp_end) pctmp_end = strstr(pctmp2,"}");
                        if (pctmp_end) {
                            int _len = pctmp_end - pctmp2;
                            char* val = _len ? calloc(_len+1,sizeof(char)) : NULL;
                            if (val) {
                                strncat(val, pctmp2, _len);
                                if (val[0] == '\"' && val[_len-1] == '\"') {
                                    char* val2 = strdup(val+1);
                                    free(val);
                                    val2[_len-2] = '\0';
                                    val = val2;
                                }
                                vals[vals_len++] = val;
                            }
                        } else {
                            fprintf(stderr, "json parse error: %s\n", ptmp);
                            free(ptmpcol);
                            break; // json error, goto next row
                        }
                    }
                    free(ptmpcol);
                }

                if (vals_len) {
                    if ((!and_key && !and_val) || (strstr(ptmp, and_match1) || strstr(ptmp, and_match2))) {
                        if ((!not_key && !not_val) || (!strstr(ptmp, not_match1) && !strstr(ptmp, not_match2))) {
                            for (i=0;i<vals_len;i++) {
                                fprintf(stdout,"%s%s",vals[i], ((i!=arr_collen-1)?sep:"\n"));
                            }
                        }
                    }
                }

                for (i=0;i<vals_len;i++) {
                    free(vals[i]);
                }
                free(vals);
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
