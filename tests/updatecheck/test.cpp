// updateChecker::compareVersions: the order of release tags, including the patch number that 10.1.1 introduced.
#include "updatecheck.h"
#include <cstdio>

static int failures=0;
static void check(const char *a,const char *op,const char *b)
{
  int c=updateChecker::compareVersions(a,b);
  bool ok=(op[0]=='<')?c<0:(op[0]=='>')?c>0:c==0;
  if(!ok) { printf("  FAIL %s %s %s (compare gave %d)\n",a,op,b,c); failures++; }
  else printf("  ok   %s %s %s\n",a,op,b);
}

int main()
{
  check("10.1.1",">","10.1");              // the reason for the patch number: a 10.1 installation must be offered 10.1.1
  check("10.1",  "<","10.1.1");
  check("10.1.1",">","10.1-Pre3");
  check("10.1.1","<","10.2");
  check("10.1.1","<","11.0");
  check("10.1.2",">","10.1.1");
  check("10.1.10",">","10.1.9");           // numbers, not text
  check("10.1.1-Pre1","<","10.1.1");       // a pre-release ranks below its release
  check("10.1.1-Pre1",">","10.1");
  check("10.1.1-Pre2",">","10.1.1-Pre1");
  check("10.1-Pre3","<","10.1");
  check("10.1",  "=","10.1.0");            // a missing patch is 0
  check("v10.1.1","=","10.1.1");           // leading v
  check("10.0",  "<","10.1");
  check("10.0",  "<","10.0.1");
  check("10.1",  "=","10.1");
  printf("%s\n",failures?"FAILED":"all passed");
  return failures?1:0;
}
