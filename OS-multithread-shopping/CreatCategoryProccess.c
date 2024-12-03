#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define MAX_PRODUCTS 80
#define MAX_NAME_LEN 100

typedef struct {
    char name[MAX_NAME_LEN];
    int quantity;
    double price;
} Product;

typedef struct {
    char userID[MAX_NAME_LEN];
    Product products[MAX_PRODUCTS];
    int productCount;
    double budgetCap; // if enter no budget cap, set it -1
} UserData;

char* categories[] = {
        "Digital",
        "Home",
        "Apperal",
        "Food",
        "Markets",
        "Toys",
        "Beauty",
        "Sports"
};


DWORD WINAPI func(LPVOID arg) {
    int i = *(int *)arg;
    free(arg);
    Sleep(i * 1000);
    printf("I'm process number %d\n", i);
    return 0;
}

DWORD WINAPI processShoppingList(LPVOID arg) {
    UserData *data = (UserData *)arg;

    printf("Processing shopping list for user: %s\n", data->userID);

    // need to implement
}



int main() {
    printf("Hey, I'm the server\n");

    while (1) {
        UserData *userData = malloc(sizeof(UserData));
        if (userData == NULL) {
            perror("Failed to allocate memory\n");
            return 1;
        }

        printf("Enter user ID (or 'exit' to quit): ");
        scanf("%s", userData->userID);
        if (strcmp(userData->userID, "exit") == 0) {
            free(userData);
            break;
        }

        printf("Enter budget cap (if you have no cap enter -1): ");
        scanf("%lf", &userData->budgetCap);

        printf("Enter number of products: ");
        scanf("%d", &userData->productCount);

        if (userData->productCount > MAX_PRODUCTS) {
            printf("Too many products! Maximum number of product allowed is %d.\n", MAX_PRODUCTS);
            free(userData);
            continue;
        }

        for (int i = 0; i < userData->productCount; i++) {
            printf("  Enter product %d Name: ", i + 1);
            scanf("%s", userData->products[i].name);

            printf("  Enter product %d Quantity: ", i + 1);
            scanf("%d", &userData->products[i].quantity);

            printf("  Enter product %d Price: ", i + 1);
            scanf("%lf", &userData->products[i].price);
        }

        HANDLE thread = CreateThread( // make thread for each user
                NULL,
                0,
                processShoppingList,    // thread function
                userData,               // thread parameter
                0,
                NULL
        );

        if (thread == NULL) {
            perror("Failed to create thread");
            free(userData);
            return 1;
        }
        CloseHandle(thread);
    }

    printf("exit.\n");
    return 0;
}
