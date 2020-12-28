#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

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

void ltrim(char *s) {
    char *p = s;
    while (isspace(*p)) p++;
    strcpy(s, p);
}

void rtrim(char *s) {
    int i = strlen(s) - 1;
    while (i >= 0 && isspace(s[i])) i--;
    s[i + 1] = '\0';
}

void trim(char *s) {
    ltrim(s);
    rtrim(s);
}

int str2int(const char *s, size_t slen, int *value) {
    const char *p = s;
    size_t plen = 0;
    int v, negative = 0;

    if (slen == plen) {
        return 0;
    }

    /* 考虑负数的情况。 */
    if (p[0] == '-') {
        p++;plen++;
        negative = 1;
        if (slen == plen) return 0;
    }

    /* 第一个数字必须为 1-9，否则字符串应该是0。 */
    if (p[0] >= '1' && p[0] <= '9') {
        v = p[0] - '0';
        p++;plen++;
    } else if (p[0] == '0' && slen == 1) {
        *value = 0;
        return 1;
    } else {
        return 0;
    }

    while (plen < slen && p[0] >= '0' && p[0] <= '9') {
        v = (v * 10) + (p[0] - '0');
        if (v > INT_MAX) {
            return 0;
        }
        p++;plen++;
    }

    /* 如果未使用所有字节，则返回 0。 */
    if (plen < slen) return 0;
    /* 如果是负数。 */
    if (negative) v = -v;
    /* 如果指针地址不为空。 */
    if (value != NULL) *value = v;

    return 1;
}

char *substr(char *str, char *begin, char *end) {
    long long len;
    unsigned long long blen;
    char *ret = NULL, *bpos, *epos;

    if ((blen = strlen(begin)) <= 0 || strlen(end) <= 0)
        return ret;

    bpos = strstr(str, begin);
    if (bpos == NULL)
        return ret;

    epos = strstr(bpos + blen, end);
    if (epos == NULL)
        return ret;

    len = epos - bpos;
    if (len <= 0)
        return ret;

    ret = calloc(1, len);
    memcpy(ret, bpos + strlen(begin), len - blen);

    return ret;
}
