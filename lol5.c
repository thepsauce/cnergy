#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef unsigned int U32;

void rotateString(char* str, U32 length, U32 amount)
{
    // Reverse the entire string
    U32 start = 0;
    U32 end = length - 1;
    while (start < end) {
        const char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }

    // Reverse the first portion
    start = 0;
    end = amount - 1;
    while (start < end) {
        const char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }

    // Reverse the remaining string
    start = amount;
    end = length - 1;
    while (start < end) {
        const char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

void move(char *s, U32 len, U32 amount)
{
    while (amount < len) {
        if (2 * amount > len)
            amount = len - amount;
        for (U32 i = 0; i < amount; i++) {
            const char tmp = s[i];
            s[i] = s[len - amount + i];
            s[len - amount + i] = tmp;
        }
        s += amount;
        len -= amount;
    }
}

double measureExecutionTime(void (*function)(char*, U32, U32), char* str, int length, int amount)
{
    const clock_t start = clock();
    function(str, length, amount);
    const clock_t end = clock();

    const double executionTime = (double) (end - start) / (double) CLOCKS_PER_SEC;
    return executionTime;
}

int main(void)
{
    char str[] = "abcdefghijklmnopqrstuvwxyz";
    int length = strlen(str);
    int amount = 10;
    int numIterations = 10000000;

    double rotateStringTime = 0.0;
    double moveTime = 0.0;

    for (int i = 0; i < numIterations; i++) {
        char* strCopy = strdup(str);
        rotateStringTime += measureExecutionTime(rotateString, strCopy, length, amount);
        free(strCopy);

        char* strCopy2 = strdup(str);
        moveTime += measureExecutionTime(move, strCopy2, length, amount);
        free(strCopy2);
    }

    rotateStringTime /= numIterations;
    moveTime /= numIterations;

    printf("Rotate String Approach: %.6f seconds\n", rotateStringTime);
    printf("Move Function Approach: %.6f seconds\n", moveTime);

    return 0;
}
