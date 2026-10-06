#ifndef HAEDREZ_LAYOUT_H
#define HAEDREZ_LAYOUT_H

typedef struct hd_rect {
    int left;
    int top;
    int right;
    int bottom;
} hd_rect;

hd_rect hd_anchor(hd_rect work, int width, int height, int margin);

#endif