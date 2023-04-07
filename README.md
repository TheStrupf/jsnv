# Jsnv

Jsnv validates .json ASCII strings terminated by '\0' and is based on a pushdown automata according to the .json flowcharts on www.json.org and ECMA-404. The algorithm iterates through all characters, looks up an encoded action in a table [STATE][CHARACTER] and takes an action accordingly.

It can used for parsing by expanding the switch statement and checking for corresponding state changes.

.jsnv_tablegen.c contains a standalone runnable main() to generate the internal action map.

## usage

```
#include "jsnv.h"

int main(void) {
  char *jsn_string; // formatted in ASCII. Expects null terminator '\0'
  
  // ...
  
  int err = jsnv_validate(jsn_string);
  if (err == JSNV_SUCCESS) { // err is 0 for valid json strings, otherwise an error code >0 is returned.
    // json string is valid
  }
}
```
