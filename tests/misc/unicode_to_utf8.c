#include <stdio.h>

int main() {
    unsigned int codePoint = 0xf6b1;

    // Convert the code point to UTF-8
    unsigned char utf8Bytes[4];
    int numBytes = 0;

    if (codePoint <= 0x7F) {
        utf8Bytes[0] = codePoint;
        numBytes = 1;
    } else if (codePoint <= 0x7FF) {
        utf8Bytes[0] = 0xC0 | (codePoint >> 6);
        utf8Bytes[1] = 0x80 | (codePoint & 0x3F);
        numBytes = 2;
    } else if (codePoint <= 0xFFFF) {
        utf8Bytes[0] = 0xE0 | (codePoint >> 12);
        utf8Bytes[1] = 0x80 | ((codePoint >> 6) & 0x3F);
        utf8Bytes[2] = 0x80 | (codePoint & 0x3F);
        numBytes = 3;
    } else if (codePoint <= 0x10FFFF) {
        utf8Bytes[0] = 0xF0 | (codePoint >> 18);
        utf8Bytes[1] = 0x80 | ((codePoint >> 12) & 0x3F);
        utf8Bytes[2] = 0x80 | ((codePoint >> 6) & 0x3F);
        utf8Bytes[3] = 0x80 | (codePoint & 0x3F);
        numBytes = 4;
    } else {
        printf("Invalid Unicode code point.\n");
        return 1;
    }

    // Print the UTF-8 bytes
    printf("UTF-8: ");
    for (int i = 0; i < numBytes; i++) {
        printf("%02X ", utf8Bytes[i]);
    }
    printf("\n");

    return 0;
}
