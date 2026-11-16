#include <stdio.h>

#include "sys-sight/marker.h"

enum color { RED, GREEN = 5, BLUE };

struct point {
    int x;
    int y;
};

struct shape {
    char label[8];
    struct point corner;
    enum color color;
    double scale;
    unsigned flags : 3;
    const char *note;
};

union number {
    int whole;
    float real;
};

static void show(int depth, struct point origin)
{
    int counter = 42;
    int numbers[3] = {10, 20, 30};
    int grid[2][2] = {{1, 2}, {3, 4}};
    struct shape square = {"sq", {3, 4}, GREEN, 1.5, 5, "hello"};
    union number both;
    int *pointer = &counter;

    both.whole = 0x3f800000;
    {
        int inside_block = 7;

        SYSSIGHT_SNAPSHOT();

        printf("%d %d %d %d %d %p %f %d %d\n", depth, origin.x, counter, numbers[0], grid[1][1],
               (void *)pointer, (double)both.real, square.corner.x, inside_block);
    }
}

int main(void)
{
    struct point start = {1, 2};

    show(1, start);
    return 0;
}
