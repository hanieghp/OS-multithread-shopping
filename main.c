#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

DWORD WINAPI func(LPVOID arg) {
    int i = *(int *)arg;
    free(arg); 
    Sleep(i * 1000); 
    printf("I'm process number %d\n", i);
    return 0;
}

int main() {
    printf("Hey, I'm the server\n");
    while (1) {
        int *num = malloc(sizeof(int));
        if (num == NULL) {
            perror("Failed to allocate memory");
            return 1;
        }

        printf("Enter a number (0 to exit): ");
        if (scanf("%d", num) != 1) { 
            free(num);
            printf("Invalid input. Exiting.\n");
            break;
        }

        if (*num == 0) { 
            free(num);
            break;
        }

        HANDLE thread = CreateThread(
                NULL,           
                0,            
                func,      
                num,           
                0,             
                NULL           
        );

        if (thread == NULL) {
            perror("Failed to create thread");
            free(num);
            return 1;
        }

        CloseHandle(thread); 
    }

    return 0;
}
