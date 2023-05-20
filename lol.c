#include <stdio.h>
#include <string.h>

typedef unsigned int U32;

int
move(char *s, U32 len, int amount)
{
	if(amount < 0) {
		amount = -amount;
		while(1) {
			amount %= len;
			if(!amount)
				break;
			len -= amount;
			for(U32 i = amount; i; ) {
				i--;
				const char tmp = s[len + i];
				s[len + i] = s[i];
				s[i] = tmp;
			}
		}
	} else {
		while(1) {
			amount %= len;
			if(!amount)
				break;
			len -= amount;
			for(U32 i = 0; i < amount; i++) {
				const char tmp = *s;
				*s = s[len];
				s[len] = tmp;
				s++;
			}
		}
	}
	return 0;
}

int
main(void)
{
    char str[] = "Hello world, my name is Abdul Azmir, k?";
	char buffer[1000];
    
	strcpy(buffer, str);
    printf("Before: %s\n", buffer);
	move(buffer, strlen(buffer), -10000);
	move(buffer, strlen(buffer), 10000);
    printf("After:  %s\n", buffer);
    
	return 0;
	strcpy(buffer, str);
    printf("Before: %s\n", buffer);
	for(int i = 0; i <= 1983; i++) {
		move(buffer + 2, strlen(buffer) - 8, 8);
		if(!strcmp(buffer, str)) {
			printf("%d repeat\n", i);
		}
	}
    printf("After:  %s\n", buffer);
    return 0;
}
