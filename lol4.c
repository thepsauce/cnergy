#include <stdio.h>
#include <string.h>

typedef unsigned int U32;

void move(char* str, U32 length, U32 amount)
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

int main()
{
    char buffer[] = "Hello world, my name is Abdul Azmir, k?";
    
    printf("Before: %s\n", buffer);
    move(buffer + 2, strlen(buffer) - 6, 3);
    printf("After:  %s\n", buffer);
    
    printf("Before: %s\n", buffer);
    move(buffer + 6, strlen(buffer) - 6, 4);
    printf("After:  %s\n", buffer);
    
    printf("Before: %s\n", buffer);
    move(buffer, strlen(buffer) - 10, 2);
    printf("After:  %s\n", buffer);
    return 0;
}
