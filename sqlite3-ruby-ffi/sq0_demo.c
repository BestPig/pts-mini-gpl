#define ALONE \
  set -ex; gcc -o "${0%.*}" -W -Wall "$0" -lsqlite3; exit 0
/*
 * sq0_demo.c: Demo for using the SQLite3 C API
 * by pts@fazekas.hu at Thu May 28 17:28:03 CEST 2009
 *
 *
 * run like this:
 *
 *   $ chmod +x sq0.c
 *   $ ./sq0.c
 *   + gcc -o sq0 -W -Wall sq0.c -lsqlite3
 *   $ ./sq0 t.sqlite3 'CREATE TABLE foo (id INT, s TEXT)'
 *   $ ./sq0 t.sqlite3 "INSERT INTO foo VALUES (1, 'hello')"
 *   $ ./sq0 t.sqlite3 "INSERT INTO foo VALUES (2, 'world')"
 *   $ ./sq0 t.sqlite3 "INSERT INTO foo VALUES (42, 'answer')"
 *   $ ./sq0 t.sqlite3 'SELECT * FROM foo'
 *   id = 1
 *   s = hello
 *
 *   id = 2
 *   s = world
 *
 *   id = 42
 *   s = answer
 *
 *   $ _
 */
#include <stdio.h>
#include <sqlite3.h>

int callback(void *dummy, int argc, char **data, char **colnames) {
  (void)dummy;
  int i;
  for(i=0; i<argc; ++i){
    printf("%s = %s\n", colnames[i], data[i] ? data[i] : "NULL");
  }
  printf("\n");
  return 0;
}

int main(int argc, char **argv){
  sqlite3 *db;
  int rc;
  char *errmsg;

  if( argc!=3 ){
    fprintf(stderr, "Usage: %s DATABASE SQL-STATEMENT\n", argv[0]);
    return 1;
  }
  if (SQLITE_OK != (rc = sqlite3_open(argv[1], &db))){
    fprintf(stderr, "Can't open db: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }
  if (SQLITE_OK != (rc =
      sqlite3_exec(db, argv[2], callback, 0, &errmsg))) {
    char *errmsg = NULL;
    fprintf(stderr, "SQL error: %s\n", errmsg);
    sqlite3_free(errmsg);
    sqlite3_close(db);
    return 1;
  }
  sqlite3_close(db);
  return 0;
}
