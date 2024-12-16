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
#include <fcntl.h>
#include <sys/mman.h>


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
#define SEM_RATING_UPDATE "/rating_update_sem"
#define SEM_SHOPPING_LIST "/shopping_list_sem"

#define SHM_KEY 0x1234

typedef struct {
   char name[MAX_NAME_LEN];
   double price;
   double score;
   int entity;
   int originEntity;
   char lastModified[50];
   int foundFlag; //to check if product is found
} Product;

typedef enum {
    NoMember,
    FirstTimeMember,
    LoyalMember
} UserMemberShip;

typedef struct {
  char userID[MAX_NAME_LEN];
  Product products[MAX_storeCount][MAX_PRODUCTS];
  int productCount;
  double budgetCap; // if enter no budget cap, set it -1
  double totalCost;
  int store_match_count[MAX_storeCount];
  pid_t userPID;
  int processingComplete;
  UserMemberShip storeMembership[MAX_storeCount];
  int purchaseCount[MAX_storeCount];
  bool hasDiscount[MAX_storeCount];
  int entity[MAX_PRODUCTS];
  int shmFd;
  int stopThread;
} UserShoppingList;

typedef struct {
    int proNum;
    int proCount;
   char *filepath;
   char **names;
   Product *product;
   UserShoppingList* shoppingList;
   int storeNum;
} threadInput;

typedef struct { 
    pthread_t threadID;
    char productName[MAX_NAME_LEN];
    int storeIndex;
    int productIndex;
} ProductThreadInfo;

sem_t *g_search_sem = NULL;
sem_t *g_result_sem = NULL;
sem_t *g_rating_sem = NULL;
sem_t *g_shopping_list_sem = NULL;

UserShoppingList* shoppingList;
int *userCount = 0;
pthread_mutex_t liveLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t liveCond = PTHREAD_COND_INITIALIZER;


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

// Function to read data from dataset file
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
char* findProductFilePath(const char* productName) {
    char** stores = getSubStoreDirectories("Dataset");
    if(!stores) return NULL;
    char* foundPath = NULL;
    for (int i = 0; i < storeCount; i++) {
        stores[i][strcspn(stores[i], "\n")] = 0;
        char** categories = getSubDirectories(stores[i]);
        for (int j = 0; categories[j] != NULL; j++) {
            categories[j][strcspn(categories[j], "\n")] = 0;
            char** productFiles = getsubfiles(categories[j]);
            for (int k = 0; productFiles[k] != NULL; k++) {
                Product* tempProduct = readProductFromFile(productFiles[k]);
                if (tempProduct && strcasecmp(tempProduct->name, productName) == 0) {
                    foundPath = strdup(productFiles[k]);
                    free(tempProduct);
                    /*for (int m = 0; productFiles[m] != NULL; m++) {
                        free(productFiles[m]);
                        productFiles[m] = NULL;
                    }
                    free(productFiles);
                    productFiles = NULL;
                    
                    for (int m = 0; categories[m] != NULL; m++) {
                        free(categories[m]);
                        categories[m] = NULL;
                    }
                    free(categories);
                    categories = NULL;
                    
                    for (int m = 0; stores[m] != NULL; m++) {
                        free(stores[m]);
                        stores[m] = NULL;
                    }
                    free(stores);
                    stores[m] = NULL;*/
                    
                    return foundPath;
                }
                //free(tempProduct);
            }
            for (int m = 0; productFiles[m] != NULL; m++) {
                free(productFiles[m]);
                productFiles[m] = NULL;
            }
            free(productFiles);
            productFiles = NULL;
        }
        for (int m = 0; categories[m] != NULL; m++) {
            free(categories[m]);
            categories[m] = NULL;
        }
        free(categories);
        categories = NULL;
    }

    for (int m = 0; stores[m] != NULL; m++) {
        free(stores[m]);
        stores[m] = NULL;
    }
    free(stores);
    stores = NULL;

    return NULL;
}

double calculateProductValue(Product* product){
    if(product->price <= 0){
        return 0;
    }
    //printf("score is: %.2f and price is: %.2f\n",product->score ,product->price);
    return product->score * product->price;
}

void* calculateStoreBaskettValue(void* args){
    usleep(1000000);
   sem_wait(g_shopping_list_sem);
   UserShoppingList* shoppingList = (UserShoppingList*)args;
   for(int i = 0; i < MAX_storeCount; i++){
       shoppingList->store_match_count[i] = 0;
   }
   double bestTotalValue = 0.0;
   int bestStore = -1;
   for(int i = 0; i < MAX_storeCount; i++){
       double totalBasketValue = 0.0;
       int totalCost = 0;
       int allProductFound = 1;


       for(int j = 0; j < shoppingList->productCount; j++){
           Product* curProduct = &(shoppingList->products[i][j]);


           if(curProduct->foundFlag){
               double productValue = calculateProductValue(curProduct);


               if(curProduct->entity >= shoppingList->entity[j]){
               totalBasketValue += productValue;
               totalCost += curProduct->price * shoppingList->entity[j];
               }
               else{
                   allProductFound = 0;
                   printf("store %d just have %d, you want %d, you can't buy from\n", i+1, curProduct->entity, shoppingList->entity[j]);
                   break;
               }
           }
           else{
               allProductFound = 0;
               printf("store %d doesn't have %s, you can't buy from\n", i+1, curProduct->name);
               break;
           }
       }
       if(allProductFound){
           //printf("allfound\n");
           if(shoppingList->budgetCap == -1 || totalCost <= shoppingList->budgetCap){
               if(totalBasketValue > bestTotalValue){
                   bestTotalValue = totalBasketValue;
                   bestStore = i;
               }
               shoppingList->store_match_count[bestStore] = 1;
               shoppingList->totalCost = totalCost;
           }
       }

    
       //printf("store %d basket value: %.2f\n", i+1, totalBasketValue);
       printf("in calculating: TID: %ld and PID: %d\n", pthread_self(), getpid());
       printf("best store is: %d\n",bestStore);
   }
       shoppingList->stopThread = true;
        printf("stopThread : %d", shoppingList->stopThread);
    
    //pthread_mutex_lock(&liveLock);
    //pthread_mutex_unlock(&liveLock);

    sem_post(g_shopping_list_sem);
    int r = pthread_cond_broadcast(&liveCond);
    printf("r : %d", r);
   return NULL;
}


void updateProductRating(const char* productName, double newRating, pthread_t callingThreadID, int storeIndex, int productIndex) {
    g_rating_sem = sem_open(SEM_RATING_UPDATE, O_CREAT, 0644, 1);
    if (g_rating_sem == SEM_FAILED) {
        perror("Semaphore creation failed for rating update");
        return;
    }

    char* filePath = findProductFilePath(productName);
    //printf("filepath: %s\n",filePath);
    if (!filePath) {
        printf("Product %s not found\n", productName);
        sem_close(g_rating_sem);
        //sem_unlink(SEM_RATING_UPDATE);
        return;
    }

    Product* product = readProductFromFile(filePath);
    if (!product) {
        printf("Failed to read product file\n");
        free(filePath);
        sem_close(g_rating_sem);
        //sem_unlink(SEM_RATING_UPDATE);
        return;
    }

    if(sem_wait(g_rating_sem) == -1){
        perror("wait fail, rating");
        free(filePath);
        free(product);
        sem_close(g_rating_sem);
        return;
    }

    //sem_wait(g_rating_sem);
    printf("old score is: %.2f\n", product->score);

    double oldScore = product->score;
    product->score = (oldScore + newRating) / 2.0;
    time_t now;
    time(&now);

    char formattedTime[20];
    strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%d %H:%M:%S", localtime(&now));

    FILE* file = fopen(filePath, "w");
    if (file) {
        fprintf(file, "Name: %s\n", product->name);
        fprintf(file, "Price: %.2f\n", product->price);
        fprintf(file, "Score: %.2f\n", product->score);
        fprintf(file, "Entity: %d\n", product->entity);
        fprintf(file, "Last Modified: %s\n", formattedTime);
        fclose(file);
        printf("Product rating updated successfully\n");
        printf("RATE: TID: %ld and PID: %d\n", pthread_self(), getpid());
        printf("new score is %.2f\n", product->score);

    } else {
        perror("Failed to update product file");
    }
    if(sem_post(g_rating_sem) == -1){
        perror("post fail, rating");
    }

    free(product);
    free(filePath);

    sem_close(g_rating_sem);
    //sem_unlink(SEM_RATING_UPDATE);
}

void* rateProducts(void* args) {
    sleep(3);
   UserShoppingList* shoppingList = (UserShoppingList*)args;

   printf("\nPlease rate the products you've purchased:\n");
  
   int selectedStore = -1;
   for(int i = 0; i < MAX_storeCount; i++){
       if(shoppingList->store_match_count[i] == 1){
           selectedStore = i;
           break;
       }
   }
   if(selectedStore == -1){
       printf("no store found for RATING\n");
       return NULL;
   }
   ProductThreadInfo threadInfos[MAX_PRODUCTS];
   int threadInfoCount = 0;

   for (int i = 0; i < shoppingList->productCount; i++) {
       double rating;
       if (shoppingList->products[selectedStore][i].foundFlag) {
           printf("Rate product %s (1-5 stars): ", shoppingList->products[selectedStore][i].name);
           scanf("%lf", &rating);
           while (rating < 1 || rating > 5) {
            printf("Invalid rating. Please enter a rating between 1 and 5: ");
               scanf("%lf", &rating);
           }
           /*threadInfos[threadInfoCount].threadID = pthread_self();
           strcpy(threadInfos[threadInfoCount].productName, shoppingList->products[selectedStore][i].name);
           threadInfos[threadInfoCount].storeIndex = selectedStore;
           threadInfos[threadInfoCount].productIndex = i;
           threadInfoCount++;*/

           updateProductRating(shoppingList->products[selectedStore][i].name, rating, pthread_self(),selectedStore,i);
       } 
   }
   return NULL;
}

void* searchProductInCategory(void* args){
    //printf("in thread with tid : %ld\n", pthread_self());
    threadInput *input = (threadInput *)args;
    UserShoppingList* shoppingList = input->shoppingList;
    char** proNames = input->names;
    sem_wait(g_search_sem);
    Product* product = readProductFromFile(input->filepath);
    sem_post(g_search_sem);
    for(int i = 0; i < input->proCount; i++){
        if (product && strcasecmp(product->name, proNames[i]) == 0){
            sem_wait(g_result_sem);
            printf("i found it in %s!!!!\n", input->filepath);
            printf("TID found: %ld\n",pthread_self());
            memcpy(input->product->name, product->name, sizeof(product->name));
            memcpy(input->product->lastModified, product->lastModified, sizeof(product->lastModified));
            input->product->price = product->price;
            input->product->score = product->score;
            input->product->entity = product->entity;
            input->product->foundFlag = 1;
            product->foundFlag = 1;
            input->proNum = i;
            sem_post(g_result_sem);
            memcpy(&(shoppingList->products[input->storeNum][i]), product, sizeof(Product));
            
            //pthread_mutex_lock(&liveLock);
            printf("after\n");
             //printf("stop : %d ", stopThread);
            while(!shoppingList->stopThread){
            }
            //pthread_cond_wait(&liveCond, &liveLock);
            
            printf("before\n");
            //pthread_mutex_unlock(&liveLock);
            printf("im done now\n");
        }
    }
    free(product);
    pthread_exit(NULL);
    return NULL;
}

void processCategories(int storeNum, const char* storePath, UserShoppingList* shoppingList) {
    pthread_t threads[1000];
    char** categories = getSubDirectories(storePath);
    char** productNames = malloc(shoppingList->productCount * sizeof(char*));
    for (int k = 0; k < shoppingList->productCount; k++) {
        printf("proName : %s\n", shoppingList->products[1][k].name);
        productNames[k] = malloc(strlen(shoppingList->products[1][k].name) + 1);
        strcpy(productNames[k], shoppingList->products[1][k].name);
    }
    int shmFd = shoppingList->shmFd;
    for (int i = 0; i < categoryCount; i++) {
        pid_t pidCategory = fork();
        if (pidCategory == 0) { // Child process
            // Remap shared memory
            //printf("im in categories\n");
            UserShoppingList *shoppingList = mmap(NULL, sizeof(UserShoppingList) * 10 + sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
            if (shoppingList == MAP_FAILED) {
                perror("mmap failed in child");
                exit(EXIT_FAILURE);
            }
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
                inputs[j]->shoppingList = shoppingList;
                inputs[j]->storeNum = storeNum;
                if (pthread_create(&threads[j], NULL, searchProductInCategory, (void*)inputs[j]) != 0) {
                    perror("pthread_create failed");
                    exit(EXIT_FAILURE);
                }
                j++;
            }
            for(long int i = 0; i < 9999999; i++){

            }
            for (int l = 0; l < j; l++) {
                pthread_join(threads[l], NULL);
                /*if (inputs[l]->product->foundFlag == 1) {
                    memcpy(&(shoppingList->products[storeNum][inputs[l]->proNum]), inputs[l]->product, sizeof(Product));
                }*/
    
                //free(inputs[l]->product);
                //free(inputs[l]);
            }
            sleep(1);
            munmap(shoppingList, sizeof(UserShoppingList) * 10 + sizeof(int));
            for(long int i = 0; i < 9999999; i++ ){

            }
            printf("category exting\n");
            exit(0);
        } else if (pidCategory < 0) {
            perror("Failed to fork for category\n");
        }
    }

    // Wait for all child processes
    for (int k = 0; k < categoryCount; k++) {
        wait(NULL);
    }

    // Free memory
    for (int k = 0; k < shoppingList->productCount; k++) {
        free(productNames[k]);
    }
    free(productNames);
    
    //sleep(2);
}


void processStores(UserShoppingList* shoppingList) {
    char** stores = getSubStoreDirectories("Dataset");
    int shmFd = shoppingList->shmFd; // File descriptor for shared memory
    //printf("budget from child : %f\n", shoppingList->budgetCap);
    for (int i = 0; i < storeCount; i++) {
        pid_t pidStore = fork();

        if (pidStore == 0) {  // Child process
            // Map shared memory in the child process
            UserShoppingList *mappedList = mmap(NULL, sizeof(UserShoppingList), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
            if (mappedList == MAP_FAILED) {
                perror("mmap failed in child");
                exit(EXIT_FAILURE);
            }

            // Debug print to verify shared memory access
            printf("Child process for store %d: budgetCap = %f\n", i + 1, mappedList->budgetCap);

            // Process the store categories
            stores[i][strcspn(stores[i], "\n")] = 0; // Remove trailing newline
            processCategories(i, stores[i], mappedList);

            // Cleanup in child process
            munmap(mappedList, sizeof(UserShoppingList)); // Unmap shared memory
            for(long int i = 0; i < 9999999; i++ ){

            }       
            printf("store exiting\n");

            exit(0); // Exit the child process
        }
        else if (pidStore < 0) {  // Fork failed
            perror("Failed to fork for store");
            break;
        } else {  // Parent process
            printf("Parent: forked child for store %d\n", i + 1);
        }
    }
    /*for(long long int i = 0; i < 999999999; i++){

    }*/
    // Wait for all child processes to complete
    int status;
    while (wait(&status) > 0); // Wait for any child process to finish
    // Free allocated memory for store names
    //sleep(2);
    for (int i = 0; i < storeCount; i++) {
        free(stores[i]);
    }
    free(stores);
    for(long int i = 0; i < 9999999; i++ ){

    }
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
   g_shopping_list_sem = sem_open(SEM_SHOPPING_LIST, O_CREAT, 0644, 1);
  
   if (g_search_sem == SEM_FAILED || g_result_sem == SEM_FAILED ||
       g_shopping_list_sem == SEM_FAILED) {
       perror("Semaphore creation failed");
       return;
   }
   
   pthread_t basketValueThread;
   pthread_t ratingThread;
   pthread_t finalListThread;
   pthread_create(&basketValueThread ,NULL, calculateStoreBaskettValue, (void*)shoppingList);

   pthread_create(&ratingThread, NULL, rateProducts, (void*)shoppingList);
    pthread_detach(basketValueThread);

   processStores(shoppingList);

    pthread_join(ratingThread, NULL);

   // printf("im starting");
    

    /*if(shoppingList->store_match_count[0] || shoppingList->store_match_count[1] ||
        shoppingList->store_match_count[2]){ // age mitonest bekhare asan

        pthread_create(&ratingThread, NULL, rateProducts, (void*)shoppingList);
        pthread_join(ratingThread, NULL);

        //pthread_create(&finalListThread, NULL, updateFinalShoppingList, shoppingList);
        //pthread_join(finalListThread, NULL);
    }*/

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
   sem_close(g_shopping_list_sem);

   sem_unlink(SEM_PRODUCT_SEARCH);
   sem_unlink(SEM_RESULT_UPDATE);
   sem_unlink(SEM_SHOPPING_LIST);
}

UserShoppingList read_user_shopping_list() {
    UserShoppingList shoppingList;
    memset(&shoppingList, 0, sizeof(UserShoppingList));
 
    printf("Enter User ID: ");
    scanf("%99s", shoppingList.userID);
 
    printf("Enter number of products: ");
    scanf("%d", &shoppingList.productCount);
    printf("Order list: \n");
    for (int i = 0; i < shoppingList.productCount; i++) {
        printf("Product %d Name: ", i + 1);
        scanf("%99s", shoppingList.products[1][i].name);
        printf("Product %d Quantity: ", i + 1);
        scanf("%d", &shoppingList.products[1][i].entity);
    }
    printf("Enter Budget Cap (-1 for no cap): ");
    scanf("%lf", &shoppingList.budgetCap);

    shoppingList.userPID = getpid();
    return shoppingList;
}

int main() {
    const char *shmName = "sharedShoppingList";
    const int shmSize = sizeof(UserShoppingList) * 10 + sizeof(int);
    int shmFd;

    shmFd = shm_open(shmName, O_CREAT | O_RDWR, 0666);
    if (shmFd == -1) {
        perror("shm_open failed");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shmFd, shmSize) == -1) {
        perror("ftruncate failed");
        exit(EXIT_FAILURE);
    }

    void *sharedMem = mmap(NULL, shmSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    if (sharedMem == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    memset(sharedMem, 0, shmSize); 
    shoppingList = (UserShoppingList *)sharedMem; 
    userCount = (int *)((char *)sharedMem + sizeof(UserShoppingList) *10); 

    while (1) {
        pid_t pid = fork(); 
        if (pid < 0) {
            perror("Failed to fork for User\n");
            break;
        } else if (pid == 0) {
            int currentUserIndex = *userCount;
            (userCount) -= (int*)(sizeof(UserShoppingList));

            UserShoppingList *currentUser = &shoppingList[currentUserIndex];
            *currentUser = read_user_shopping_list(); 
            currentUser->shmFd = shmFd;

            processUser(currentUser);
            exit(0); 
        } else {
            wait(NULL);
            if (*userCount >= 10) {
                printf("Maximum user count reached.\n");
                break;
            }
        }
    }

    printf("Exiting...\n");

    // Clean up shared memory
    munmap(sharedMem, shmSize);
    shm_unlink(shmName);

    return 0;
}

