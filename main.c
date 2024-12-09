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
#include "OS-multithread-shopping/CreatCategoryProccess.c"


#define MAX_PRODUCTS 80
#define MAX_NAME_LEN 100
#define MAX_PATH_LEN 1000
#define storeCount 3
#define categoryCount 8
#define maxUser 10


#define SEM_PRODUCT_SEARCH "/product_search_sem"
#define SEM_RESULT_UPDATE "/result_update_sem"


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
   Product products[MAX_PRODUCTS];
   int productCount;
   double budgetCap; // if enter no budget cap, set it -1
   double totalCost;
   int store_match_count[storeCount];
   pid_t userPID;
   int processingComplete;
} UserShoppingList;


typedef struct { //shared memory structure
   UserShoppingList users[maxUser];
   int activeUserCount;
} SharedMemoryData;



typedef struct {
   char *categoryAddress;
   char *name;
   Product *product;
} threadInput;



SharedMemoryData* sharedData = NULL;

//define all functions
void processCategories(const char* storePath, UserShoppingList* shoppingList);
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


UserShoppingList* read_user_shopping_list() {
   UserShoppingList* shoppingList = malloc(sizeof(UserShoppingList));
   memset(shoppingList, 0, sizeof(UserShoppingList));
  
   printf("Enter User ID: ");
   scanf("%99s", shoppingList->userID);
  
  
   printf("Enter number of products: ");
   scanf("%d", &shoppingList->productCount);

   printf("Order list: \n");
   for (int i = 0; i < shoppingList->productCount; i++) {
       printf("Product %d Name: ", i + 1);
       scanf("%99s", shoppingList->products[i].name);
       printf("Product %d Quantity: ", i + 1);
       scanf("%d", &shoppingList->products[i].entity);
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
       printf("Unable to open store directory: %s\n", storePath);
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


int initializeSharedMemory() {
   int shmid = shmget(SHM_KEY, sizeof(SharedMemoryData), IPC_CREAT | 0666);
   if (shmid == -1) {
       perror("shmget failed");
       return -1;
   }


   sharedData = (SharedMemoryData*)shmat(shmid, NULL, 0);
   if (sharedData == (void*) -1) {
       perror("shmat failed");
       return -1;
   }


   // Initialize shared memory
   memset(sharedData, 0, sizeof(SharedMemoryData));
   return shmid;
}

char** getSubDirectories(char dir[1000]){
   int count = 0;
   char **categories = malloc(sizeof(char *) * 1000);      
   char subDir[1000], command[1000];
   snprintf(command, sizeof(command), "find %s -maxdepth 1 -type d", dir);
   FILE *fp = popen((command), "r");
  
   if (!fp) {
       perror("Error opening files");
       if (fp) fclose(fp);
       return NULL;
   }
   fgets(subDir, sizeof(subDir), fp);
   while(fgets(subDir, sizeof(subDir), fp)!=NULL){
       //printf("subDIR stores : %s", subDir);
       categories[count] = strdup(subDir);
       count++;                
   }
   pclose(fp);
   return categories;
}

/*Product* searchProductInCategory(const char* categoryPath, const char* productName){
   //return NULL;
   // neeed to implement
   DIR *dir;
   struct dirent *entry;
    
   dir = opendir(categoryPath);
   if (dir == NULL) {
       printf("Unable to open category directory: %s\n", categoryPath);
       return NULL;
   }
  
   while ((entry = readdir(dir)) != NULL) {
       if (entry->d_type == DT_REG) {  // If it's a regular file
           char filepath[MAX_PATH_LEN];
           snprintf(filepath, sizeof(filepath), "%s/%s", categoryPath, entry->d_name);
           Product* product = readProductFromFile(filepath);
           if (product && strcasecmp(product->name, productName) == 0) {
               closedir(dir);
               return product;
           }
           if (product) free(product);
       }
   }
  
   closedir(dir);
   return NULL;


}
*/


void* searchProductInCategory(void* args){
    printf("hi\n");
    threadInput *input = (threadInput *)args;
   printf("productName : %s\n", input->name);
   //printf("name : %s", input->product->name);
   memcpy(input->product->name, "bahar", 5);
   pthread_exit(NULL);

   //DIR *dir;
   //struct dirent *entry;
   //dir = opendir(input->categoryAddress);
}
/*void *searchProductInCategory(void* args){
   //return NULL;
   // neeed to implement
   threadInput *input = (threadInput *)args;
   printf("productName : %s\n", *(input->name));
   DIR *dir;
   struct dirent *entry;
   //char categoryPath[MAX_PATH_LEN] = input->categoryAddress;
   //char productName[MAX_NAME_LEN] = input->product;
   
   dir = opendir(input->categoryAddress);
   /*if (dir == NULL) {
       printf("Unable to open category directory: %s\n", input->categoryAddress);
       return;
   }
  
   while ((entry = readdir(dir)) != NULL) {
       if (entry->d_type == DT_REG) {  // If it's a regular file
           char filepath[MAX_PATH_LEN];
           snprintf(filepath, sizeof(filepath), "%s/%s", input->categoryAddress, entry->d_name);
           Product* product = readProductFromFile(filepath);
           if (product && strcasecmp(product->name, input->name) == 0) {
               closedir(dir);
               input->product = product;
           }
           if (product) free(product);
       }
   }
   closedir(dir);
    memcpy(input->product->name, "bahar", 5);
    pthread_exit(NULL);

   closedir(dir);
   //return NULL;
}
*/
void processCategories(const char* storePath, UserShoppingList* shoppingList){//making process for categories
   char** categories = getSubDirectories(storePath);
   for(int i = 0; i < categoryCount; i++){
       pid_t pidCategory = vfork();
       if(pidCategory == 0){

            pthread_t threads[3];
            printf("processing category: %s\n",categories[i]);
            Product foundProduct;
            
            for(int j = 0; j < 3; j++){
               pthread_t thread_id;
               threads[j] = thread_i
               d;
               char productFile[1000];
               categories[i][strcspn(categories[i], "\n")] = 0;
               threadInput input = {"bahar",  "Dataset", &foundProduct};
               printf("hiooooo\n");
        
                pthread_create(&thread_id, NULL, (&searchProductInCategory),(void*) &input);
                //Product* foundProduct = searchProductInCategory(categories[i], shoppingList->products[j].name);
               /*if(foundProduct){ // found product in category store
                   printf("found product: %s in %s\n",shoppingList->products[j].name, categories[i]);
                   shoppingList->products[j].foundFlag = 1;
                   memcpy(&shoppingList->products[j], foundProduct, sizeof(Product));
                
                   //free(foundProduct);
                   // hala ke peyda shod mitone bekhare
                   // badan piadesazi beshe
               }*/
           }
           for(int i =0 ; i < 3; i++){
                pthread_join(threads[i], NULL);
                printf("finish\n");
            }
            printf("name : %s", foundProduct.name);

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

void processStores(UserShoppingList* shoppingList){ //making process for stores
   char** stores = getSubDirectories("Dataset");
   for(int i = 0; i < storeCount; i++){
       pid_t pidStore = vfork();
       if(pidStore == 0){
           printf("processing store: %s\n",stores[i]);
           stores[i][strcspn(stores[i], "\n")] = 0;
           processCategories(stores[i],shoppingList);
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


   // Print processed products
   printf("\nProcessed Shopping List for User %s:\n", shoppingList->userID);
   for (int i = 0; i < shoppingList->productCount; i++) {
       if (shoppingList->products[i].foundFlag) {
           printf("Product %d: %s (Price: %.2f, Score: %.2f, Entity: %d)\n",
                  i+1,
                  shoppingList->products[i].name,
                  shoppingList->products[i].price,
                  shoppingList->products[i].score,
                  shoppingList->products[i].entity);
       } else {
           printf("Product %d: %s - Not Found\n",
                  i+1,
                  shoppingList->products[i].name);
       }
   }


   // Clean up semaphores
   sem_close(search_sem);
   sem_close(result_sem);
   sem_unlink(SEM_PRODUCT_SEARCH);
   sem_unlink(SEM_RESULT_UPDATE);


}


int main(){
    pthread_t threads[3];
    Product foundProduct;

    for (int i =0 ; i < 3; i++){
        pthread_t thread_id;
        threadInput input = {"bahar",  "Dataset", &foundProduct};
        printf("hi\n");
        threads[i] = thread_id;
        pthread_create(&thread_id, NULL, (&searchProductInCategory),(void*) &input);
    }
    for(int i =0 ; i < 3; i++){
        pthread_join(threads[i], NULL);
        printf("finish\n");
    }

    printf("name : %s", foundProduct.name);

   /*int shmID = initializeSharedMemory();
   if(shmID == -1){
       return 1;
   }


   while (1) {
       pid_t pidUser = vfork(); //process user


       if(pidUser < 0){
           perror("Failed to fork for User\n");
           break;
       }
       else if(pidUser == 0){
           UserShoppingList* shoppingList = read_user_shopping_list();
           processUser(shoppingList);
           free(shoppingList);
           exit(0);
       }
   }


   printf("Exiting...\n");
   //detach and remove shared memory
   shmdt(sharedData);
   //shmctl(shmID, IPC_RMID, NULL);
*/   return 0;
}
