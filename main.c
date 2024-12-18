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
#include <sys/file.h>

#define FILESNUMBER 79
#define MAX_PRODUCTS 80
#define MAX_NAME_LEN 100
#define MAX_PATH_LEN 1000
#define storeCount 3
#define MAX_storeCount 3
#define categoryCount 8
#define maxUser 10
#define MAX_THREAD 100

#define SEM_PRODUCT_SEARCH "/product_search_sem"
#define SEM_RESULT_UPDATE "/result_update_sem"
#define SEM_RATING_UPDATE "/rating_update_sem"
#define SEM_SHOPPING_LIST "/shopping_list_sem"
#define FILE_ADDRESS "users.txt"

#define SHM_KEY 0x1234

typedef enum{
    THREAD_SEARCHING,
    THREAD_FOUND,
    THREAD_COMPLETED
} ThreadState;

typedef struct {
  char name[MAX_NAME_LEN];
  double price;
  double score;
  int entity;
  int originEntity;
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
 int purchaseCount[MAX_storeCount];
 bool hasDiscount[MAX_storeCount];
 int entity[MAX_PRODUCTS];
 int shmFd;
 bool stopThread;
  bool stopFork;
 pthread_mutex_t *mutex;
 pthread_cond_t *cond;
} UserShoppingList;

typedef struct {
   int proNum;
   int proCount;
  char *filepath;
  char **names;
  Product *product;
  UserShoppingList* shoppingList;
  int storeNum;
  ThreadState threadState;
  pthread_mutex_t stateMutex;
  pthread_cond_t stateCond;
  int* orderID;
} threadInput;

typedef struct {
    pthread_t items[100];
    int front;
    int rear;
} Queue;

// Function to initialize the queue
void initializeQueue(Queue* q)
{
    q->front = -1;
    q->rear = 0;
}

// Function to check if the queue is empty
bool isEmpty(Queue* q) { return (q->front == q->rear - 1); }

// Function to check if the queue is full
bool isFull(Queue* q) { return (q->rear == 100); }

// Function to add an element to the queue (Enqueue
// operation)
void enqueue(Queue* q, int value)
{
    if (isFull(q)) {
        printf("Queue is full\n");
        return;
    }
    q->items[q->rear] = value;
    q->rear++;
}

// Function to remove an element from the queue (Dequeue
// operation)
void dequeue(Queue* q)
{
    if (isEmpty(q)) {
        printf("Queue is empty\n");
        return;
    }
    q->front++;
}

// Function to get the element at the front of the queue
// (Peek operation)
int peek(Queue* q)
{
    if (isEmpty(q)) {
        printf("Queue is empty\n");
        return -1; // return some default value or handle
                   // error differently
    }
    return q->items[q->front + 1];
}

sem_t *g_search_sem = NULL;
sem_t *g_result_sem = NULL;
sem_t *g_rating_sem = NULL;
sem_t *g_shopping_list_sem = NULL;
sem_t start_threads_sem;


UserShoppingList* shoppingList;
int *userCount = 0;
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

void write_user_store_to_file(const char *fileAddress, char* userID, int storeNum) { // Open the file in append mode to add data without overwriting existing content 
    FILE *file = fopen(fileAddress, "a"); 
    if (file == NULL) { 
        perror("Failed to open file for writing"); 
        return; 
    } // Write the UserId and StoreNum to the file 
    fprintf(file, "UserId: %s , StoreNum: %d\n", userID, storeNum); 
    // Close the file after writing 
    fclose(file); 
    printf("Successfully wrote UserId: %s , StoreNum: %d to %s\n", userID, storeNum, fileAddress);
}  

bool check_user_store_in_file(const char *filePath, char* userID, int storeNum) {
    FILE *file = fopen(filePath, "r");
    if (file == NULL) {
        perror("Failed to open file");
        return false;
    }

    char line[256]; // Buffer to hold each line from the file
    while (fgets(line, sizeof(line), file)) {
        char fileUserID[1000];
        int fileStoreNum = 0;

        // Parse the line to extract UserId and StoreNum
        if (sscanf(line, "UserId: %s , StoreNum: %d", fileUserID, &fileStoreNum) == 2) {
            // Compare with the provided UserID and StoreNum
            if (strcmp(fileUserID, userID) ==0 && fileStoreNum == storeNum) {
                fclose(file); // Close the file before returning
                return true;  // Match found
            }
        }
    }

    fclose(file); // Close the file after reading all lines
    return false; // No match found
}


void* calculateStoreBaskettValue(void* args){
    sem_wait(&start_threads_sem);
    printf("in calculating: TID: %ld and PID: %d\n", pthread_self(), getpid());
    //printf("im in\n");
    //sleep(3);
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

       sem_post(g_shopping_list_sem);
   }
   printf("best store is: %d\n",bestStore);
    pthread_mutex_lock(shoppingList->mutex);
    shoppingList->stopThread = false;
    pthread_mutex_unlock(shoppingList->mutex);
    pthread_cond_broadcast(shoppingList->cond);
    if(check_user_store_in_file(FILE_ADDRESS, shoppingList->userID, bestStore)){
    printf("wow , good for you ! youll get discount from us!");
    shoppingList->totalCost = shoppingList->totalCost*0.9;
    }
    //printf("userId : %s\n", shoppingList->userID);
    write_user_store_to_file(FILE_ADDRESS, shoppingList->userID, bestStore);

    pthread_exit(NULL);
   return NULL;
}


void updateProductRating(int newEntity, const char* productName, double newRating, pthread_t callingThreadID, int storeIndex, int productIndex) {
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
        fprintf(file, "Entity: %d\n", newEntity);
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
    sem_wait(&start_threads_sem);
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
  for (int i = 0; i < shoppingList->productCount; i++) {
      double rating;
      if (shoppingList->products[selectedStore][i].foundFlag) {
          printf("Rate product %s (1-5 stars): ", shoppingList->products[selectedStore][i].name);
          scanf("%lf", &rating);
          while (rating < 1 || rating > 5) {
           printf("Invalid rating. Please enter a rating between 1 and 5: ");
              scanf("%lf", &rating);
          }
          int newEntity = shoppingList->products[selectedStore][i].entity - shoppingList->entity[i];
          updateProductRating(newEntity ,shoppingList->products[selectedStore][i].name,rating,pthread_self() ,selectedStore,i);
      }
  }
  pthread_exit(NULL);
  return NULL;
}


int getNextOrderID(const char* storePath, const char* categoryPath, const char* userID) {
   char logDir[MAX_PATH_LEN];
   snprintf(logDir, sizeof(logDir), "%s/logs", categoryPath);
  
   // Ensure log directory exists
   mkdir(logDir, 0777);
  
   // Create a lock file to ensure thread-safe incrementation
   char lockFilePath[MAX_PATH_LEN];
   snprintf(lockFilePath, sizeof(lockFilePath), "%s/orderid_lock", logDir);
  
   int lockFd = open(lockFilePath, O_CREAT | O_RDWR, 0666);
   if (lockFd == -1) {
       perror("Failed to create lock file");
       return 1;  // Default to 1 if lock fails
   }
  
   // Acquire an exclusive lock
   if (flock(lockFd, LOCK_EX) == -1) {
       perror("Failed to acquire lock");
       close(lockFd);
       return 1;
   }
  
   int maxOrderID = 0;
   DIR *dir;
   struct dirent *entry;
  
   // Open the directory
   dir = opendir(logDir);
   if (dir != NULL) {
       while ((entry = readdir(dir)) != NULL) {
           // Check if the filename starts with the userID
           if (strncmp(entry->d_name, userID, strlen(userID)) == 0) {
               char *underscore = strrchr(entry->d_name, '_');
               if (underscore) {
                   int currentOrderID = atoi(underscore + 1);
                   if (currentOrderID > maxOrderID) {
                       maxOrderID = currentOrderID;
                   }
               }
           }
       }
       closedir(dir);
   }
  
   // Release the lock
   if (flock(lockFd, LOCK_UN) == -1) {
       perror("Failed to release lock");
   }
   close(lockFd);
  
   // Return the next order ID (increment by 1)
   return maxOrderID + 1;
}

void createCategoryLogFile(const char* storePath, const char* categoryPath, const char* userID, int* orderID){
   *orderID = getNextOrderID(storePath, categoryPath, userID);
  char logDir[MAX_PATH_LEN];
  snprintf(logDir, sizeof(logDir), "%s/logs", categoryPath);
  mkdir(logDir, 0777);
  char logFileName[MAX_PATH_LEN];
  snprintf(logFileName, sizeof(logFileName), "%s/%s_%d.log",logDir,userID,*orderID);


  FILE* logFile = fopen(logFileName, "w");
  if(!logFile){
      perror("fail to create logfile");
      return;
  }
  fclose(logFile);
}

void writeToLogFile(const char* categoryPath, const char* userID, int orderID, const char* message){
  char logDir[MAX_PATH_LEN];
  snprintf(logDir, sizeof(logDir), "%s/logs",categoryPath);


  char logFileName[MAX_PATH_LEN];
  snprintf(logFileName, sizeof(logFileName), "%s/%s_%d.log", logDir, userID, orderID);


  FILE* logFile = fopen(logFileName, "a");
  if(!logFile){
      perror("can't open log file");
      return;
  }
  fprintf(logFile, "%s\n", message);
  fclose(logFile);
}

void* searchProductInCategory(void* args){
   threadInput *input = (threadInput *)args;
   UserShoppingList* shoppingList = input->shoppingList;
   char** proNames = input->names;
   int orderID = 0;
    /*pthread_mutex_lock(&input->stateMutex);
    input->threadState = THREAD_SEARCHING;
    pthread_mutex_unlock(&input->stateMutex);*/
 //printf("storepath %s", input->filepath);
   //sem_wait(g_search_sem);
   Product* product = readProductFromFile(input->filepath);
   //sem_post(g_search_sem);
 //printf("storepath %s");
   char storePath[MAX_PATH_LEN];
   const char *datasetPath = strstr(input->filepath, "Dataset/");
   if(datasetPath){
    const char *firstSlash = strchr(datasetPath + strlen("Dataset/"),'/');
    if(firstSlash){
        strncpy(storePath, input->filepath, firstSlash - input->filepath);
        storePath[firstSlash - input->filepath] = '\0';
    }
   }
   //printf("storepath %s", storePath);
   char filename[MAX_PATH_LEN];
   const char *lastSlash = strrchr(input->filepath, '/');
    if(lastSlash){
    size_t pathLenght = lastSlash - input->filepath;
    strncpy(filename ,input->filepath, pathLenght);
    filename[pathLenght] = '\0';
    }
    //orderID = getNextOrderID(storePath, filename, shoppingList->userID);
    //printf("\ninput filepath: %s\n", filename);
   for(int i = 0; i < input->proCount; i++){
       if (product && strcasecmp(product->name, proNames[i]) == 0){
           sem_wait(g_result_sem);
           
           char threadLogMsg[MAX_PATH_LEN];
           snprintf(threadLogMsg, sizeof(threadLogMsg), "TID: %ld searching file: %s | Product: %s | Status: FOUND",
           pthread_self(), input->filepath, proNames[i]);
           writeToLogFile(filename, shoppingList->userID, *input->orderID, threadLogMsg);

           printf("i found it in %s!!!!\n", input->filepath);
           printf("TID found: %ld\n",pthread_self());
           printf("ordeID %d\n",*input->orderID);

           memcpy(input->product->name, product->name, sizeof(product->name));
            memcpy(input->product->lastModified, product->lastModified, sizeof(product->lastModified));
            input->product->price = product->price;
            input->product->score = product->score;
            input->product->entity = product->entity;
            input->product->foundFlag = 1;
            input->proNum = i;
            sem_post(g_result_sem);

            memcpy(&(shoppingList->products[input->storeNum][i]), input->product, sizeof(Product));

            /*pthread_mutex_lock(&input->stateMutex);
            input->threadState = THREAD_FOUND;
            pthread_cond_broadcast(&input->stateCond);
            pthread_mutex_unlock(&input->stateMutex);

            pthread_mutex_lock(&input->stateMutex);
            while(input->threadState != THREAD_COMPLETED){
                pthread_cond_wait(&input->stateCond, &input->stateMutex);
            }
            pthread_mutex_unlock(&input->stateMutex);*/
            printf("im waiting\n");
            pthread_mutex_lock(shoppingList->mutex);
            while(stopThread){
                pthread_cond_wait(shoppingList->cond, shoppingList->mutex);
            }
            pthread_mutex_unlock(shoppingList->mutex);
            printf("im done\n");
            if(shoppingList->store_match_count[input->storeNum] == 1){
                shoppingList->stopFork = false;
            }
       }
       else{
        char failedSreachMsg[MAX_PATH_LEN];
        snprintf(failedSreachMsg, sizeof(failedSreachMsg), "TID: %ld searching file: %s | Product: %s | Status: NOT FOUND",
        pthread_self(),input->filepath, proNames[i]);
        writeToLogFile(filename, shoppingList->userID, *input->orderID, failedSreachMsg);
       }
   }
   free(product);

   /*pthread_mutex_lock(&input->stateMutex);
   input->threadState = THREAD_COMPLETED;
   pthread_cond_broadcast(&input->stateCond);
   pthread_mutex_unlock(&input->stateMutex);*/
   //pthread_exit(NULL);
   return NULL;
}


void processCategories(int storeNum, const char* storePath, UserShoppingList* shoppingList) {

   char** categories = getSubDirectories(storePath);
   char** productNames = malloc(shoppingList->productCount * sizeof(char*));
   for (int k = 0; k < shoppingList->productCount; k++) {
       //printf("proName : %s\n", shoppingList->products[1][k].name);
       productNames[k] = malloc(strlen(shoppingList->products[1][k].name) + 1);
       strcpy(productNames[k], shoppingList->products[1][k].name);
   }
   int shmFd = shoppingList->shmFd;
   int orderID = 0;
   for (int i = 0; i < categoryCount; i++) {
       pid_t pidCategory = fork();
       if (pidCategory == 0) {
           pthread_t threads[10000];
           UserShoppingList *shoppingList = mmap(NULL, sizeof(UserShoppingList) * 10 + sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
           if (shoppingList == MAP_FAILED) {
               perror("mmap failed in child");
               exit(EXIT_FAILURE);
           }
           categories[i][strcspn(categories[i], "\n")] = 0;
           char *category = strrchr(categories[i], '/');
           if(category != NULL){
            category++;
           }
           printf("PID %d create child for %s PID:%d\n",getppid(), category, getpid());
           createCategoryLogFile(storePath, categories[i], shoppingList->userID, &orderID);
           /*char procLogMsg[MAX_PATH_LEN];
           snprintf(procLogMsg, sizeof(procLogMsg), "PID %d create child for %s pid: %d with order %d",
           getppid(), categories[i], getpid(), orderID);
           writeToLogFile(categories[i], shoppingList->userID, orderID, procLogMsg);*/


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
               inputs[j]->orderID = &orderID;
                if (pthread_create(&threads[j], NULL, searchProductInCategory, (void*)inputs[j]) != 0) {
                   perror("pthread_create failed");
                   exit(EXIT_FAILURE);
               }
               fflush(stdout);
               char threadLogMsg[MAX_PATH_LEN];
               snprintf(threadLogMsg, sizeof(threadLogMsg), "Pid %d create thread for order TID: %ld", getpid(), threads[j]);
               //writeToLogFile(categories[i], shoppingList->userID, orderID, threadLogMsg);
               //printf("bafore path: %s\n",categories[i]);
               j++;
           }

           for (int l = 0; l < j; l++) {
               pthread_join(threads[l], NULL);
               free(inputs[l]->product);
               free(inputs[l]);
           }
           while(shoppingList->stopFork){
                usleep(7);
           }
            //for(long int o = 0; o < 999999; o++){}
            //printf("category exiting! : %d\n", shoppingList->stopThread);
           munmap(shoppingList, sizeof(UserShoppingList) * 10 + sizeof(int));
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
}

void processStores(UserShoppingList* shoppingList) {
   char** stores = getSubStoreDirectories("Dataset");
   int shmFd = shoppingList->shmFd; // File descriptor for shared memory
   for (int i = 0; i < storeCount; i++) {
       pid_t pidStore = fork();
       if (pidStore == 0) {  
           UserShoppingList *mappedList = mmap(NULL, sizeof(UserShoppingList), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
           if (mappedList == MAP_FAILED) {
               perror("mmap failed in child");
               exit(EXIT_FAILURE);
           }
           printf("PID %d create child for Store%d PID:%d\n",getppid(), i + 1, getpid());

           stores[i][strcspn(stores[i], "\n")] = 0; // Remove trailing newline
           processCategories(i, stores[i], mappedList);
            
           while(shoppingList->stopFork){
                usleep(7);
           }
            //for(long int o = 0; o < 999999; o++){}
           // Cleanup in child process
           munmap(mappedList, sizeof(UserShoppingList)); // Unmap shared memory
           printf("store exiting!\n, stopfork : %d", shoppingList->stopFork);
           exit(0); // Exit the child process
       }
       else if (pidStore < 0) {  // Fork failed
           perror("Failed to fork for store");
           break;
       } /*else {  // Parent process
           printf("Parent: forked child for store %d\n", i + 1);
       }*/
   }


   // Wait for all child processes to complete
   int status;
   while (wait(&status) > 0); // Wait for any child process to finish


   // Free allocated memory for store names
   for (int i = 0; i < storeCount; i++) {
       free(stores[i]);
   }
   free(stores);
}  




void processUser(UserShoppingList* shoppingList){
   /*sem_unlink(SEM_PRODUCT_SEARCH);
  sem_unlink(SEM_RESULT_UPDATE);
  sem_unlink(SEM_INVENTORY_UPDATE);
  sem_unlink(SEM_SHOPPING_LIST);*/
   printf("\n%s create PID: %d\n",shoppingList->userID ,getpid());
   //initThreadPool();
  //semaphore
  g_search_sem = sem_open(SEM_PRODUCT_SEARCH, O_CREAT, 0644, 1);
  g_result_sem = sem_open(SEM_RESULT_UPDATE, O_CREAT, 0644, 1);
  g_shopping_list_sem = sem_open(SEM_SHOPPING_LIST, O_CREAT, 0644, 1);
   if (g_search_sem == SEM_FAILED || g_result_sem == SEM_FAILED ||
      g_shopping_list_sem == SEM_FAILED) {
      perror("Semaphore creation failed");
      return;
    }

  sem_init(&start_threads_sem, 0, 0);

  pthread_t basketValueThread, ratingThread, finalListThread;

  if(pthread_create(&basketValueThread ,NULL, calculateStoreBaskettValue, (void*)shoppingList) != 0){
    perror("failed to create first thread");
    return;
  } 
  printf("PID %d create thread for Orders TID: %ld\n",getpid(), basketValueThread);

  if(pthread_create(&ratingThread, NULL, rateProducts, (void*)shoppingList) != 0){
    perror("failed to create second thread");
    return;
  }
  printf("PID %d create thread for Scores TID: %ld\n",getpid(), ratingThread);

  processStores(shoppingList);

  sem_post(&start_threads_sem);
  sem_post(&start_threads_sem);

  pthread_join(basketValueThread,NULL);
  pthread_join(ratingThread,NULL);

  printf("ALL thread comleted\n");
 

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
       scanf("%d", &shoppingList.entity[i]);
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
            shoppingList->stopThread = true; 
            shoppingList->stopFork = true;    
            pthread_mutex_init(&shoppingList->mutex, NULL);
            pthread_cond_init(&shoppingList->cond, NULL);

           *currentUser = read_user_shopping_list();
           currentUser->shmFd = shmFd;
           processUser(currentUser);
           pthread_mutex_destroy(&shoppingList->mutex);
           pthread_cond_destroy(&shoppingList->cond);
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