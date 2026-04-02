#!/bin/bash
# Generate N trivial C source files for stress testing
N="${1:-50}"
mkdir -p src
cat > src/main.c << 'MAIN'
#include <stdio.h>
int main(void) {
    int sum = 0;
MAIN

for i in $(seq 1 "$N"); do
    cat > "src/f${i}.c" << EOF
int f${i}(void) { return ${i}; }
EOF
    echo "int f${i}(void);" >> src/main.c
done

for i in $(seq 1 "$N"); do
    echo "    sum += f${i}();" >> src/main.c
done

cat >> src/main.c << 'MAIN'
    printf("%d\n", sum);
    return 0;
}
MAIN
