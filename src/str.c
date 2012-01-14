/* String manipulation functions for muxirc
 *
 * James Stanley 2012
 */

#include <stdlib.h>
#include <string.h>

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

/* append 'append' to s, with end pointer stored at end, without going over
 * maxlen bytes; return s
 */
char *strappend(char *s, char **end, size_t maxlen, const char *append) {
    /* set the end pointer if there isn't one */
    if(!*end)
        *end = s + strlen(s);

    /* add each character until we reach either the byte limit or the end of
     * the string we are appending
     */
    size_t bytestaken = *end - s;
    while(*append && ++bytestaken < maxlen)
        *((*end)++) = *(append++);
    **end = '\0';

    return s;
}
