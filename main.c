#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <windows.h>
#include "OS-multithread-shopping/CreatCategoryProccess.c" 

#define MAX_PRODUCTS 80
#define MAX_NAME_LEN 100
#define storeCount 3
#define categoryCount 8

typedef struct { 
    char name[MAX_NAME_LEN]; 
    double price; 
    double score; 
    int entity; 
} Product;

typedef struct {
    char userID[MAX_NAME_LEN];
    Product products[MAX_PRODUCTS];
    int productCount;
    double budgetCap; // if enter no budget cap, set it -1
    double totalCost;
    int store_match_count[3];
} UserShoppingList;

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

UserShoppingList* read_user_shopping_list() {
    UserShoppingList* shoppingList = malloc(sizeof(UserShoppingList));
    
    printf("Enter User ID: ");
    scanf("%99s", shoppingList->userID);
    
    
    printf("Enter number of products: ");
    scanf("%d", &shoppingList->productCount);


    printf("Order list: \n");
    for (int i = 0; i < shopping_list->productCount; i++) {
        printf("Product %d Name: ", i + 1);
        scanf("%99s", shoppingList->products[i].name);
        
        printf("Product %d Quantity: ", i + 1);
        scanf("%d", &shoppingList->products[i].entity);
    }
    printf("Enter Budget Cap (-1 for no cap): ");
    scanf("%lf", &shoppingList->budgetCap);
    
    return shoppingList;
}

char** getSubDirectories(char dir[1000]){
    int count = 0;
    char **categories = malloc(sizeof(char *) * 1000);       
    char sdir[1000], command[1000];
    snprintf(command, sizeof(command), "find %s -maxdepth 1 -type d", dir);
    FILE *fp = popen((command), "r");
    
    if (!fp) {
        perror("Error opening files");
        if (fp) fclose(fp);
        return NULL;
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

Product* searchProductInCategory(const char* categoryPath, const char* productName){
    return NULL;
    // neeed to implement
}

void processStoreCategories(const char* storePath, UserShoppingList* shoppingList){//making process for categories
    char** categories = getSubDirectories(storePath);

    for(int i = 0; i < categoryCount; i++){
        pid_t pidCategory = vfork();
        if(pidCategory == 0){
            printf("processing category: %s\n",categories[i]);
            
            for(int j = 0; j < shoppingList->productCount; j++){
                char categoryFile[1000];
                snprintf(categoryFile, sizeof(categoryFile), "%s/%s.txt", categories[i], shoppingList->products[j].name);

                Product* foundProduct = searchProductInCategory(categoryFile, shoppingList->products[i].name);
                if(foundProduct){ // found product in category store
                    printf("found product: %s in %s\n",shoppingList->products[j].name, categoryFile);
                    // hala ke peyda shod mitone bekhare
                    // badan piadesazi beshe
                }
            }
            exit(0);
        }
        else if(pidCategory < 0){
            perror("Failed to fork for category\n");
        }
    }
    for(int i = 0; i < categoryCount; i++){
        free(categories[i]);
    }
    free(categories);
}

void processStroes(UserShoppingList* shoppingList){ //making process for stores
    char** stores = getSubDirectories("Dataset");
    for(int i = 0; i < storeCount; i++){
        pid_t pidStore = vfork();
        if(pidStore == 0){
            printf("processing store: %s\n",stores[i]);
            processStoreCategories(stores[i],shoppingList);
            exit(0);
        }
        else if(pidStore < 0){
            perror("Failed to fork for store\n");
        }
    }
    for(int i = 0; i < storeCount; i++){
        free(stores[i]);
    }
    free(stores);
}

int main(){
    while (1) {
        pid_t pidUser = vfork(); //process user

        if(pidUser < 0){
            perror("Failed to fork for User\n");
        }
        else if(pidUser == 0){ 
            UserShoppingList* shoppingList = read_user_shopping_list();
            processStroes(shoppingList);
            free(shoppingList);
            exit(0);
        }
    }
    printf("Exiting...\n");
    return 0;
}