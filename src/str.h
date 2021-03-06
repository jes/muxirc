/* String manipulation functions for muxirc
 *
 * James Stanley 2012
 */

#ifndef STRING_H_INC
#define STRING_H_INC

char *strprefix(const char *s, size_t n);
char *strappend(char *s, char **end, size_t maxlen, const char *append);

#endif
