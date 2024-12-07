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
}*/

UserData *processShoppingList () {
    UserData *userData = malloc(sizeof(UserData));
    if (userData == NULL) {
        perror("Failed to allocate memory\n");
        return 1;
    }

    printf("Enter user ID (or 'exit' to quit): ");
    scanf("%s", userData->userID);
    if (strcmp(userData->userID, "exit") == 0) {
        free(userData);
        return 0;
    }

    printf("Enter budget cap (if you have no cap enter -1): ");
    scanf("%lf", &userData->budgetCap);

    printf("Enter number of products: ");
    scanf("%d", &userData->productCount);

    /*if (userData->productCount > MAX_PRODUCTS) {
        printf("Too many products! Maximum number of product allowed is %d.\n", MAX_PRODUCTS);
        free(userData);
        continue;
    }*/

    for (int i = 0; i < userData->productCount; i++) {
        printf("  Enter product %d Name: ", i + 1);
        scanf("%s", userData->products[i].name);

        printf("  Enter product %d Quantity: ", i + 1);
        scanf("%d", &userData->products[i].quantity);

        printf("  Enter product %d Price: ", i + 1);
        scanf("%lf", &userData->products[i].price);
    }
    printf("Processing shopping list for user: %s\n", userData->userID);

    return userData;
}

char** getSubDirectories(char dir[1000]){
    int count = 0;
    char **categories = malloc(sizeof(char *) * 1000);       
    char sdir[1000], command[1000];
    printf("im in\n");
    snprintf(command, sizeof(command), "find %s -maxdepth 1 -type d", dir);
    printf("im in\n");
    FILE *fp = popen((command), "r");
    printf("im in\n");
    
    if (!fp) {
        perror("Error opening files");
        if (fp) fclose(fp);
        return;
    }
    fgets(sdir, sizeof(sdir), fp);
    while(fgets(sdir, sizeof(sdir), fp)!=NULL){
        printf("%s", sdir);
        categories[count] = strdup(sdir);
        count++;                 
    }
    pclose(fp);
    return categories;
}