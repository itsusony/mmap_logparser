# mmap_logparser
less memory json-log parser by using mmap

# usage
```
bin/parser -f LOGFILE_PATH -c COLUMNS -s SEP -a AND_cond -n NOT_cond -l LIKE_cond

COLUMNS: col1,col2,col3
SEP: , (default is TAB), maxlen need be 1
AND_cond: ip=8.8.8.8 (will list rows by ip=8.8.8.8), use empty like this: ip=
NOT_cond: ip=1.1.1.1 (will list rows by ip!=1.1.1.1), use empty like this: ip=
LIKE_cond: ua=ios    (will list rows by ua contains ios)
```
