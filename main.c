#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <bits/pthreadtypes.h>
#include <ctype.h>
#include <stdbool.h>

#define FILESNUMBER 79
#define MAX_PRODUCTS 80
#define MAX_NAME_LEN 100
#define MAX_PATH_LEN 1000
#define storeCount 3
#define MAX_storeCount 3
#define categoryCount 8
#define maxUser 10


#define SEM_PRODUCT_SEARCH "/product_search_sem"
#define SEM_RESULT_UPDATE "/result_update_sem"
#define SEM_INVENTORY_UPDATE "/inventory_update_sem"
#define SEM_SHOPPING_LIST "/shopping_list_sem"

#define SHM_KEY 0x1234

typedef struct {
   char name[MAX_NAME_LEN];
   double price;
   double score;
   int entity;
   char lastModified[50];
   int foundFlag; //to check if product is found
} Product;

typedef struct {
  char userID[MAX_NAME_LEN];
  Product products[MAX_storeCount][MAX_PRODUCTS];
  int productCount;
  double budgetCap; // if enter no budget cap, set it -1
  double totalCost;
  int store_match_count[MAX_storeCount];
  pid_t userPID;
  int processingComplete;
  //UserMembership membership;
  int entity[MAX_PRODUCTS];
} UserShoppingList;

typedef struct {
    int proNum;
    int proCount;
   char *filepath;
   char **names;
   Product *product;
} threadInput;

sem_t *g_search_sem = NULL;
sem_t *g_result_sem = NULL;
sem_t *g_inventory_sem = NULL;
sem_t *g_shopping_list_sem = NULL;

pthread_mutex_t liveLock = PTHREAD_MUTEX_INITIALIZER;
bool stopThread = false;

//define all functions
void processCategories(int storeNum, const char* storePath, UserShoppingList* shoppingList);
void processStroes(UserShoppingList* shoppingList);
void processUser(UserShoppingList* shoppingList);

char** getSubStoreDirectories(const char *dir){
  int count = 0;
  char **categories = malloc(sizeof(char *) * 1000);     
  char subDir[MAX_PATH_LEN], command[MAX_NAME_LEN];
  for(int i = 0; i < 3; i++){
    snprintf(command, sizeof(command), "find %s -name Store%d -type d", dir, i+1);
    command[strcspn(command, "\n")] = 0;
    //printf("command is: %s\n",command);
    FILE *fp = popen((command), "r");
   if (!fp) {
      perror("Error opening files");
      if (fp) fclose(fp);
      return NULL;
  }
  //fgets(subDir, sizeof(subDir), fp);
  if(fgets(subDir, sizeof(subDir), fp)!=NULL){
      subDir[strcspn(subDir, "\n")] = 0;
      categories[count] = strdup(subDir);
      count++;               
  }
  pclose(fp);
  }
  
  return categories;
}

Product* readProductFromFile(const char* filepath) {
   FILE* file = fopen(filepath, "r");
   if (!file) {
       return NULL;
   }

   Product* product = malloc(sizeof(Product));
   memset(product, 0, sizeof(Product));
   char line[200];

   while (fgets(line, sizeof(line), file)) {
       if (strncmp(line, "Name:", 5) == 0) {
           sscanf(line, "Name: %99[^\n]", product->name);
       } else if (strncmp(line, "Price:", 6) == 0) {
           sscanf(line, "Price: %lf[\n]", &product->price);
       } else if (strncmp(line, "Score:", 6) == 0) {
           sscanf(line, "Score: %lf", &product->score);
       } else if (strncmp(line, "Entity:", 7) == 0) {
           sscanf(line, "Entity: %d", &product->entity);
       } else if (strncmp(line, "Last Modified:", 14) == 0) {
           sscanf(line, "Last Modified: %49[^\n]", product->lastModified);
       }
   }
   fclose(file);
   product->foundFlag = 0;
   return product;
}


UserShoppingList* read_user_shopping_list() {
   UserShoppingList* shoppingList = malloc(sizeof(UserShoppingList));
   memset(shoppingList, 0, sizeof(UserShoppingList));
  
   printf("Enter User ID: ");
   scanf("%99s", shoppingList->userID);
  
   printf("Enter number of products: ");
   scanf("%d", &shoppingList->productCount);
    printf("the number : %d\n", shoppingList->productCount);
   printf("Order list: \n");
   for (int i = 0; i < shoppingList->productCount; i++) {
       printf("Product %d Name: ", i + 1);
       scanf("%99s", shoppingList->products[1][i].name);
       //scanf("%99s", shoppingList->products[2][i].name);
       //scanf("%99s", shoppingList->products[3][i].name);
       printf("Product %d Quantity: ", i + 1);
       scanf("%d", &shoppingList->entity[i]);
       //scanf("%d", &shoppingList->products[2][i].entity);
       //scanf("%d", &shoppingList->products[3][i].entity);
   }
   printf("Enter Budget Cap (-1 for no cap): ");
   scanf("%lf", &shoppingList->budgetCap);

   shoppingList->userPID = getpid();
   return shoppingList;
}


// Function to list all products in a category
void listCategoryProducts(const char* categoryPath) {
   DIR* dir;
   struct dirent* entry;

   dir = opendir(categoryPath);
   if (dir == NULL) {
       printf("Unable to open category directory: %s\n", categoryPath);
       return;
   }
   printf("Products in category: %s\n", categoryPath);

   while ((entry = readdir(dir)) != NULL) {
       if (entry->d_type == DT_REG) {  // If it's a regular file
           char filepath[MAX_PATH_LEN];
           snprintf(filepath, sizeof(filepath), "%s/%s", categoryPath, entry->d_name);
           Product* product = readProductFromFile(filepath);
           if (product) {
               printf("- %s (Price: %.2f, Score: %.2f, Quantity: %d)\n",
                      product->name, product->price, product->score, product->entity);
               //free(product);
           }
       }
   }
   closedir(dir);
}


// Function to list all products in a store
void listStoreProducts(const char* storePath) {
   DIR* dir;
   struct dirent* entry;

   dir = opendir(storePath);
   if (dir == NULL) {
       printf("Uunable to open store directory: %s\n", storePath);
       return;
   }

   printf("Listing products for store: %s\n", storePath);

   while ((entry = readdir(dir)) != NULL) {
       if (entry->d_type == DT_DIR &&
           strcmp(entry->d_name, ".") != 0 &&
           strcmp(entry->d_name, "..") != 0) {  // If it's a subdirectory (category)
           char categoryPath[MAX_PATH_LEN];
           snprintf(categoryPath, sizeof(categoryPath), "%s/%s", storePath, entry->d_name);
           listCategoryProducts(categoryPath);
       }
   }
   closedir(dir);
}


char** getSubDirectories(const char *dir){
   int count = 0;
   char **categories = malloc(sizeof(char *) * 1000);      
   char subDir[MAX_PATH_LEN], command[MAX_NAME_LEN];
   snprintf(command, sizeof(command), "find %s -maxdepth 1 -type d", dir);
   FILE *fp = popen((command), "r");
  
   if (!fp) {
       perror("Error opening files");
       if (fp) fclose(fp);
       return NULL;
   }
   fgets(subDir, sizeof(subDir), fp);
   while(fgets(subDir, sizeof(subDir), fp)!=NULL){
       categories[count] = strdup(subDir);
       count++;                
   }
   pclose(fp);
   return categories;
}

double calculateProductValue(Product* product){
    if(product->price <= 0){
        return 0;
    }
    printf("score is: %.2f and price is: %.2f\n",product->score ,product->price);
    return product->score * product->price;
}

void* calculateStoreBaskettValue(void* args){
    UserShoppingList* shoppingList = (UserShoppingList*)args;

    for(int i = 0; i < MAX_storeCount; i++){
        double totalBasketValue = 0.0;
        int validProductCount = 0;

        for(int j = 0; j < shoppingList->productCount; j++){
            Product* curProduct = &(shoppingList->products[i][j]);

            if(curProduct->foundFlag){
                double productValue = calculateProductValue(curProduct) * shoppingList->entity[j];
                totalBasketValue += productValue;
                validProductCount++;
            }
        }
        sem_wait(g_shopping_list_sem);
        shoppingList->totalCost = totalBasketValue;
        shoppingList->store_match_count[i] = validProductCount;

        printf("store %d basket value: %.2f\n", i+1, totalBasketValue);
        printf("match product in store %d: %d out of %d\n", i+1, validProductCount, shoppingList->productCount);
        printf("TID: %ld and PID: %d\n", pthread_self(), getpid());

        if(shoppingList->budgetCap > 0 && totalBasketValue > shoppingList->budgetCap){
            printf("store %d total cost %.2f but budget cap %.2f\n", i+1, totalBasketValue, shoppingList->budgetCap);
        }
        sem_post(g_shopping_list_sem);
    }
    return NULL;
}

void* searchProductInCategory(void* args){
    //printf("in thread with tid : %ld\n", pthread_self());
    threadInput *input = (threadInput *)args;
    char** proNames = input->names;
    sem_wait(g_search_sem);
    Product* product = readProductFromFile(input->filepath);
    sem_post(g_search_sem);
    //printf("name : %s, %s, %s\n", product->name, input->filepath, proNames[0]);
    for(int i = 0; i < input->proCount; i++){
        //printf("name : %s\n", product->name);
        if (product && strcasecmp(product->name, proNames[i]) == 0){
            sem_wait(g_result_sem);
            printf("i found it in %s!!!!\n", input->filepath);
            memcpy(input->product->name, product->name, sizeof(product->name));
            memcpy(input->product->lastModified, product->lastModified, sizeof(product->lastModified));
            input->product->price = product->price;
            input->product->score = product->score;
            input->product->entity = product->entity;
            input->product->foundFlag = 1;
            input->proNum = i;
            sem_post(g_result_sem);
            /*while(true){
                pthread_mutex_lock(&liveLock);
                if(stopThread){
                    pthread_mutex_unlock(&liveLock);
                    break;
                }
                pthread_mutex_unlock(&liveLock);
            }*/
        }
    }
    free(product);
    return NULL;
}

char ** getsubfiles(char *dir){
    int count = 0;
    char **files = malloc(sizeof(char *) * 1000); 
    char filepath[MAX_PATH_LEN], command[1000];
    snprintf(command, sizeof(command), "find %s -maxdepth 1 -type f", dir);
    FILE *fp = popen((command), "r");
    if (!fp) {
       perror("Error opening files");
       if (fp) fclose(fp);
       return NULL;
    }
    fgets(filepath, sizeof(filepath), fp);
    while(fgets(filepath, sizeof(filepath), fp)!=NULL){
        filepath[strcspn(filepath, "\n")] = 0;
        files[count] = strdup(filepath);
        //printf("file : %s", files[count]);
        count++; 
    }
    pclose(fp);
    return files;
}


void processCategories(int storeNum, const char* storePath, UserShoppingList* shoppingList) { 
    pthread_t threads[1000]; 
    char** categories = getSubDirectories(storePath); 
    char** productNames = malloc(shoppingList->productCount * sizeof(char*)); 

    for (int k = 0; k < shoppingList->productCount; k++) { 
        productNames[k] = malloc(strlen(shoppingList->products[1][k].name) + 1); 
        strcpy(productNames[k], shoppingList->products[1][k].name); 
    } for (int i = 0; i < categoryCount; i++) { 
        pid_t pidCategory = vfork(); 
        if (pidCategory == 0) { 
            //printf("category : %s\n", categories[i]); 
            categories[i][strcspn(categories[i], "\n")] = 0; 
            char** productFiles = getsubfiles(categories[i]); 
            int j = 0; 
            threadInput* inputs[500]; 
            while (productFiles[j] != NULL) { 
                inputs[j] = malloc(sizeof(threadInput)); 
                inputs[j]->filepath = productFiles[j]; 
                inputs[j]->proCount = shoppingList->productCount; 
                inputs[j]->names = productNames; 
                inputs[j]->product = malloc(sizeof(Product)); 
                pthread_create(&threads[j], NULL, searchProductInCategory, (void*)inputs[j]); 
                j++; 
            } 
            for (int l = 0; l < j; l++) { 
                pthread_join(threads[l], NULL); 
                if (inputs[l]->product->foundFlag == 1) { 
                    //printf("%d : found product: %s in %s\n", storeNum, shoppingList->products[1][l].name, categories[i]); 
                    
                    memcpy(&(shoppingList->products[storeNum][inputs[l]->proNum]), inputs[l]->product, sizeof(Product)); 
                } 
                free(inputs[l]->product); 
                free(inputs[l]); 
            } 
            exit(0); 
            } else if (pidCategory < 0) {
                perror("Failed to fork for category\n"); 
            }
        } // Free memory 
        for (int k = 0; k < shoppingList->productCount; k++) { 
            free(productNames[k]); 
            } 
        free(productNames); 
    }  

void processStores(UserShoppingList* shoppingList){ //making process for stores
    char** stores = getSubStoreDirectories("Dataset");

   for(int i = 0; i < storeCount; i++){
       pid_t pidStore = vfork();
       if(pidStore == 0){
           //printf("processing store: %s\n",stores[i]);
           stores[i][strcspn(stores[i], "\n")] = 0;
           processCategories(i, stores[i], shoppingList);
           exit(0);
       }
       else if(pidStore < 0){
           perror("Failed to fork for store\n");
       }
       printf("store %d finished\n",i+1);
   }
   for(int i = 0; i < storeCount; i++){
       free(stores[i]);
   }
   free(stores);
}



void processUser(UserShoppingList* shoppingList){
    /*sem_unlink(SEM_PRODUCT_SEARCH);
   sem_unlink(SEM_RESULT_UPDATE);
   sem_unlink(SEM_INVENTORY_UPDATE);
   sem_unlink(SEM_SHOPPING_LIST);*/
    printf("in user process with pid: %d\n", getpid());
   //semaphore
   g_search_sem = sem_open(SEM_PRODUCT_SEARCH, O_CREAT, 0644, 1);
   g_result_sem = sem_open(SEM_RESULT_UPDATE, O_CREAT, 0644, 1);
   g_inventory_sem = sem_open(SEM_INVENTORY_UPDATE, O_CREAT, 0644, 1);
   g_shopping_list_sem = sem_open(SEM_SHOPPING_LIST, O_CREAT, 0644, 1);
  
   if (g_search_sem == SEM_FAILED || g_result_sem == SEM_FAILED ||
       g_inventory_sem == SEM_FAILED || g_shopping_list_sem == SEM_FAILED) {
       perror("Semaphore creation failed");
       return;
   }

   processStores(shoppingList);
   pthread_t basketValueThread;
   if(pthread_create(&basketValueThread ,NULL, calculateStoreBaskettValue, (void*)shoppingList) != 0){
    perror("faile to create basekt value");
    return;
   }
   pthread_join(basketValueThread, NULL);

    printf("\nProcessed Shopping List for User %s:\n", shoppingList->userID);
    for (int i = 0; i < shoppingList->productCount; i++) {
        for (int j = 0; j < storeCount; j++){
            if (shoppingList->products[j][i].foundFlag) {
                printf("store %d Product %d: %s (Price: %.2f, Score: %.2f, Entity: %d, userEntity : %d)\n",
                        j+1,
                        i+1,
                        shoppingList->products[j][i].name,
                        shoppingList->products[j][i].price,
                        shoppingList->products[j][i].score,
                        shoppingList->products[j][i].entity,
                        shoppingList->entity[i]);
            } else {
                printf("store %d Product %d: %s - Not Found\n",
                        j+1,
                        i+1,
                        shoppingList->products[j][i].name);
            }
        }
    }


   // Clean up semaphores
   sem_close(g_search_sem);
   sem_close(g_result_sem);
   sem_close(g_inventory_sem);
   sem_close(g_shopping_list_sem);

   sem_unlink(SEM_PRODUCT_SEARCH);
   sem_unlink(SEM_RESULT_UPDATE);
   sem_unlink(SEM_INVENTORY_UPDATE);
   sem_unlink(SEM_SHOPPING_LIST);
}

int main(){
   while (1) {
       pid_t pidUser = vfork(); //process user

       if(pidUser < 0){
           perror("Failed to fork for User\n");
           break;
       }
       else if(pidUser == 0){
            pthread_t threads[storeCount];
            UserShoppingList* shoppingList = read_user_shopping_list();
            /*if(pthread_create(&threads[storeCount], NULL, calculateStoreBaskettValue, (void*)shoppingList != 0)){
                perror("faile to create basekt value");
                exit(1);
            }*/
            processUser(shoppingList);
                // show user its choices
            //pthread_create(&threads[1], NULL, &getProductsValue, shoppingList->products);
            free(shoppingList);
            exit(0);
       }
   }


   printf("Exiting...\n");
   return 0;
}