#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "hashmap.h"

// Adding pthread library
#include <pthread.h>

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

// Adding thread lock ( universal lock TODO needs description )
pthread_rwlock_t rwlock;

// No lock necessary here
HashMap* MapInit(void)
{
    HashMap* hashmap = (HashMap*) malloc(sizeof(HashMap));
    hashmap->contents = (MapPair**) calloc(MAP_INIT_CAPACITY, sizeof(MapPair*));
    hashmap->capacity = MAP_INIT_CAPACITY;
    hashmap->size = 0;
    return hashmap;
}

void MapPut(HashMap* hashmap, char* key, void* value, int value_size)
{
    // Write Lock
    pthread_rwlock_wrlock(&rwlock);
    if (hashmap->size > (hashmap->capacity / 2)) {
        if (resize_map(hashmap) < 0) {
            // Unlocking ( may not be necessary )
            pthread_rwlock_unlock(&rwlock);
            exit(0);
        }
    }
    
    MapPair* newpair = (MapPair*) malloc(sizeof(MapPair));
    int h;

    newpair->key = strdup(key);
    newpair->value = (void *)malloc(value_size);
    memcpy(newpair->value, value, value_size);
    h = Hash(key, hashmap->capacity);

    while (hashmap->contents[h] != NULL) {
        // if keys are equal, update
        if (!strcmp(key, hashmap->contents[h]->key)) {
            free(hashmap->contents[h]);
            hashmap->contents[h] = newpair;
            // Unlocking
            pthread_rwlock_unlock(&rwlock);
            return;
        }
        h++;
        if (h == hashmap->capacity){
            h = 0;
        }
    }

    // key not found in hashmap, h is an empty slot
    hashmap->contents[h] = newpair;
    hashmap->size += 1;
    // Unlocking
    pthread_rwlock_unlock(&rwlock);
}

char* MapGet(HashMap* hashmap, char* key)
{
    // Read Lock
    pthread_rwlock_rdlock(&rwlock);
    int h = Hash(key, hashmap->capacity);
    while (hashmap->contents[h] != NULL) {
        if (!strcmp(key, hashmap->contents[h]->key)) {
            return hashmap->contents[h]->value;
        }
        h++;
        if (h == hashmap->capacity) {
            h = 0;
        }
    }
    // Unlocking
    pthread_rwlock_unlock(&rwlock);
    return NULL;
}

size_t MapSize(HashMap* map)
{
    return map->size;
}

int resize_map(HashMap* map)
{
    MapPair** temp;
    size_t newcapacity = map->capacity * 2; // double the capacity

    // allocate a new hashmap table
    temp = (MapPair**) calloc(newcapacity, sizeof(MapPair*));
    if (temp == NULL) {
        printf("Malloc error! %s\n", strerror(errno));
        return -1;
    }

    size_t i;
    int h;
    MapPair* entry;
    // rehash all the old entries to fit the new table
    for (i = 0; i < map->capacity; i++) {
        if(map->contents[i] == NULL){
            continue;
        }
        entry = map->contents[i];
        
        h = Hash(entry->key, newcapacity);
        
        while (temp[h] != NULL) {
            h++;
            if (h == newcapacity){
                h = 0;
                //continue;
            }
            //h++;
        }
        temp[h] = entry;
    }

    // free the old table
    free(map->contents);
    // update contents with the new table, increase hashmap capacity
    map->contents = temp;
    map->capacity = newcapacity;
    return 0;
}

// FNV-1a hashing algorithm
// https://en.wikipedia.org/wiki/Fowler-Noll-Vo_hash_function#FNV-1a_hash
size_t Hash(char* key, size_t capacity) {
    size_t hash = FNV_OFFSET;
    for (const char *p = key; *p; p++) {
        hash ^= (size_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
        hash ^= (size_t)(*p);
    }
    return (hash % capacity);
}
