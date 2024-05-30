#ifndef DATABASE_LIB_H
#define DATABASE_LIB_H
#include "sqlite3.h"

void init_db();
void rm_db();
void initialSeed();
int handleLogin(char const *username, char const *password);
#endif
