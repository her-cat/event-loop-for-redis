#include <string.h>

int strpos(const char *haystack, const char *needle) {
    if (strlen(needle) == 0) return 0;

    unsigned long n = strlen(haystack), m = strlen(needle);

    for (int i = 0; i <= n - m; i++) {
        int j = 0, k = i;
        for (; j < m && k < n && haystack[k] == needle[j]; j++, k++);

        if (j == m) return i;
    }

    return -1;
}