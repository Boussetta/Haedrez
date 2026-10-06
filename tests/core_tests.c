#include "haedrez/layout.h"
#include <stdio.h>

static int failures;

static void check(int condition, const char *name)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        ++failures;
    }
}

int main(void)
{
    hd_rect work = {0, 0, 1920, 1040};
    hd_rect result = hd_anchor(work, 56, 56, 16);
    check(result.left == 1848 && result.top == 968, "above taskbar");
    work = (hd_rect){-1920, -200, 0, 840};
    result = hd_anchor(work, 56, 56, 16);
    check(result.left == -72 && result.top == 768, "negative monitor origin");
    work = (hd_rect){0, 0, 40, 30};
    result = hd_anchor(work, 56, 56, 16);
    check(result.left == 0 && result.top == 0 && result.right == 40 &&
          result.bottom == 30, "small work area");
    result = hd_anchor(work, -5, -5, -5);
    check(result.left == 40 && result.top == 30, "invalid dimensions");
    return failures ? 1 : 0;
}