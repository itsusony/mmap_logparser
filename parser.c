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
                    "\n\nusage:\n\n\t./parser -f LOGFILE_PATH -c COLUMNS -s SEP -a AND_cond -n NOT_cond -l LIKE_cond"
                    "\n\n\tCOLUMNS: col1,col2,col3"
                    "\n\n\tSEP: , (default is TAB), maxlen need be 1"
                    "\n\n\tAND_cond: ip=8.8.8.8 (will list rows by ip=8.8.8.8), use empty like this: ip="
                    "\n\n\tNOT_cond: ip=1.1.1.1 (will list rows by ip!=1.1.1.1), use empty like this: ip="
                    "\n\n\tLIKE_cond: ua=ios    (will list rows by ua contains ios)"
                    "\n\n"
    );
    exit(1);
}

char sep[2] = "\t";
typedef struct _KeyVal {
    char* key;
    char* val;
} KeyVal;

KeyVal **and_conds;
int and_cond_len = 0;
KeyVal **not_conds;
int not_cond_len = 0;
KeyVal **like_conds;
int like_cond_len = 0;

int and_check(KeyVal** kvs, int kvs_len) {
    if (!and_cond_len) return 1;
    int j,k, rst;
    for (k=0;k<kvs_len;k++) {
        KeyVal* col = kvs[k];
        if (col->key == NULL) continue;

        for (j=0;j<and_cond_len;j++) {
            KeyVal *kv = and_conds[j];
            if (strcmp(kv->key, col->key) != 0) continue;

            if (kv->val == NULL) {
                rst = (col->val == NULL || strlen(col->val)==0) ? 1 : 0;
            } else {
                rst = (col->val && strstr(col->val, kv->val)) ? 1 : 0;
            }
            if (!rst) return 0;
        }
    }
    return 1;
}
int not_check(KeyVal** kvs, int kvs_len) {
    if (!not_cond_len) return 1;
    int j,k, rst;
    for (k=0;k<kvs_len;k++) {
        KeyVal* col = kvs[k];
        if (col->key == NULL) continue;

        for (j=0;j<not_cond_len;j++) {
            KeyVal *kv = not_conds[j];
            if (strcmp(kv->key, col->key) != 0) continue;

            if (kv->val == NULL) {
                rst = (col->val != NULL && strlen(col->val)!=0) ? 1 : 0;
            } else {
                rst = (col->val && !strstr(col->val, kv->val)) ? 1 : 0;
            }
            if (!rst) return 0;
        }
    }

    for (j=0;j<not_cond_len;j++) {
        KeyVal *kv = not_conds[j];
        if (kv->val == NULL) {
            int exist = 0; // if not exist, return 0
            for (k=0;k<kvs_len;k++) {
                KeyVal* col = kvs[k];
                if (col->key == NULL) continue;
                if (strcmp(kv->key, col->key) == 0) {
                    exist=1;
                    break;
                }
            }
            if (!exist) return 0;
        }
    }

    return 1;
}
int like_check(KeyVal** kvs, int kvs_len) {
    if (!like_cond_len) return 1;
    int j,k, rst;
    for (k=0;k<kvs_len;k++) {
        KeyVal* col = kvs[k];
        if (col->key == NULL) continue;

        for (j=0;j<like_cond_len;j++) {
            KeyVal *kv = like_conds[j];
            if (strcmp(kv->key, col->key) != 0) continue;

            rst = (col->val && (strstr(col->val, kv->val))) ? 1 : 0;
            if (rst) return 1;
        }
    }
    return 0;
}

void parse_cond(const char* arg, const char* split, KeyVal*** conds, int* conds_len) {
    char* tmp_and = strdup(arg);
    char* ptr = strtok(tmp_and, split);
    if (ptr) {
        KeyVal* ret = calloc(1,sizeof(KeyVal));
        ret->key = strdup(ptr);

        ptr = strtok(NULL, split);
        ret->val = ptr ? strdup(ptr) : NULL;

        (*conds)[(*conds_len)++] = ret;
    }
    free(tmp_and);
}

int main(int argc, char *argv[]) {
    and_conds  = calloc(128,sizeof(KeyVal*));
    not_conds  = calloc(128,sizeof(KeyVal*));
    like_conds = calloc(128,sizeof(KeyVal*));

    char *file_path = NULL, *columns = NULL;
    int c;
    while ((c=getopt(argc, argv, "f:c:s:a:n:l:")) != -1) {
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
                parse_cond(optarg, "=", &and_conds, &and_cond_len);
                break;
            case 'n':
                parse_cond(optarg, "=", &not_conds, &not_cond_len);
                break;
            case 'l':
                parse_cond(optarg, "=", &like_conds, &like_cond_len);
                break;
        }
    }

    if (!file_path || !columns || strlen(sep)!=1) usage();

    char** arr_cols = calloc(MAX_COLS_LEN+1, sizeof(char*)); // no need to free. will be gc by exit
    int arr_collen = 0;
    char* tp = strtok(columns, ",");
    while (tp != NULL && arr_collen < MAX_COLS_LEN) {
        if (strlen(tp)) {
            arr_cols[arr_collen++] = strdup(tp);
        }
        tp = strtok(NULL, ",");
    }

    struct stat sb;
    if (stat(file_path, &sb) == 0) {
        int fd = open(file_path, O_RDONLY);

        int page_size = getpagesize();
        int map_size = (sb.st_size / page_size + 1) * page_size;

        char *mem = NULL;
        if ((mem = mmap(NULL, map_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
            fprintf(stderr, "mmap open error!\n");
            exit(1);
        } else { // mmap ok
            char* p1 = mem;
            char* p2 = p1 ? strchr(p1, '\n') : NULL;
            unsigned long plen = p2 ? p2 - p1 : 0;

            while (p1 && plen) {
                char* ptmp = calloc(plen+1, sizeof(char));
                strncpy(ptmp, p1, plen);

                KeyVal **kvs = calloc(arr_collen, sizeof(KeyVal*));
                int kvs_len = 0, i;
                for (i=0;i<arr_collen;i++) {
                    char* _col = arr_cols[i];

                    char* ptmpcol = calloc(strlen(_col)+32, sizeof(char));
                    sprintf(ptmpcol,"\"%s\":", _col);

                    char* pctmp = strstr(ptmp, ptmpcol);
                    if (pctmp) {
                        char* pctmp2 = pctmp + strlen(ptmpcol);
                        char* pctmp_end = pctmp2[0] == '"' ? (strstr(pctmp2+1,"\"")+1) : strstr(pctmp2,",");
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
                                KeyVal* kv = calloc(1,sizeof(KeyVal*));
                                kv->key = strdup(_col);
                                kv->val = val;
                                kvs[kvs_len++] = kv;
                            }
                        } else {
                            fprintf(stderr, "error: json parse error: %s\n", ptmp);
                            goto col_is_empty;
                        }
                    } else {
                        col_is_empty:;
                        KeyVal* kv = calloc(1,sizeof(KeyVal*));
                        kv->key = NULL;
                        kv->val = NULL;
                        kvs[kvs_len++] = kv;
                    }
                    free(ptmpcol);
                }

                if (and_check(kvs,kvs_len) && not_check(kvs,kvs_len) && like_check(kvs,kvs_len)) {
                    for (i=0;i<kvs_len;i++) {
                        const KeyVal* kv = kvs[i];
                        fprintf(stdout,"%s%s",(kv->val==NULL ? "" : kv->val), ((i!=arr_collen-1)?sep:"\n"));
                    }
                }

                for (i=0;i<kvs_len;i++) {
                    KeyVal* kv = kvs[i];
                    free(kv->key);
                    free(kv->val);
                    free(kv);
                }
                free(kvs);
                free(ptmp);

                p1 += plen+1;
                p2 = p1 ? strchr(p1, '\n') : NULL;
                plen = p2 ? p2 - p1 : 0;
            }
        }
    } else {
        fprintf(stderr, "file open error!\n");
        exit(1);
    }

    return 0;
}
