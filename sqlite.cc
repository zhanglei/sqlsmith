#include <stdexcept>
#include <cassert>
#include "sqlite.hh"
#include <iostream>
using namespace std;

extern "C"  {
#include <sqlite3.h>
}

extern "C" int my_sqlite3_busy_handler(void *, int)
{
  throw std::runtime_error("sqlite3 timeout");
}

extern "C" int callback(void *arg, int argc, char **argv, char **azColName)
{
  (void)arg;
  int i;
  for(i=0; i<argc; i++){
    printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  printf("\n");
  return 0;
}

extern "C" int table_callback(void *arg, int argc, char **argv, char **azColName)
{
  auto tables = (vector<table> *)arg;
  tables->push_back(table(argv[2], "main", true, true));
  return 0;
}

extern "C" int column_callback(void *arg, int argc, char **argv, char **azColName)
{
  table *tab = (table *)arg;
  column c(argv[1], argv[2]);
  tab->columns().push_back(c);
  return 0;
}

schema_sqlite::schema_sqlite(std::string &conninfo)
{
  rc = sqlite3_open_v2(conninfo.c_str(), &db, SQLITE_OPEN_READONLY, 0);
  if (rc) {
    throw std::runtime_error(sqlite3_errmsg(db));
  }

  sqlite3_busy_handler(db, my_sqlite3_busy_handler, 0);
//   q("PRAGMA busy_timeout = 1000;");
  
  cerr << "Loading tables...";

  rc = sqlite3_exec(db, "SELECT * FROM main.sqlite_master where type='table'", table_callback, (void *)&tables, &zErrMsg);
  if (rc!=SQLITE_OK) {
    auto e = std::runtime_error(zErrMsg);
    sqlite3_free(zErrMsg);
    throw e;
  }

  cerr << "done." << endl;

  cerr << "Loading columns and constraints...";

  for (auto t = tables.begin(); t != tables.end(); ++t) {
    string q("pragma table_info(");
    q += t->name;
    q += ");";

    rc = sqlite3_exec(db, q.c_str(), column_callback, (void *)&*t, &zErrMsg);
    if (rc!=SQLITE_OK) {
      auto e = std::runtime_error(zErrMsg);
      sqlite3_free(zErrMsg);
      throw e;
    }
  }

  cerr << "done." << endl;

//   cerr << "Loading operators...";
//   cerr << "done." << endl;

  booltype = sqltype::get("int");
  inttype = sqltype::get("int");

  internaltype = sqltype::get("internal");
  arraytype = sqltype::get("ARRAY");

//   cerr << "Loading routines...";
//   cerr << "done." << endl;

//   cerr << "Loading routine parameters...";
//   cerr << "done." << endl;

//   cerr << "Loading aggregates...";
//   cerr << "done." << endl;

//   cerr << "Loading aggregate parameters...";
//   cerr << "done." << endl;

  cerr << "Generating indexes...";
  generate_indexes();
  cerr << "done." << endl;

}

void schema_sqlite::q(const char *query)
{
  rc = sqlite3_exec(db, query, callback, 0, &zErrMsg);
  if( rc!=SQLITE_OK ){
    auto e = std::runtime_error(zErrMsg);
    sqlite3_free(zErrMsg);
    throw e;
  }
}

schema_sqlite::~schema_sqlite()
{
  sqlite3_close(db);
}
