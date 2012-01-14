/* String manipulation functions for muxirc
 *
 * James Stanley 2012
 */

#include <stdlib.h>

#include "str.h"

/* return a copy of the first n characters in s */
char *strprefix(const char *s, size_t n) {
    char *p = malloc(n + 1);
    int i;

    /* copy characters until either n bytes or the end of s is reached */
    for(i = 0; i < n && s[i]; i++)
        p[i] = s[i];

    p[i] = '\0';

    return p;
}
