#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <windows.h>

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


/*DWORD WINAPI func(LPVOID arg) {
    int i = *(int *)arg;
    free(arg);
    Sleep(i * 1000);
    printf("I'm process number %d\n", i);
    return 0;
}

/*DWORD WINAPI processShoppingList(LPVOID arg) {
    UserData *data = (UserData *)arg;

    printf("Processing shopping list for user: %s\n", data->userID);

    // need to implement
}*/
