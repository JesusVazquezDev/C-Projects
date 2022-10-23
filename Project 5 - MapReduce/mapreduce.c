#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "mapreduce.h"


// Struct for a KeyValue Pair in a Partition
typedef struct {
    char* key;
    char* value;
} KeyValuePair;

// Struct for a partition which contain KeyValue Pairs
typedef struct {
    KeyValuePair* contents;    // Partition -> contents[i] = KeyValuePair
    int maxCapacity;           // Tracks the maxCapacity of each Partition   Partition -> capacity[i]
    int usedCapacity;	       // Tracks how much of the partition has been used
    int checked;	       // Acts as a partition pointer. 
} Partition;

// Struct that manages all partitions created
typedef struct {
    Partition** partitions;    // pStructure -> partitions[i] = Partition
    int num_partitions;         // Track how many partitions there are.
} pStructure;


pthread_mutex_t mapLock;
pthread_mutex_t reduceLock;
pthread_mutex_t fileLock;

char **files;
int fileIndex;
int partitionIndex;

int num_files;

Mapper mapFunc;
Reducer reduceFunc;
Partitioner partFunc;

pStructure* pmap;

/**
 * Initializes core partition structure with key values
 */
void pmap_init(int size) {
    // Initializes the entire underlying structure
    pmap  = (pStructure*) malloc(sizeof(pStructure));
    
    // Sets up 'size' partitions in structure
    pmap->partitions = (Partition**) malloc(size * sizeof(Partition*));
    
    // Number of Partitions created
    pmap->num_partitions = size;

    // For each partition
    for (int i = 0; i < size; i++) {
        // Allocating partitions Partition
        pmap->partitions[i] = (Partition*) malloc(sizeof(Partition));
        
        // Allocating the contents in partition to be KeyValuePairs
        pmap->partitions[i]->contents = (KeyValuePair*) calloc(size,sizeof(KeyValuePair));
        
        // Setting the capacity of paritition to be 0 initially.
        pmap->partitions[i]->maxCapacity = size;
        pmap->partitions[i]->usedCapacity = 0;
        pmap->partitions[i]->checked = 0;
        
    }
}

int resize_partition(unsigned long i){ 
    // Doubling the capacity of KeyValuePairs in specified partition
    int newCapacity = pmap->partitions[i]->maxCapacity * 2;
   
    // Setting up temp to be new partition
    KeyValuePair* temp = (KeyValuePair*) calloc(newCapacity,sizeof(KeyValuePair));
    
    if (temp == 0) {
        printf("Malloc error! %s\n",strerror(errno));
        return -1;
    }
    
    int h = 0;
    while(pmap->partitions[i]->contents[h].key != 0){
        temp[h] = pmap->partitions[i]->contents[h];
        h++;
    }
    // Freeing previously allocated partition
    free(pmap->partitions[i]->contents);

    pmap->partitions[i]->contents = temp;
    
    
    //printf("New Capacity is : [%d] \n", newCapacity);
    // Setting new capacity of paritition.
    pmap->partitions[i]->maxCapacity = newCapacity;
    
    return 0;
}

int pmap_put(KeyValuePair* newPair, unsigned long i){
    pthread_mutex_lock(&mapLock);
    
    // Resizes the partition if we are getting close to capacity
    if (pmap->partitions[i]->usedCapacity > (pmap->partitions[i]->maxCapacity / 2)) {
        if (resize_partition(i) < 0) {
            // Unlocking ( may not be necessary )
            pthread_mutex_unlock(&mapLock);
            return -1;
        }
    }
    
    pmap->partitions[i]->contents[pmap->partitions[i]->usedCapacity] = *newPair;
    pmap->partitions[i]->usedCapacity += 1;
    
    // Unlocking
    pthread_mutex_unlock(&mapLock);
    return 0;
}


void MR_Emit(char *key, char *value) {

    // allocating new pair
    KeyValuePair* newPair = (KeyValuePair*) malloc(sizeof(KeyValuePair));
    
    // If error in allocation, exit
    if (newPair == NULL) {
        printf("Malloc error! %s\n", strerror(errno));
        exit(1);
    }
    
    // setting key and value;
    newPair->key = strdup(key);
    newPair->value = strdup(value);
    
    // Find partition for which newPair belongs in
    unsigned long index = partFunc(key, pmap->num_partitions);
    
    // pmap_put checks if already in partition and adds it if not.
    if(pmap_put(newPair,index) == -1){
       printf("Unable to expand partition. %s\n", strerror(errno));	
       exit(1);
    }
}

// Wrapper for map Function
void* mapping (void* args){
    // While files exist in the queue
    while (fileIndex < num_files) {
        pthread_mutex_lock(&fileLock);
        int index = fileIndex;
        fileIndex += 1;
        pthread_mutex_unlock(&fileLock);
        // CRITICAL SECTION
        // Saving name of file while in lock
        //printf("[%d]\n", index);
        char* file = files[index];

        //printf("fileName: %s\n",file);
        if(file != NULL){
            mapFunc(file);
        }
    }
    //printf("ALL FILES MAPPED! [%d] \n", index);
    return NULL;
}

char* get_func(char* key, int i) {
    // If index is somehow greater than the number of Partitions
    if(i >= pmap->num_partitions){
        return NULL;
    }
     // if no more keyValue Pairs are to be checked.
    if (pmap->partitions[i]->checked >= pmap->partitions[i]->usedCapacity) {
        return NULL;
    }
    
    // If key ends up being NULL
    if(key == 0){
        return NULL;
    }

    // Grabs partition pointer
    int keyValueIndex = pmap->partitions[i]->checked;
    
    if (!strcmp(pmap->partitions[i]->contents[keyValueIndex].key, key)) {
        pmap->partitions[i]->checked += 1;
        return pmap->partitions[i]->contents[keyValueIndex].value;
    }
    
    return NULL;
}

void* reducing(void* args){
    int index;
    
    pthread_mutex_lock(&reduceLock);
    index = partitionIndex;
    partitionIndex++;
    pthread_mutex_unlock(&reduceLock);
    //printf("Index is: [%d]\n",index);

    int keyValueIndex = 0;
    char* key = 0;
    while (pmap->partitions[index]->checked < pmap->partitions[index]->usedCapacity) {
        keyValueIndex = pmap->partitions[index]->checked;
        key = pmap->partitions[index]->contents[keyValueIndex].key;
	reduceFunc(key, get_func, index);
    }
    return NULL;
}

int cmp(const void* A, const void* B) {
    KeyValuePair* dictA = (KeyValuePair *) A;
    KeyValuePair* dictB = (KeyValuePair *) B;
    
    return strcmp(dictA->key, dictB->key);
}



void MR_Run(int argc, char *argv[], Mapper map, int num_mappers,
            Reducer reduce, int num_reducers, Partitioner partition) {
    
    // Globalizing the passed in map, reduce and partition functions
    mapFunc = map;
    reduceFunc = reduce;
    partFunc = partition;
    
    num_files = argc-1;
    
    // This ensures that the file array created isn't to large for no reason.
    char* filesFound[num_files];
    
    // Globalizing.
    files = filesFound;
    
    int i;
    for(i = 0; i < num_files; i++){
        files[i] = argv[i+1];
    }
    
    // Setting global variable
    fileIndex = 0;
    partitionIndex = 0;
    
    pmap_init(num_reducers);
    
    // Initializing locks
    pthread_mutex_init(&mapLock, NULL);
    pthread_mutex_init(&reduceLock, NULL);
    pthread_mutex_init(&fileLock, NULL);
    
    pthread_t mapper_threads[num_mappers];
    for (int i = 0; i < num_mappers; i++) {
        pthread_create(&mapper_threads[i], NULL, mapping, NULL);
    }

    for(int i = 0; i < num_mappers; i++) {
        pthread_join(mapper_threads[i], NULL);
    }
    
    for (int i = 0; i < num_reducers; i++){
        qsort(pmap->partitions[i]->contents, pmap->partitions[i]->usedCapacity, sizeof(KeyValuePair), cmp);
    }

    pthread_t reducer_threads[num_reducers];
    for (int i = 0; i < num_reducers; i++) {
        pthread_create(&reducer_threads[i], NULL, reducing, NULL);
    }

    for(int i = 0; i < num_reducers; i++) {
        pthread_join(reducer_threads[i], NULL);
    }
}

unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
}
