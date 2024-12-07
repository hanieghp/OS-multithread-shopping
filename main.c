#include <stdio.h>
#include "OS-multithread-shopping/CreatCategoryProccess.c" 

int main(){
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

        pid_t pidUser = vfork();
        
        if(pidUser < 0){
            printf("fork faild!\n");
        }

        if(pidUser == 0){
            int storeCount = 3;
            // making process for stores
            for (int i = 0; i < storeCount; i++){
                pid_t pidStore = vfork();
                if(pidStore == 0){
                    printf("im searching for your items in store%d\n", i);
                    
                }
                if(pidStore < 0){
                    printf("make process for store number %d faild\n", i);
                }
            }
        }

    }
    printf("exit.\n");

    return 0;
}