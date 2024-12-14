// indexing , reading from file, updating file after purchase, semaphore, thread handling, calculating part


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
#include <time.h>


#define MAX_PRODUCTS 80
#define MAX_NAME_LEN 100
#define MAX_PATH_LEN 200
#define MAX_storeCount 3
#define categoryCount 8
#define maxUser 10
#define discountPercentage 0.1 // 10%


#define SEM_PRODUCT_SEARCH "/product_search_sem"
#define SEM_RESULT_UPDATE "/result_update_sem"
#define SEM_INVENTORY_UPDATE "/inventory_update_sem"
#define SEM_SHOPPING_LIST "/shopping_list_sem"


typedef struct {
    char userID[MAX_NAME_LEN];
    int buyingCount[MAX_storeCount];
    int hasDiscount[MAX_storeCount];
} UserMembership;


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
  UserMembership membership;
  int entity[MAX_PRODUCTS];
} UserShoppingList;


typedef struct {
  char *categoryAddress;
  char *name;
  Product *product;
} threadInput;


typedef struct {
    UserShoppingList* shoppingList;
    int bestStore;
    pthread_mutex_t* valuationMutex;
    pthread_mutex_t* ratingMutex;
    pthread_mutex_t* updateMutex;
} ThreadManagementData;


sem_t *g_search_sem = NULL;
sem_t *g_result_sem = NULL;
sem_t *g_inventory_sem = NULL;
sem_t *g_shopping_list_sem = NULL;


//define all functions
void processCategories(int storeNum, const char* storePath, UserShoppingList* shoppingList);
void processStroes(UserShoppingList* shoppingList);
void processUser(UserShoppingList* shoppingList);


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
      printf("Unable to open category dddirectory: %s\n", categoryPath);
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
              free(product);
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

char** getSubStoreDirectories(const char *dir){
  int count = 0;
  char **categories = malloc(sizeof(char *) * 1000);     
  char subDir[MAX_PATH_LEN], command[MAX_NAME_LEN];
  for(int i = 0; i < 3; i++){
    snprintf(command, sizeof(command), "find %s -name Store%d -type d", dir, i+1);
    command[strcspn(command, "\n")] = 0;
    printf("command is: %s\n",command);
    FILE *fp = popen((command), "r");
   if (!fp) {
      perror("Error opening files");
      if (fp) fclose(fp);
      return NULL;
  }
  //fgets(subDir, sizeof(subDir), fp);
  if(fgets(subDir, sizeof(subDir), fp)!=NULL){
      printf("hi : %s\n", subDir);
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

double calculateProductValue(Product* product){
  if(product->price <= 0){
      return 0;
  }
  printf("score is: %.2f and price is %.2f\n",product->score, product->price);
  return product->score * product->price;
}

double calculateTotalCost(UserShoppingList* shoppingList, int store){
   double totalCost = 0;
   for(int i = 0;i < shoppingList->productCount; i++){
       Product* product = &shoppingList->products[store][i];
       if(product->foundFlag){
           int productQuantity = shoppingList->entity[i];
           printf("price is %.2f and entity : %d and store %d\n",product->price, productQuantity, store);
           totalCost += product->price * productQuantity;
       }
   }
   return totalCost;
}

void* basketValuationThread(void* args) {
   ThreadManagementData* data = (ThreadManagementData*)args;
   UserShoppingList* shoppingList = data->shoppingList;
   int bestStore = data->bestStore;
  
   pthread_mutex_lock(data->valuationMutex);
  
   double totalBasketValue = 0;
   for (int i = 0; i < shoppingList->productCount; i++) {
       Product* product = &shoppingList->products[bestStore][i];
       if (product->foundFlag) {
           totalBasketValue += calculateProductValue(product) * product->entity;
       }
   }
  
   printf("tootal Basket Value: %.2f\n", totalBasketValue);
  
   pthread_mutex_unlock(data->valuationMutex);
   return NULL;
}

int findBestStore(UserShoppingList* shoppingList){
  int bestStore = -1;
  double bestStoreValue = -1;


  for (int i = 0; i < MAX_storeCount; i++) {
       double basketStoreValue = 0;
       int allProductFound = 1;


       for (int j = 0; j < shoppingList->productCount; j++) {
           Product* product = &shoppingList->products[i][j];


           if (!product->foundFlag) {
               allProductFound = 0;
               break;
           }
           basketStoreValue += calculateProductValue(product) * product->entity;
       }
       if(allProductFound && basketStoreValue > bestStoreValue){
           bestStoreValue = basketStoreValue;
           bestStore = i;
           break;
       }
      
       printf("Store %d: Basket Value = %.2f\n", i+1, bestStoreValue);
   }


   return bestStore;
}


void MemberShipDiscount(UserShoppingList* shoppingList){
    int bestStore = findBestStore(shoppingList);
    if (bestStore == -1){
        return;
    }

    if(shoppingList->membership.buyingCount > 0){
        for(int i = 0; i < shoppingList->productCount; i++){
            Product* product = &shoppingList->products[bestStore][i];
            if(product->foundFlag){
                product->price *= (1 - discountPercentage);
            }
        }
        printf("hehe discount at store %d\n", bestStore + 1);
    }
    shoppingList->membership.buyingCount[bestStore]++;
}


void* productRatingThread(void* args) {
    ThreadManagementData* data = (ThreadManagementData*)args;
    UserShoppingList* shoppingList = data->shoppingList;
    int bestStore = data->bestStore;
    
    pthread_mutex_lock(data->ratingMutex);
    
    // Re-rate products
    for (int i = 0; i < shoppingList->productCount; i++) {
        Product* product = &shoppingList->products[bestStore][i];
        if (product->foundFlag) {
            double userRating;
            printf("Rate product %s (0-5): ", product->name);
            scanf("%lf", &userRating);
            
            // Update product score
            product->score = (product->score + userRating) / 2.0;
            
            // Update product file with new score
            char filepath[MAX_PATH_LEN];
            snprintf(filepath, sizeof(filepath), "Dataset/Store%d/Category/%s.txt", 
                     bestStore + 1, product->name);
            
            FILE* file = fopen(filepath, "w");
            if (file) {
                fprintf(file, "Name: %s\n", product->name);
                fprintf(file, "Price: %.2f\n", product->price);
                fprintf(file, "Score: %.2f\n", product->score);
                fprintf(file, "Entity: %d\n", product->entity);
                
                time_t now;
                time(&now);
                char timestamp[50];
                strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
                fprintf(file, "Last Modified: %s\n", timestamp);
                
                fclose(file);
            }
        }
    }
pthread_mutex_unlock(data->ratingMutex);
    return NULL;
}


void* productListUpdateThread(void* args) {
    ThreadManagementData* data = (ThreadManagementData*)args;
    UserShoppingList* shoppingList = data->shoppingList;
    int bestStore = data->bestStore;
    
    pthread_mutex_lock(data->updateMutex);
    
    for (int i = 0; i < shoppingList->productCount; i++) {
        Product* product = &shoppingList->products[bestStore][i];
        if (product->foundFlag) {
            product->price *= (1 - discountPercentage);
        }
    }
    
    pthread_mutex_unlock(data->updateMutex);
    return NULL;
}

void UpdateRateProducts(UserShoppingList* shoppingList, int bestStore){
   pthread_t ratingThread[MAX_PRODUCTS];
   threadInput inputs[MAX_PRODUCTS];


   printf("**rate to your products**\n");


   for(int i = 0; i < shoppingList->productCount; i++){
       if(shoppingList->products[bestStore][i].foundFlag){
        char categoryPath[MAX_PATH_LEN];
        //snprintf(categoryPath, sizeof(categoryPath), "Dataset/Store%d/Category")
        pthread_create(&ratingThread[i], NULL, productRatingThread, &shoppingList->products[bestStore][i]);
       }
   }


   for(int i = 0; i < shoppingList->productCount; i++){
       if(shoppingList->products[bestStore][i].foundFlag){
           pthread_join(ratingThread[i], NULL);
       }
   }
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
  printf("pid: %d\n",shoppingList->userPID);
  return shoppingList;
}
void* searchProductInCategory(void* args){
    sem_wait(g_search_sem);

  DIR *dir;
  struct dirent *entry;
  char categoryPath[MAX_PATH_LEN], command[1000];
   threadInput *input = (threadInput *)args;
   //printf("input : %s %s\n",input-> input->name);
   snprintf(command, sizeof(command), "find %s -maxdepth 1 -type f", input->categoryAddress);
   FILE *fp = popen((command), "r");
   if (!fp) {
      perror("Error opening files");
      if (fp) fclose(fp);
      return NULL;
   }
   char filepath[MAX_PATH_LEN];
   fgets(filepath, sizeof(filepath), fp);
   while(fgets(filepath, sizeof(filepath), fp)!=NULL){
       filepath[strcspn(filepath, "\n")] = 0;
       Product* product = readProductFromFile(filepath);
       if (product && strcasecmp(product->name, input->name) == 0){


           sem_wait(g_result_sem);


           //printf("i found it in %s!!!!\n", filepath);
           memcpy(input->product->name, product->name, sizeof(product->name));
           memcpy(input->product->lastModified, product->lastModified, sizeof(product->lastModified));
           input->product->price = product->price;
           input->product->score = product->score;
           input->product->entity = product->entity;
           input->product->foundFlag = 1;


           sem_post(g_result_sem);


           pclose(fp);
           sem_post(g_search_sem);
           return NULL;  
       }
       free(product);
   }
   //printf("i finished the files????\n");            
   pclose(fp);
   sem_post(g_search_sem);
   return NULL;
}


int checkStoreInventory(UserShoppingList* shoppingList, int bestStore){
   for(int i = 0; i < shoppingList->productCount; i++){
       Product* product = &shoppingList->products[bestStore][i];


       if(!product->foundFlag){
           printf("not found in store %s", shoppingList->products[bestStore][i].name);
           return 0;
       }


       if(product->entity < shoppingList->products[bestStore][i].entity){
           printf("there isn't enought %s product", product->name);
           printf("There is in store %d", shoppingList->products[bestStore][i].entity, product->entity);
           return 0;
       }
   }
   return 1;
}



int checkBudgetConstraint(UserShoppingList* shoppingList, int bestStore) {
   double totalCost = calculateTotalCost(shoppingList, bestStore);
  
   if (shoppingList->budgetCap > 0 && totalCost > shoppingList->budgetCap) {
       printf("total cost: %.2f, your budgetCap: %.2f\n", totalCost, shoppingList->budgetCap);
       printf("you can't buy\n");
       return 0;
   }
   else{
       printf("total cost: %.2f\n", totalCost);
       return 0;
   }
  
   shoppingList->totalCost = totalCost;
   return 1;
}

void updateStoreInventory(UserShoppingList* shoppingList, int bestStore) {
    sem_wait(g_inventory_sem);


    char specificStorePath[MAX_PATH_LEN];
    FILE* inventoryFile;
    time_t now;
    time(&now);
    char timestamp[50];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));


    for (int i = 0; i < shoppingList->productCount; i++) {
        Product* product = &shoppingList->products[bestStore][i];
        if (product->foundFlag) {
            product->entity -= shoppingList->products[1][i].entity;
            snprintf(specificStorePath, sizeof(specificStorePath), 
                     "Dataset/Store%d/%s/%s.txt", 
                     bestStore + 1, product->name, product->name);
  
            inventoryFile = fopen(specificStorePath, "w");
            if (inventoryFile) {
                fprintf(inventoryFile, "Name: %s\n", product->name);
                fprintf(inventoryFile, "Price: %.2f\n", product->price);
                fprintf(inventoryFile, "Score: %.2f\n", product->score);
                fprintf(inventoryFile, "Entity: %d\n", product->entity);
                fprintf(inventoryFile, "Last Modified: %s\n", timestamp);
                
                strcpy(product->lastModified, timestamp);
                
                fclose(inventoryFile);
                printf("updated inventory for %s in Store %d\n", product->name, bestStore + 1);
            } else {
                printf("couldn't open file %s for updating\n", specificStorePath);
            }
        }
    }
    sem_post(g_inventory_sem);
}


void processCategories(int storeNum, const char* storePath, UserShoppingList* shoppingList){//making process for categories
  char** categories = getSubDirectories(storePath);

   for(int i = 0; i < categoryCount; i++){
       pthread_t threads[shoppingList->productCount];
       threadInput inputs[shoppingList->productCount];
       int totalCost = 0;
       pid_t pidCategory = vfork();
       if(pidCategory == 0){
           printf("processing category: %s\n",categories[i]);
           int proCount = shoppingList->productCount;
           //Product foundProduct[shoppingList->productCount];
           for(int j = 0; j < proCount; j++){
               char productFile[1000],categoryFile[1000];
               categories[i][strcspn(categories[i], "\n")] = 0;
               Product foundProduct[50];
               inputs[j].categoryAddress=categories[i];
               inputs[j].name = shoppingList->products[1][j].name;
               //printf("im in for order %d %s\n",j, shoppingList->products[1][j].name);
               inputs[j].product = &foundProduct[j];
              
               pthread_create(&threads[j], NULL, (&searchProductInCategory),(void*) &inputs[j]);
               if(foundProduct[j].foundFlag){ // found product in category store
                  //printf("store %d : flag : %s", storeNum, foundProduct.foundFlag);
                  printf("found product: %s in %s\n",shoppingList->products[1][j].name, categories[i]);
                  //foundProduct[j].entity = shoppingList->products[1][j].entity;
                  memcpy(&(shoppingList->products[storeNum][j]), &foundProduct[j], sizeof(Product));
               }
          }
           for(int j =0 ; j < shoppingList->productCount; j++){
               pthread_join(threads[j], NULL);
           } 
          exit(0);
      }else if(pidCategory < 0){
          perror("Failed to fork for category\n");
      }
  }
  for(int i = 0; i < categoryCount; i++){
       wait(NULL);
  }
  //printf("i finished the categories??????\n");
}

void processStores(UserShoppingList* shoppingList){ //making process for stores
    printf("im processing stores\n");
   char** stores = getSubStoreDirectories("Dataset");
   //sem_t sem;
   //sem_init(&sem, 1, 1);
  for(int i = 0; i < MAX_storeCount; i++){
      pid_t pidStore = vfork();
      if(pidStore == 0){
          printf("processing store: %s\n",stores[i]);
          stores[i][strcspn(stores[i], "\n")] = 0;
          processCategories(i, stores[i], shoppingList);
          exit(0);
      }
      else if(pidStore < 0){
          perror("Failed to fork for store\n");
      }
  }
  for(int i = 0; i < MAX_storeCount; i++){
      free(stores[i]);
  }
  free(stores);
  //sem_destroy(&sem);
}


void processUser(UserShoppingList* shoppingList){
  //semaphore
    /*g_search_sem = sem_open(SEM_PRODUCT_SEARCH, O_CREAT, 0644, 1);
    g_result_sem = sem_open(SEM_RESULT_UPDATE, O_CREAT, 0644, 1);
    g_inventory_sem = sem_open(SEM_INVENTORY_UPDATE, O_CREAT, 0644, 1);
    g_shopping_list_sem = sem_open(SEM_SHOPPING_LIST, O_CREAT, 0644, 1);




    if (g_search_sem == SEM_FAILED || g_result_sem == SEM_FAILED || 
        g_inventory_sem == SEM_FAILED || g_shopping_list_sem == SEM_FAILED) {
        perror("Semaphore creation failed");
        return;
    }*/


    //sem_wait(g_shopping_list_sem);


  // Process stores to find products
  processStores(shoppingList);


  /*int bestStore = findBestStore(shoppingList);


  if(bestStore != -1 && checkBudgetConstraint(shoppingList, bestStore) && checkStoreInventory(shoppingList, bestStore)){


    pthread_mutex_t valuationMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t ratingMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t updateMutex = PTHREAD_MUTEX_INITIALIZER;


    ThreadManagementData threadData = {
            .shoppingList = shoppingList,
            .bestStore = bestStore,
            .valuationMutex = &valuationMutex,
            .ratingMutex = &ratingMutex,
            .updateMutex = &updateMutex
        };




    pthread_t valuationThread, ratingThread, updateThread;
        
    pthread_create(&valuationThread, NULL, basketValuationThread, &threadData);
    pthread_create(&ratingThread, NULL, productRatingThread, &threadData);
    pthread_create(&updateThread, NULL, productListUpdateThread, &threadData);
    
    
    pthread_join(valuationThread, NULL);
    pthread_join(ratingThread, NULL);
    pthread_join(updateThread, NULL);
    
    
    pthread_mutex_destroy(&valuationMutex);
    pthread_mutex_destroy(&ratingMutex);
    pthread_mutex_destroy(&updateMutex);
    
    
    updateStoreInventory(shoppingList, bestStore);


     printf("puechase succesfull\n");
     printf("your total cost %.2f\n", shoppingList->totalCost);


     //updateStoreInventory(shoppingList, bestStore);
     MemberShipDiscount(shoppingList);
  }
  else{
    printf("you can't buy");
  }
*/

 // sem_post(g_shopping_list_sem);
  
  // Print processed products
   printf("\nProcessed Shopping List for User %s:\n", shoppingList->userID);
   for (int i = 0; i < shoppingList->productCount; i++) {
       for (int j = 0; j < MAX_storeCount; j++){
           if (shoppingList->products[j][i].foundFlag) {
               printf("store %d Product %d: %s (Price: %.2f, Score: %.2f, Entity: %d, UserEntity: %d)\n",
                       j+1,
                       i+1,
                       shoppingList->products[j][i].name,
                       shoppingList->products[j][i].price,
                       shoppingList->products[j][i].score,
                       shoppingList->products[j][i].entity,
                       shoppingList->entity[i]);
                       calculateProductValue(&shoppingList->products[j][i]);
           } else {
               printf("store %d Product %d: %s - Not Found\n",
                       j+1,
                       i+1,
                       shoppingList->products[j][i].name);
           }
       }
   }
/*



   if(bestStore != -1){
       printf("best store is: %d\n",bestStore+1);
   }


    sem_close(g_search_sem);
    sem_close(g_result_sem);
    sem_close(g_inventory_sem);
    sem_close(g_shopping_list_sem);
*/



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
           pthread_t threads[MAX_storeCount];
           UserShoppingList* shoppingList = read_user_shopping_list();
           //printf("bafore user process");
           processUser(shoppingList);
           free(shoppingList);
           exit(0);
      }
  }


  printf("Exiting...\n");
  return 0;
}