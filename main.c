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

#define FILESNUMBER 79
#define MAX_PRODUCTS 80
#define MAX_NAME_LEN 100
#define MAX_PATH_LEN 1000
#define storeCount 3
#define MAX_storeCount 3
#define categoryCount 3
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


typedef struct { //shared memory structure
   UserShoppingList users[maxUser];
   int activeUserCount;
} SharedMemoryData;


typedef struct {
    int proNum;
    int proCount;
   char *filepath;
   char **names;
   Product *product;
} threadInput;


SharedMemoryData* sharedData = NULL;

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
    printf("in thread with tid : %ld\n", pthread_self());
    threadInput *input = (threadInput *)args;
    char** proNames = input->names;
    Product* product = readProductFromFile(input->filepath);
    //printf("name : %s, %s, %s\n", product->name, input->filepath, proNames[0]);
    for(int i = 0; i < input->proCount; i++){
        printf("name : %s\n", product->name);
        if (product && strcasecmp(product->name, proNames[i]) == 0){
            printf("i found it in %s!!!!\n", input->filepath);
            memcpy(input->product->name, product->name, sizeof(product->name));
            memcpy(input->product->lastModified, product->lastModified, sizeof(product->lastModified));
            input->product->price = product->price;
            input->product->score = product->score;
            input->product->entity = product->entity;
            input->product->foundFlag = 1;
            input->proNum = i;
        }
    }
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

void processCategories(int storeNum, const char* storePath, UserShoppingList* shoppingList){
    pthread_t threads[1000];
    char** categories = getSubDirectories(storePath);
    int count = 0;
    char** productNames = malloc(shoppingList->productCount*sizeof(char*));
    for(int k=0; k < shoppingList->productCount; k++){
        productNames[k] = shoppingList->products[1][k].name;
    }
    for(int i = 0; i < categoryCount; i++){
        pid_t pidCategory = vfork();
        if(pidCategory==0){
            
            printf("category : %s\n", categories[i]);
            categories[i][strcspn(categories[i], "\n")] = 0;
            char** productFiles;
            productFiles = getsubfiles(categories[i]);
            //printf("file : %s\n", productFiles[0]);
            int numberF = sizeof(productFiles);
            printf("number : %d\n", numberF);
            threadInput inputs[numberF];
            for(int j =0; j<numberF; j++){
                if(strcmp(productFiles[j], "") != 0){
                    Product foundProduct[50];
                    strcpy(inputs[j].filepath,productFiles[j]);
                    inputs[j].proCount = shoppingList->productCount;
                    inputs[j].names = productNames;
                    inputs[j].product = &foundProduct[j];
                    pthread_create(&threads[j*categoryCount+i], NULL, (&searchProductInCategory),(void*) &inputs[j]);
                    if((inputs[j].product->foundFlag) == 1){ // found product in category store
                        //printf("store %d : flag : %s", storeNum, foundProduct.foundFlag);
                        printf("found product: %s in %s\n",shoppingList->products[1][j].name, categories[i]);
                        memcpy(&(shoppingList->products[storeNum][inputs[j].proNum]), &foundProduct[j], sizeof(Product));
                    }
    
                }
            }
            /*for (int k = 0; k < numberF; k++){
               free(productFiles[k]);
            }
            free(productFiles);
            */
            for(int j =0 ; j < numberF; j++){
                pthread_join(threads[j], NULL);
            }  
            exit(0);
            
        }else if(pidCategory < 0){
           perror("Failed to fork for category\n");
       }
    }
    //free(productNames);
    //free(categories);
    
}

/*void processCategories(int storeNum, const char* storePath, UserShoppingList* shoppingList){//making process for categories
   char** categories = getSubDirectories(storePath);

    pthread_t threads[categoryCount*shoppingList->productCount];

    for(int i = 0; i < categoryCount; i++){
        threadInput inputs[PRO_COUNT];
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
                inputs[j].fileAddress=categories[i];
                inputs[j].name = shoppingList->products[1][j].name;
                printf("im in for order %d %s\n",j, shoppingList->products[1][j].name);
                inputs[j].product = &foundProduct[j];
                
                pthread_create(&threads[j*categoryCount+i], NULL, (&readProductFromFile),(void*) &inputs[j]);
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
*/

void processStores(UserShoppingList* shoppingList){ //making process for stores
    char** stores = getSubStoreDirectories("Dataset");
    sem_t sem;
    sem_init(&sem, 1, 1);
   for(int i = 0; i < storeCount; i++){
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
   for(int i = 0; i < storeCount; i++){
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


   // Print processed products
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
            pthread_t threads[storeCount];
            UserShoppingList* shoppingList = read_user_shopping_list();
            processUser(shoppingList);
                // show user its choices
            //pthread_create(&threads[1], NULL, &getProductsValue, shoppingList->products);
            free(shoppingList);
            exit(0);
       }
   }


   printf("Exiting...\n");
   //detach and remove shared memory
   //shmdt(sharedData);
   //shmctl(shmID, IPC_RMID, NULL);
   return 0;
}