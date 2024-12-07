#include <stdio.h>
#include "OS-multithread-shopping/CreatCategoryProccess.c" 

int main(){
    while (1) {
        pid_t pidUser = vfork();
        
        if(pidUser < 0){
            printf("fork faild!\n");
        }

        if(pidUser == 0){ 
            int storeCount = 3;
            char Dataset[1000] = "Dataset";  
            char** stores = getSubDirectories(Dataset);

            //storeCount = sizeof(stores);
            for (int i = 0; i < storeCount; i++){
                printf("k : %s\n", stores[i]);
            }
            UserData *userData = malloc(sizeof(UserData));
            userData = processShoppingList();
            
            // making process for stores
            
            for (int i = 0; i < storeCount; i++){
                pid_t pidStore = vfork();
                if(pidStore == 0){
                    char dir[1000] = "";
                    printf("im searching for your items in store%d\n", i);
                    //creating childs fot categories
                    char ** categories;
                    categories = getSubDirectories(stores[i]);
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