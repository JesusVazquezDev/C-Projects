//#include "official.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

struct Node{
    char *shortcut; // the shortcut
    char *command; // command
    struct Node *next; // Null when end of line
};

struct Node* head;
struct Node* current;
char buffer[512];
char* token;
char* argm[100];


// Prints all the aliases currently registered
void printAlias() {
    struct Node* current = head;

    while(current != NULL) {
        fprintf(stdout, "%s %s\n", current->shortcut, current->command);
        fflush(stdout);
        current = current->next;
    }
}

// Create new alias at the beginning of the list
void newAlias(char* shortcut, char* command) {
    struct Node* newNode = (struct Node*) malloc(sizeof(struct Node));
    
    newNode->shortcut = shortcut;
    newNode->command = command;
    newNode->next = NULL;
    
    if(head != NULL){
        struct Node* lastNode = head;
        // Finding the last node
        while(lastNode->next != NULL){
            lastNode = lastNode -> next;
        }
        lastNode -> next = newNode;
    }
    else{
        head = newNode;
    }
}


// Searches the linked list to see if alias already exists.
struct Node* findAlias(char* shortcut) {
    struct Node* current = head;

    //if empty
    if(head == NULL) {
        return NULL;
    }

    while(strcmp(current->shortcut,shortcut) != 0){
        // If no more aliases, return NULL = alias not found
        if(current->next == NULL) {
            return NULL;
        }
        else {
            current = current->next;
            continue;
        }
    }
    
    return current;
}

// Deletes specified alias
struct Node* deleteAlias(char* shortcut) {
    struct Node* current = head;
    struct Node* previous = NULL;
    
    //if empty
    if(head == NULL) {
        return NULL;
    }

    while(strcmp(current->shortcut,shortcut) != 0) {
        // If no more aliases, return NULL = alias not found
        if(current->next == NULL) {
            return NULL;
        }
        else {
            // Saves reference to current alias
            previous = current;
            // Set current to next alias
            current = current->next;
            continue;
        }
    }
    
    // If alias is the first one in the list
    if(current == head) {
        // Removes alias
        head = head->next;
    }
    else {
        // Removes alias
        previous->next = current->next;
    }
    
    return current;
}

// Checks the length of the linked list
int lengthAlias() {
    int length = 0;
    struct Node *current;

    for(current = head; current != NULL; current = current->next) {
        length += 1;
    }
    
    return length;
}

char* createCommand(char* argm[], int args){
    char* remaining;
    
    for(int i = 2; i < args; i++) {
        //printf("List goes up to: %i\n",i);
        
        if(i == 2){
            remaining = argm[i];
            // adding space to divide evenly
            strcat(remaining," ");
            //printf("Should start looping again...");
        }
        else{
            //printf("About to concat 2nd arg\n");
            strcat(remaining,argm[i]);
            if(i+1 != args){
                printf("Adding space after: %s\n",argm[i]);
                strcat(remaining," ");
            }
        }
    }
    
    return remaining;
}

void cleanToken(int o, char* temp, char *token){
//    printf("Token: %s\n",token);
//    printf("Length Token: %i\n",(int)strlen(token));
    int track = 0;
    for(int y = 0; y < o;y++){
        if(token[y] != ' ' && token[y] != '\n' && token[y] != '\0' && token[y] != '\t'){
//            printf("token: %c\n",token[y]);
//            printf("No spaces or new lines found in: %i\n",y);
            temp[y-track] = token[y];
        }
        else if((y + 1) == o){
//            printf("o: %i\n",o);
            temp[y-track] = '\0';
            break;
        }
        
        else{
//            printf("YOU");
            track +=1;
            continue;
        }
    }
}


int checkRedirects(char* buffer){
    int test = 0 ;
    int redirects = 0;
    int track = 0;
    int amount = strlen(buffer);
    
    for(int i = 0; i < amount; i ++){
        if(i == 0 && buffer[i] == '>'){
            // error out
            char* error = "Redirection misformatted.\n";
            write(2,error, strlen(error));
            return -1;
        }
        else if(buffer[i] == '>'){
            // Keep track of the amount of redirects given
            redirects += 1;
            track = i;
            // Erase it
            buffer[i] = ' ';
        }
        else if(buffer[i] == ' '){
            if(redirects>0){
                // does this give me the end of every word?
                test += 1;
            }
        }
    }
//    printf("Found %i redirects in buffer.\n", redirects);
//    printf("Found %i words in buffer.\n", test);
//
    // Validates that we only have one file specified after the redirect
    if(redirects == 1 && test > 1){
        //printf("test: %i\n",test);
        // error out
        char* error = "Redirection misformatted.\n";
        write(2,error, strlen(error));
        return -1;
        //exit(1);
    }
    //
    if(redirects == 1 && test == 0 ){
//        printf("track and amount : %i %i\n", track,amount);
        if((track+1) == (amount-1)){
            // error out
            char* error = "Redirection misformatted.\n";
            write(2,error, strlen(error));
            return -1;
        }
    }
    
    // Handles the case of more than 1 redirection
    if(redirects > 1){
        // error out
        char* error = "Redirection misformatted.\n";
        write(2,error, strlen(error));
        return -1;
    }
    return redirects;
}


int populateArg(char* token,char* argm[],int redirects){
    int x = 0;
    while(token != NULL){
//        printf("Value of token: %s\n",token);
        //MODIFIES TOKEN
        int o = strlen(token);
        char* temp = malloc(o);
        // Cleans up the token
        cleanToken(o,temp,token); // modifies temp
        // appending clean token to argm == argv
        //redirects += checkForRedirects(temp,token);
        if((int)strlen(temp) == 0){
            token = strtok(NULL," ");
            argm[x] = NULL;
            continue;
        }
        //printf("length of temp: %i\n", (int) strlen(temp));
        // Temp is clean now
        argm[x] = temp;
        x+=1;
        token = strtok(NULL," ");
        argm[x] = NULL;
        
    }
    
//    if(redirects == 1 && int flag == 0){
//        argm[x-1] = NULL;
//        x -= 1;
//        token = argm[x-1];
//        strcat(token,"X");
//        argm[x-1] = token;
//    }
    
    return x;
}

int newChild (int i, int redirect, char *argm[]) {
//    printf("redirects: %i\n",redirect);
    int status;
    int pid = fork();
    char* file = argm[i-1];
    
    if (pid == 0){
        if(redirect == 1){
//            int file_fd = open(argm[i-1], O_CREAT|O_RDWR|O_TRUNC|O_CLOEXEC|O_WRONLY|00700);
            int file_fd = open(argm[i-1], O_WRONLY | O_TRUNC | O_CREAT, 00666);
//            printf("file descriptor: %d\n",file_fd);
            argm[i-1] = NULL;
            i -= 1;
            token = argm[i-1];
            strcat(token,"\0");
            argm[i-1] = token;
            
//            // prints argm
//            for(int y = 0; y < i; y++){
//                printf("Child argm[%i] == %s\n", y , argm[y]);
//            }
            
            if(file_fd == -1){
                fprintf(stderr, "Cannot write to file %s.\n", file);
                fflush(stderr);
                _exit(1);
            }
            dup2(file_fd,1);
        }
        execv(argm[0],argm);
        // Should not go past this...
//        int errvalue;
//        errvalue = errno;
//        printf( "The error generated was %d\n", errvalue );
//        printf( "That means: %s\n", strerror( errvalue ) );
//        printf("This should not print out");
        // Exits with error
        _exit(1);
    }
    else{
        // Waits for child process
        waitpid(pid, &status, 0);
        return status;
    }
    return status;
}

void interactiveMode(FILE *fp){
    while(1){
        // Continuously writes to stout
        write(1,"mysh> ",6);
        
        
        // Prompts for user input and handles ctrl-d scenario
        if(fgets(buffer,sizeof(buffer),fp) == NULL) {
            //fflush(fp);
            break;
        }
        int range = strlen(buffer);

        if(range == 1){
            continue;
        }
        
        // TODO: This needs modification because if exit is found present at all in the buffer, it should close.
        if(strncmp(buffer,"exit",4) == 0 ) {
            fflush(fp);
            break;
        }
        
        // Verifies if redirects are present, it handles all of the error cases. Returns amount of Redirects found.
        int redirects = checkRedirects(buffer);
        if (redirects < 0 || redirects > 1){
            continue;
        }
        
    
        // Tokenizes the buffer NOTE: strtok modifies input
        token = strtok(buffer," ");
        
        // Method that corrects tokenized input properly and populates an argument list of pointers
        int i = populateArg(token,argm,redirects);
        
        if(i == 1 && strcmp(argm[0],"alias") != 0 && findAlias(argm[0]) == NULL){
            // 139 refers to segmentation fault
            if(newChild(i,redirects,argm) != 139){
//                int ret = newChild(i,redirects,argm);
//                printf("RETURN VALUE: %i\n",ret);
                fprintf(stderr, "%s: Command not found.\n", argm[0]);
                fflush(stderr);
                continue;
            }
            else{
                fflush(fp);
                continue;
            }
        }
        
        // If alias is found
        if(findAlias(argm[0]) != NULL && i == 1){
            struct Node* test = findAlias(argm[0]);
            //printf("Executing alias.\n");
            // Tokenizes the buffer NOTE: strtok modifies input
            token = strtok(test->command," ");
            //argm = NULL;
            // Method that corrects tokenized input properly and populates an argument list of pointers
            int i = populateArg(token,argm,redirects);
            newChild(i,0,argm);
            fflush(fp);
            continue;
        }
        
//        printf("2args: %i\n",i);
        // Checks to see if user wants to create an alias before processing
        if(strcmp(argm[0],"alias") == 0){
            // if user only specifies "alias", print all alias
            if(argm[1] == NULL){
                // Print Aliases
                printAlias();
                fflush(fp);
                continue;
            }
            
            // Save shortcut
            char* shortcut = argm[1];
            
            // Creates new char pointer with commands after shortcut
            char* command = createCommand(argm,i);

            // Checks the length of our linked list, if 0, it adds Alias to the head
            if(lengthAlias() == 0){
                newAlias(shortcut,command);
                fflush(fp);
                continue;
            }
            
            else{
                // Search the list to see if alias is present
                if(findAlias(shortcut) == NULL){
                    // Add alias given it is not present
                    newAlias(shortcut,command);
                    fflush(fp);
                    continue;
                }
                else{
                    // Alias exists, delete and re-add ( TODO: May need to modify to only change entries )
                    if(argm[2] != NULL){
                        deleteAlias(shortcut);
                        newAlias(shortcut,command);
        //                    fflush(fp);
                        continue;
                    }
                    else{
                        current = findAlias(shortcut);
                        char* test = current->command;
                        int length = strlen(test);
                        
//                        printf("before length: %i\n",length);
                        if(test[length-1] == ' '){
                            test[length-1] = '\0';
                        }
//                        length = strlen(test);
//                        printf("after length: %i\n",length);
                        fprintf(stdout,"%s %s\n", current->shortcut, current->command);
                        fflush(stdout);
                        continue;
                    }
                }
            }
        }
        
        else if(strcmp(argm[0],"unalias") == 0){
            char* shortcut = argm[1];
            deleteAlias(shortcut);
            continue;
        }
        
        else{
            //printf("Executing normally.\n");
//            printf("3args: %i\n",i);
            newChild(i,redirects,argm);
            //fflush(fp);
        }
    }
}

void batchMode(FILE* fp){
    // fgets waits for user input
    while(fgets(buffer,sizeof(buffer),fp) != NULL) {
        
        write(1,buffer,strlen(buffer));
        
        // Verifies if redirects are present, it handles all of the error cases. Returns amount of Redirects found.
        int redirects = checkRedirects(buffer);
        
        if (redirects < 0 || redirects > 1){
            continue;
        }

//        printf("BUFFER: %s\n",buffer);
        // Tokenizes the buffer NOTE: strtok modifies input
        token = strtok(buffer," ");
        
        // Method that corrects tokenized input properly and populates an argument list of pointers
        int i = populateArg(token,argm,redirects);
        
        
        // Check if exit is present.
        for(int y = 0 ; y < i; y++){
            if(strcmp(argm[y],"exit") == 0 ) {
                exit(0);
            }
        }
        
        
//        printf("args size is: %i\n",i);
        if(i == 0){
            continue;
        }
//        for(int z = 0; z < i ; z++){
//            printf("argm[%i] = %s\n",z,argm[z]);
//        }
        
        if(i == 1 && strcmp(argm[0],"alias") != 0 && findAlias(argm[0]) == NULL){
            
            if(newChild(i,redirects,argm) != 139){
//                int ret = newChild(i,redirects,argm);
//                printf("RETURN VALUE: %i\n",ret);
                fprintf(stderr, "%s: Command not found.\n", argm[0]);
                fflush(stderr);
                continue;
            }
            else{
                fflush(fp);
                continue;
            }
        }
        
        // If alias is found
        if(findAlias(argm[0]) != NULL && i == 1){
            struct Node* test = findAlias(argm[0]);
            //printf("Executing alias.\n");
            // Tokenizes the buffer NOTE: strtok modifies input
            token = strtok(test->command," ");
            //argm = NULL;
            // Method that corrects tokenized input properly and populates an argument list of pointers
            int i = populateArg(token,argm,redirects);
            newChild(i,0,argm);
            fflush(fp);
            continue;
        }
        
        
        // Checks to see if user wants to create an alias before processing
        if(strcmp(argm[0],"alias") == 0){
            // if user only specifies "alias", print all alias
            if(argm[1] == NULL){
                // Print Aliases
                printAlias();
                fflush(fp);
                continue;
            }
            
            // Save shortcut
            char* shortcut = argm[1];
            
            // Creates new char pointer with commands after shortcut
            char* command = createCommand(argm,i);

            // Checks the length of our linked list, if 0, it adds Alias to the head
            if(lengthAlias() == 0){
                newAlias(shortcut,command);
                fflush(fp);
                continue;
            }
            
            else{
                // Search the list to see if alias is present
                if(findAlias(shortcut) == NULL){
                    // Add alias given it is not present
                    newAlias(shortcut,command);
                    fflush(fp);
                    continue;
                }
                else{
                    // Alias exists, delete and re-add ( TODO: May need to modify to only change entries )
                    if(argm[2] != NULL){
                        deleteAlias(shortcut);
                        newAlias(shortcut,command);
        //                    fflush(fp);
                        continue;
                    }
                    else{
                        current = findAlias(shortcut);
                        char* test = current->command;
                        int length = strlen(test);
                        
//                        printf("before length: %i\n",length);
                        if(test[length-1] == ' '){
                            test[length-1] = '\0';
                        }
//                        length = strlen(test);
//                        printf("after length: %i\n",length);
                        
                        fprintf(stdout,"%s %s\n", current->shortcut, current->command);
                        fflush(stdout);
                        continue;
                    }
                }
            }
        }
        
        else if(strcmp(argm[0],"unalias") == 0){
            char* shortcut = argm[1];
            deleteAlias(shortcut);
            continue;
        }
        
        newChild(i,redirects,argm);
//        fflush(fp);
        continue;
    }
}


int main(int argc, char *argv[]){
    
    // Shell takes either 1 or 2 arguments, nothing more.
    if(argc > 2) {
        char* errstr = "Usage: mysh [batch-file]\n";
        write(STDERR_FILENO,errstr,strlen(errstr));
        exit(1);
    }
    
    // Setting fp as stdin by default
    FILE* fp = stdin;

    // Interactive Mode ( argc = 1 , argv[0] = "./mysh" )
    if(argc == 1) {
        //printf("Running in interactive mode\n");
        interactiveMode(fp);
    }

    // Batch Mode ( argc = 2 , argv[0] = "./mysh" , argv[1] = file/path to file)
    else{
        //printf("Running in batch mode\n");
        // Read in file
        fp = fopen(argv[1],"r");
        
        // If unable to open, error
        if(fp == NULL){
            char err[100] = "Error: Cannot open file ";
            strcat(err,argv[1]);
            strcat(err,".\n");
            write(STDERR_FILENO, err, strlen(err));
            exit(1);
        }
        batchMode(fp);
    }

    return 0;
}


