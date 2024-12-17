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
#include <raylib.h>

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

//graphic
const int screenWidth = 800;
const int screenHeight = 450;
#define INPUT_FIELD_HEIGHT 40
#define INPUT_FIELD_WIDTH 300
#define INPUT_FIELD_X ((screenWidth - INPUT_FIELD_WIDTH) / 2)

typedef enum {
    STATE_USER_ID,
    STATE_PRODUCT_COUNT,
    STATE_PRODUCT_DETAILS,
    STATE_BUDGET,
    STATE_COMPLETE
} UIState;


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
   Product* foundProduct;
   int isActive;
   sem_t threadSem;
} EnhancedThreadInfo;


sem_t *g_search_sem = NULL;
sem_t *g_result_sem = NULL;
sem_t *g_rating_sem = NULL;
sem_t *g_shopping_list_sem = NULL;


UserShoppingList* shoppingList;
int *userCount = 0;
pthread_mutex_t liveLock = PTHREAD_MUTEX_INITIALIZER;
bool stopThread = false;


EnhancedThreadInfo g_threadPool[MAX_THREAD];
pthread_mutex_t g_threadPoolMutex = PTHREAD_MUTEX_INITIALIZER;


//define all functions
void processCategories(int storeNum, const char* storePath, UserShoppingList* shoppingList);
void processStroes(UserShoppingList* shoppingList);
void processUser(UserShoppingList* shoppingList);

void DrawInputField(int x, int y, int width, int height, const char* label, const char* input){
    DrawRectangle(x, y, width, height, LIGHTGRAY);
    DrawRectangleLines(x, y, width, height, DARKGRAY);


    DrawText(label, x, y - 20, 20, BLACK);
    DrawText(input, x + 5, y + 10, 20, BLACK);
}


void HandleTextInput(char* inputBuffer, int* cursorPosition, int maxLength) {
    int key = GetCharPressed();
    
    // Backspace
    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (*cursorPosition > 0) {
            (*cursorPosition)--;
            inputBuffer[*cursorPosition] = '\0';
        }
    }
    
    // Text input
    if (key >= 32 && key <= 125 && *cursorPosition < maxLength - 1) {
        inputBuffer[*cursorPosition] = (char)key;
        (*cursorPosition)++;
        inputBuffer[*cursorPosition] = '\0';
    }
}

UserShoppingList* GraphicalUserInput() {
    UserShoppingList* shoppingList = malloc(sizeof(UserShoppingList));
    memset(shoppingList, 0, sizeof(UserShoppingList));
    
    UIState currentState = STATE_USER_ID;
    char inputBuffer[100] = {0};
    int cursorPosition = 0;
    int currentProductIndex = 0;
    
    InitWindow(screenWidth, screenHeight, "kh");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        switch (currentState) {
            case STATE_USER_ID:
                DrawText("Enter User ID:", 50, 100, 20, BLACK);
                DrawRectangleLines(50, 150, 300, 40, BLACK);
                DrawText(inputBuffer, 60, 160, 20, BLACK);
                
                HandleTextInput(inputBuffer, &cursorPosition, MAX_NAME_LEN);
                
                if (IsKeyPressed(KEY_ENTER) && strlen(inputBuffer) > 0) {
                    strcpy(shoppingList->userID, inputBuffer);
                    memset(inputBuffer, 0, sizeof(inputBuffer));
                    cursorPosition = 0;
                    currentState = STATE_PRODUCT_COUNT;
                }
                break;
            case STATE_PRODUCT_COUNT:
                DrawText("Enter Number of Products:", 50, 100, 20, BLACK);
                DrawRectangleLines(50, 150, 300, 40, BLACK);
                DrawText(inputBuffer, 60, 160, 20, BLACK);
                
  HandleTextInput(inputBuffer, &cursorPosition, 3);
                
                if (IsKeyPressed(KEY_ENTER) && strlen(inputBuffer) > 0) {
                    shoppingList->productCount = atoi(inputBuffer);
                    memset(inputBuffer, 0, sizeof(inputBuffer));
                    cursorPosition = 0;
                    currentState = STATE_PRODUCT_DETAILS;
                }
                break;
            case STATE_PRODUCT_DETAILS:
                DrawText(TextFormat("Enter Product %d Name:", currentProductIndex + 1), 50, 100, 20, BLACK);
                DrawRectangleLines(50, 150, 300, 40, BLACK);
                DrawText(inputBuffer, 60, 160, 20, BLACK);
                
                HandleTextInput(inputBuffer, &cursorPosition, MAX_NAME_LEN);


                if (IsKeyPressed(KEY_ENTER) && strlen(inputBuffer) > 0) {
                    strcpy(shoppingList->products[1][currentProductIndex].name, inputBuffer);
                    memset(inputBuffer, 0, sizeof(inputBuffer));
                    cursorPosition = 0;
                    //printf("innnnnnnn");
                    
                    while(1){
                        BeginDrawing();
                        ClearBackground(RAYWHITE);


                        DrawText(TextFormat("Enter Product %d Quantity:", currentProductIndex + 1), 50, 200, 20, BLACK);
                        DrawRectangleLines(50, 250, 300, 40, BLACK);
                        DrawText(inputBuffer, 60, 260, 20, BLACK);
                        EndDrawing();


                        HandleTextInput(inputBuffer, &cursorPosition, 3);
if(IsKeyPressed(KEY_ENTER) && strlen(inputBuffer) > 0){
                            shoppingList->entity[currentProductIndex] = atoi(inputBuffer);
                            memset(inputBuffer, 0, sizeof(inputBuffer));
                            cursorPosition = 0;


                            currentProductIndex++;
                            if (currentProductIndex >= shoppingList->productCount) {
                            currentState = STATE_BUDGET;
                            }
                            break;
                        }
                    }
                }
                break;
            case STATE_BUDGET:
                DrawText("Enter Budget Cap (-1 for no cap):", 50, 100, 20, BLACK);
                DrawRectangleLines(50, 150, 300, 40, BLACK);
                DrawText(inputBuffer, 60, 160, 20, BLACK);
                
                HandleTextInput(inputBuffer, &cursorPosition, 20);
                
                if (IsKeyPressed(KEY_ENTER) && strlen(inputBuffer) > 0) {
                    shoppingList->budgetCap = atof(inputBuffer);
                    shoppingList->userPID = getpid();
                    currentState = STATE_COMPLETE;
                }
                break;
            case STATE_COMPLETE:
                CloseWindow();
                return shoppingList;
        }
        EndDrawing();
}
    CloseWindow();
    free(shoppingList);
    return NULL;
}

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
            printf("i'm in");
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
       printf("best store is: %d\n",bestStore+1);

       sem_post(g_shopping_list_sem);
   }
        if(check_user_store_in_file(FILE_ADDRESS, shoppingList->userID, bestStore)){
        printf("wow , good for you ! youll get discount from us!");
        shoppingList->totalCost = shoppingList->totalCost*0.9;
        }
        //printf("userId : %s\n", shoppingList->userID);
        write_user_store_to_file(FILE_ADDRESS, shoppingList->userID, bestStore);

   return NULL;
}


void updateProductRating(const char* productName, double newRating, int storeIndex, int productIndex) {
   int threadPoolIndex = findThreadByProductAndStore(productName, storeIndex);
   if (threadPoolIndex == -1) {
       printf("Thread for product %s not found\n", productName);
       return;
   }
  
   g_rating_sem = sem_open(SEM_RATING_UPDATE, O_CREAT, 0644, 1);
   if (g_rating_sem == SEM_FAILED) {
       perror("Semaphore creation failed for rating update");
       return;
   }
  
   sem_wait(&g_threadPool[threadPoolIndex].threadSem);
   Product* product = g_threadPool[threadPoolIndex].foundProduct;
  
   if (!product) {
       printf("No product found in thread pool\n");
       sem_post(&g_threadPool[threadPoolIndex].threadSem);
       sem_close(g_rating_sem);
       return;
   }
  
   if (sem_wait(g_rating_sem) == -1) {
       perror("Wait failed for rating");
       sem_post(&g_threadPool[threadPoolIndex].threadSem);
       sem_close(g_rating_sem);
       return;
   }
  
   // Update product rating
   double oldScore = product->score;
   product->score = (oldScore + newRating) / 2.0;
  
   // Update timestamp
   time_t now;
   time(&now);
   char formattedTime[20];
   strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%d %H:%M:%S", localtime(&now));
   strcpy(product->lastModified, formattedTime);
  
   // Save to file
   char* filePath = findProductFilePath(productName);
   if (filePath) {
       FILE* file = fopen(filePath, "w");
       if (file) {
           fprintf(file, "Name: %s\n", product->name);
           fprintf(file, "Price: %.2f\n", product->price);
           fprintf(file, "Score: %.2f\n", product->score);
           fprintf(file, "Entity: %d\n", product->entity);
           fprintf(file, "Last Modified: %s\n", formattedTime);
           fclose(file);
          
           printf("Product rating updated successfully\n");
           printf("Thread ID: %ld, PID: %d\n", pthread_self(), getpid());
           printf("New score: %.2f\n", product->score);
       } else {
           perror("Failed to update product file");
       }
       free(filePath);
   }
  
   sem_post(g_rating_sem);
   sem_post(&g_threadPool[threadPoolIndex].threadSem);
  
   sem_close(g_rating_sem);
}


void* rateProducts(void* args) {
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
  EnhancedThreadInfo threadInfos[MAX_PRODUCTS];
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


          updateProductRating(shoppingList->products[selectedStore][i].name, rating,selectedStore,i);
      }
  }
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




void initThreadPool(){
   pthread_mutex_lock(&g_threadPoolMutex);
   for(int i = 0; i < MAX_THREAD; i++){
       g_threadPool[i].isActive = 0;
       g_threadPool[i].foundProduct = NULL;
       sem_init(&g_threadPool[i].threadSem, 0, 1);
   }
   pthread_mutex_unlock(&g_threadPoolMutex);
}


int addThreadToPool(pthread_t threadID, const char* productName, int storeIndex, int productIndex) {
   pthread_mutex_lock(&g_threadPoolMutex);
   for (int i = 0; i < MAX_THREAD; i++) {
       if (!g_threadPool[i].isActive) {
           g_threadPool[i].threadID = threadID;
           g_threadPool[i].storeIndex = storeIndex;
           g_threadPool[i].productIndex = productIndex;
           strncpy(g_threadPool[i].productName, productName, MAX_NAME_LEN - 1);
           g_threadPool[i].isActive = 1;
           g_threadPool[i].foundProduct = malloc(sizeof(Product));
          
           pthread_mutex_unlock(&g_threadPoolMutex);
           return i;
       }
   }
   pthread_mutex_unlock(&g_threadPoolMutex);
   return -1; // Pool full
}


int findThreadByProductAndStore(const char* productName, int storeIndex) {
   pthread_mutex_lock(&g_threadPoolMutex);
   for (int i = 0; i < MAX_THREAD; i++) {
       if (g_threadPool[i].isActive &&
           strcmp(g_threadPool[i].productName, productName) == 0 &&
           g_threadPool[i].storeIndex == storeIndex) {
           pthread_mutex_unlock(&g_threadPoolMutex);
           return i;
       }
   }
   pthread_mutex_unlock(&g_threadPoolMutex);
   return -1;
}


void updateThreadPoolProduct(const char* productName, int storeIndex, Product* product) {
   int index = findThreadByProductAndStore(productName, storeIndex);
   if (index != -1) {
       sem_wait(&g_threadPool[index].threadSem);
       memcpy(g_threadPool[index].foundProduct, product, sizeof(Product));
       sem_post(&g_threadPool[index].threadSem);
   }
}

// Cleanup thread pool
void cleanupThreadPool() {
   pthread_mutex_lock(&g_threadPoolMutex);
   for (int i = 0; i < MAX_THREAD; i++) {
       if (g_threadPool[i].isActive) {
           if (g_threadPool[i].foundProduct) {
               free(g_threadPool[i].foundProduct);
           }
           sem_destroy(&g_threadPool[i].threadSem);
           g_threadPool[i].isActive = 0;
       }
   }
   pthread_mutex_unlock(&g_threadPoolMutex);
}


void* searchProductInCategory(void* args){
   //printf("in thread with tid : %ld\n", pthread_self());
   threadInput *input = (threadInput *)args;
   UserShoppingList* shoppingList = input->shoppingList;
   char** proNames = input->names;
   sem_wait(g_search_sem);
   Product* product = readProductFromFile(input->filepath);
   sem_post(g_search_sem);
   //printf("proCount from thread : %d\n", input->proCount);
   //printf("name : %s, %s, %s\n", product->name, input->filepath, proNames[0]);
   for(int i = 0; i < input->proCount; i++){
       if (product && strcasecmp(product->name, proNames[i]) == 0){
           sem_wait(g_result_sem);


           int threadPoolIndex = addThreadToPool(pthread_self(), proNames[i], input->storeNum, i);


           printf("i found it in %s!!!!\n", input->filepath);
           printf("TID found: %ld\n",pthread_self());


           Product* threadProduct = g_threadPool[threadPoolIndex].foundProduct;
           memcpy(threadProduct, product, sizeof(Product));
           threadProduct->foundFlag = 1;


           memcpy(&(shoppingList->products[input->storeNum][i]), threadProduct, sizeof(Product));


           /*memcpy(input->product->name, product->name, sizeof(product->name));
           memcpy(input->product->lastModified, product->lastModified, sizeof(product->lastModified));
           input->product->price = product->price;
           input->product->score = product->score;
           input->product->entity = product->entity;
           input->product->foundFlag = 1;
           input->proNum = i;*/
           sem_post(g_result_sem);


           /*while (!shoppingList.stopThread){
               pthread_yield();
           }*/
           //memcpy(&(shoppingList->products[input->storeNum][i]), input->product, sizeof(Product));
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
   int orderID = 0;
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
           createCategoryLogFile(storePath, categories[i], shoppingList->userID, &orderID);
           char procLogMsg[MAX_PATH_LEN];
           snprintf(procLogMsg, sizeof(procLogMsg), "PID %d create child for %s pid: %d with order %d",
           getppid(), categories[i], getpid(), orderID);
           writeToLogFile(categories[i], shoppingList->userID, orderID, procLogMsg);


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
               char threadLogMsg[MAX_PATH_LEN];
               snprintf(threadLogMsg, sizeof(threadLogMsg), "Pid %d create thread for order TID: %ld", getpid(), threads[j]);
               writeToLogFile(categories[i], shoppingList->userID, orderID, threadLogMsg);
               j++;
           }


           for (int l = 0; l < j; l++) {
               pthread_join(threads[l], NULL);
               /*if (inputs[l]->product->foundFlag == 1) {
                   memcpy(&(shoppingList->products[storeNum][inputs[l]->proNum]), inputs[l]->product, sizeof(Product));
               }*/
               free(inputs[l]->product);
               free(inputs[l]);
           }


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
           exit(0); // Exit the child process
       }
       else if (pidStore < 0) {  // Fork failed
           perror("Failed to fork for store");
           break;
       } else {  // Parent process
           printf("Parent: forked child for store %d\n", i + 1);
       }
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
   printf("in user process with pid: %d\n", getpid());
   initThreadPool();
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


    processStores(shoppingList);

  pthread_create(&basketValueThread ,NULL, calculateStoreBaskettValue, (void*)shoppingList);
  //pthread_create(&ratingThread, NULL, rateProducts, (void*)shoppingList);
   //pthread_detach(basketValueThread);
  
   //pthread_join(ratingThread, NULL);


  pthread_join(basketValueThread, NULL);


  if(shoppingList->store_match_count[0] || shoppingList->store_match_count[1] ||
       shoppingList->store_match_count[2]){ // age mitonest bekhare asan


       pthread_create(&ratingThread, NULL, rateProducts, (void*)shoppingList);
       pthread_join(ratingThread, NULL);


       //pthread_create(&finalListThread, NULL, updateFinalShoppingList, shoppingList);
       //pthread_join(finalListThread, NULL);

       cleanupThreadPool();
   }


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

            //graphic
            /*UserShoppingList* shoppingList = GraphicalUserInput();
    
            if (shoppingList) {
            // Print out the collected information for verification
            printf("User ID: %s\n", shoppingList->userID);
            printf("Product Count: %d\n", shoppingList->productCount);
            
            for (int i = 0; i < shoppingList->productCount; i++) {
                printf("Product %d: %s (Quantity: %d)\n", 
                    i+1, 
                    shoppingList->products[1][i].name, 
                    shoppingList->entity[i]);
            }
            
            printf("Budget Cap: %.2f\n", shoppingList->budgetCap);*/

            ////////////////////////////////
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