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

#define MAX_PRODUCTS 80
#define MAX_NAME_LEN 100
#define MAX_PATH_LEN 200
#define MAX_storeCount 3
#define categoryCount 8
#define maxUser 10
#define discountPercentage 0.1 // 10%

#define SEM_PRODUCT_SEARCH "/product_search_sem"
#define SEM_RESULT_UPDATE "/result_update_sem"

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
} UserShoppingList;

typedef struct {
  char *categoryAddress;
  char *name;
  Product *product;
} threadInput;

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

double calculateProductValue(Product* product){
   if(product->price <= 0){
       return 0;
   }
   return product->score * product->price;
}

double calculateTotalValue(Product products[], int productCount, int storeIn){
   double totalValue = 0;
   for(int i = 0; i < productCount; i++){
       if(products[i].foundFlag){
           totalValue += calculateProductValue(&products[i]);
       }
   }
   return totalValue;
}

int findBestStore(UserShoppingList* shoppingList){
   int bestStore = -1;
   double bestBasketValue = -1;

   for(int i = 0; i < MAX_storeCount; i++){
       double BasketValue = calculateTotalValue(shoppingList->products[i],shoppingList->productCount,i);

       double totalCost = 0;
       for(int j = 0; j < shoppingList->productCount; j++){
           if(shoppingList->products[i][j].foundFlag){
               totalCost += shoppingList->products[i][j].price * shoppingList->products[i][j].entity;
           }
       }

       if(BasketValue > bestBasketValue){
           bestBasketValue = BasketValue;
           bestStore = i;
       }
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

void* rateProductThread(void* args){
    threadInput* input = (threadInput*)args;
   Product* product = input->product;
   double userRate;

   printf("rate to %s product (0-5)", product->name);
   scanf("%lf",&userRate);

   product->score = (product->score + userRate) / 2.0;

   char filepath[MAX_PATH_LEN];
   snprintf(filepath, sizeof(filepath), "%s/%s.txt", input->categoryAddress, product->name);

   FILE* file = fopen(filepath, "W");
   if(file){
    fprintf(file, "Name: %s\n", product->name);
    fprintf(file, "Price: %s\n", product->price);
    fprintf(file, "Score: %s\n", product->score);
    fprintf(file, "Entity: %s\n", product->entity);
    fprintf(file, "last modified %s\n", product->lastModified);
    fclose(file);
   }

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
        pthread_create(&ratingThread[i], NULL, rateProductThread, &shoppingList->products[bestStore][i]);
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
      scanf("%d", &shoppingList->products[1][i].entity);
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

void* searchProductInCategory(void* args){
  //return NULL;
  // neeed to implement
  printf("in thread\n");
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
           printf("i found it in %s!!!!\n", filepath);
           memcpy(input->product->name, product->name, sizeof(product->name));
           memcpy(input->product->lastModified, product->lastModified, sizeof(product->lastModified));
           input->product->price = product->price;
           input->product->score = product->score;
           input->product->entity = product->entity;
           input->product->foundFlag = 1;
           pclose(fp);
           return NULL;  
       }
   }
   printf("i finished the files????\n");            
   pclose(fp);
   return NULL;
}

int checkStoreInventory(UserShoppingList* shoppingList, int bestStore){
    for(int i = 0; i < shoppingList->productCount; i++){
        Product* Product = &shoppingList->products[bestStore][i];

        if(!product->foundFlag){
            printf("not found in store %s", shoppingList->products[1][i].name);
            return 0;
        }

        if(product->entity < shoppingList->products[1][i].entity){
            printf("there isn't enought %s product", product->name);
            printf("There is in store %d", shoppingList->products[1][i].entity, Product->entity);
            return 0;
        }
    }
    return 1;
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
               printf("im in for order %d %s\n",j, shoppingList->products[1][j].name);
               inputs[j].product = &foundProduct[j];
              
               pthread_create(&threads[j], NULL, (&searchProductInCategory),(void*) &inputs[j]);
               if((foundProduct[j].foundFlag) == 1){ // found product in category store
                  //printf("store %d : flag : %s", storeNum, foundProduct.foundFlag);
                  printf("found product: %s in %s\n",shoppingList->products[1][j].name, categories[i]);
                  memcpy(&(shoppingList->products[storeNum][j]), &foundProduct[j], sizeof(Product));
                   //lock the critical secton
                   //sem_wait(&sem);
                   // found product ro brizim too shared memory (critical section)
                   //unlock the critical section
                  //break;
               }
              //if(foundProduct.foundFlag == 1) break;
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
  printf("i finished the categories??????\n");
}


void processStores(UserShoppingList* shoppingList){ //making process for stores
   char** stores = getSubDirectories("Dataset");
   sem_t sem;
   sem_init(&sem, 1, 1);
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
  sem_destroy(&sem);
}

void processUser(UserShoppingList* shoppingList){
  //semaphore
  sem_unlink(SEM_PRODUCT_SEARCH);
  sem_unlink(SEM_RESULT_UPDATE);
   sem_t* search_sem = sem_open(SEM_PRODUCT_SEARCH, O_CREAT, 0644, 1);
  sem_t* result_sem = sem_open(SEM_RESULT_UPDATE, O_CREAT, 0644, 1);
   if (search_sem == SEM_FAILED || result_sem == SEM_FAILED) {
      perror("Semaphore creation failed");
      return;
  }

  // Process stores to find products
  processStores(shoppingList);

  int bestStore = findBestStore(shoppingList);

  MemberShipDiscount(shoppingList);
  // Print processed products
   printf("\nProcessed Shopping List for User %s:\n", shoppingList->userID);
   for (int i = 0; i < shoppingList->productCount; i++) {
       for (int j = 0; j < MAX_storeCount; j++){
           if (shoppingList->products[j][i].foundFlag) {
               printf("store %d Product %d: %s (Price: %.2f, Score: %.2f, Entity: %d)\n",
                       j+1,
                       i+1,
                       shoppingList->products[j][i].name,
                       shoppingList->products[j][i].price,
                       shoppingList->products[j][i].score,
                       shoppingList->products[j][i].entity);
                       calculateProductValue(&shoppingList->products[j][i]);
           } else {
               printf("store %d Product %d: %s - Not Found\n",
                       j+1,
                       i+1,
                       shoppingList->products[j][i].name);
           }
       }
   }


   if(bestStore != -1){
       printf("best store is: %d\n",bestStore+1);
   }
  sem_close(search_sem);
  sem_close(result_sem);
  sem_unlink(SEM_PRODUCT_SEARCH);
  sem_unlink(SEM_RESULT_UPDATE);
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




