#include <stdio.h>
#include "config.h"

int get_version(void);

int main(void) {
    printf("version=%d\n", get_version() + APP_VERSION);
    return 0;
}
