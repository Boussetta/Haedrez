#include "haedrez/layout.h"

hd_rect hd_anchor(hd_rect work, int width, int height, int margin)
{
    hd_rect result;
    if (margin < 0) margin = 0;
    if (width < 0) width = 0;
    if (height < 0) height = 0;
    result.left = work.right - width - margin;
    result.top = work.bottom - height - margin;
    if (result.left < work.left) result.left = work.left;
    if (result.top < work.top) result.top = work.top;
    result.right = result.left + width;
    result.bottom = result.top + height;
    if (result.right > work.right) result.right = work.right;
    if (result.bottom > work.bottom) result.bottom = work.bottom;
    return result;
}