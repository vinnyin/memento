#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "map.h"

/*
 * Return an empty hashmap, or NULL on failure.
 */
map_t m_create() {
    h_map* m = (h_map*) malloc(sizeof(h_map));
    if(!m) return NULL;

    m->data = (kv_pair*) calloc(INITIAL_SIZE, sizeof(kv_pair));
    if(!m->data) {
        if (m)
            m_release(m);
        return NULL;
    }

    m->table_size = INITIAL_SIZE;
    m->size = 0;

    return m;
}

/* The implementation here was originally done by Gary S. Brown. Slighltly
 * modified by Pete Warden, without any imposition on the reuse of the code.
 */

/* ============================================================= */
/*  COPYRIGHT (C) 1986 Gary S. Brown.  You may use this program, or       */
/*  code or tables extracted from it, as desired without restriction.     */
/*                                                                        */
/*  First, the polynomial itself and its table of feedback terms.  The    */
/*  polynomial is                                                         */
/*  X^32+X^26+X^23+X^22+X^16+X^12+X^11+X^10+X^8+X^7+X^5+X^4+X^2+X^1+X^0   */
/*                                                                        */
/*  Note that we take it "backwards" and put the highest-order term in    */
/*  the lowest-order bit.  The X^32 term is "implied"; the LSB is the     */
/*  X^31 term, etc.  The X^0 term (usually shown as "+1") results in      */
/*  the MSB being 1.                                                      */
/*                                                                        */
/*  Note that the usual hardware shift register implementation, which     */
/*  is what we're using (we're merely optimizing it by doing eight-bit    */
/*  chunks at a time) shifts bits into the lowest-order term.  In our     */
/*  implementation, that means shifting towards the right.  Why do we     */
/*  do it this way?  Because the calculated CRC must be transmitted in    */
/*  order from highest-order term to lowest-order term.  UARTs transmit   */
/*  characters in order from LSB to MSB.  By storing the CRC this way,    */
/*  we hand it to the UART in the order low-byte to high-byte; the UART   */
/*  sends each low-bit to hight-bit; and the result is transmission bit   */
/*  by bit from highest- to lowest-order term without requiring any bit   */
/*  shuffling on our part.  Reception works similarly.                    */
/*                                                                        */
/*  The feedback terms table consists of 256, 32-bit entries.  Notes:     */
/*                                                                        */
/*      The table can be generated at runtime if desired; code to do so   */
/*      is shown later.  It might not be obvious, but the feedback        */
/*      terms simply represent the results of eight shift/xor opera-      */
/*      tions for all combinations of data and CRC register values.       */
/*                                                                        */
/*      The values must be right-shifted by eight bits by the "updcrc"    */
/*      logic; the shift must be unsigned (bring in zeroes).  On some     */
/*      hardware you could probably optimize the shift in assembler by    */
/*      using byte-swap instructions.                                     */
/*      polynomial $edb88320                                              */
/*                                                                        */
/*  --------------------------------------------------------------------  */

static unsigned long crc32_tab[] = {
    0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
    0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
    0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
    0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
    0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
    0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
    0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
    0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
    0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
    0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
    0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
    0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
    0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
    0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
    0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
    0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
    0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
    0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
    0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
    0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
    0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
    0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
    0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
    0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
    0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
    0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
    0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
    0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
    0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
    0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
    0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
    0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
    0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
    0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
    0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
    0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
    0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
    0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
    0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
    0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
    0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
    0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
    0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
    0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
    0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
    0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
    0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
    0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
    0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
    0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
    0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
    0x2d02ef8dL
};

/* Return a 32-bit CRC of the contents of the buffer. */

static unsigned long crc32(const unsigned char *s, unsigned int len) {
    unsigned int i;
    unsigned long crc32val;

    crc32val = 0;
    for (i = 0;  i < len;  i ++) {
        crc32val = crc32_tab[(crc32val ^ s[i]) & 0xff] ^ (crc32val >> 8);
    }
    return crc32val;
}

/*
 * Hashing function for a string
 */
static unsigned int hashmap_hash_int(h_map * m, char* keystring) {

    unsigned long key = crc32((unsigned char *) (keystring), strlen(keystring));

    /* Robert Jenkins' 32 bit Mix Function */
    key += (key << 12);
    key ^= (key >> 22);
    key += (key << 4);
    key ^= (key >> 9);
    key += (key << 10);
    key ^= (key >> 2);
    key += (key << 7);
    key ^= (key >> 12);

    /* Knuth's Multiplicative Method */
    key = (key >> 3) * 2654435761;

    return key % m->table_size;
}

/*
 * Return the integer of the location in data
 * to store the point to the item, or MAP_FULL.
 */
static int hashmap_hash(map_t in, char* key) {
    int i, curr;

    /* Cast the hashmap */
    h_map* m = (h_map *) in;

    /* If full, return immediately */
    if(m->size >= (m->table_size/2)) return MAP_FULL;

    /* Find the best index */
    curr = hashmap_hash_int(m, key);

    /* Linear probing */
    for(i = 0; i< MAX_CHAIN_LENGTH; i++){
        if(m->data[curr].in_use == 0)
            return curr;

        if(m->data[curr].in_use == 1 && (strcmp(m->data[curr].key, key) == 0))
            return curr;

        curr = (curr + 1) % m->table_size;
    }

    return MAP_FULL;
}

/* Auxiliary function to determine wether a descriptor is already inside a
 * subscriber array, return the index position if it is contained, -1 otherwise
 */
static int already_in(int *arr, int fd, int size) {
    for(int i = 0; i < size; i++) {
        if(arr[i] == fd)
            return i;
    }
    return -1;
}

static int m_get_kv_pair(map_t in, char *key, kv_pair *arg) {
    int i, curr;
    h_map* m;

    /* Cast the hashmap */
    m = (h_map *) in;

    /* Find data location */
    curr = hashmap_hash_int(m, key);

    /* Linear probing, if necessary */
    for(i = 0; i < MAX_CHAIN_LENGTH; i++){
        int in_use = m->data[curr].in_use;
        if (in_use == 1){
            if (strcmp(m->data[curr].key, key) == 0){
                *arg = m->data[curr];
                return MAP_OK;
            }
        }

        curr = (curr + 1) % m->table_size;
    }

    arg = NULL;

    /* Not found */
    return MAP_MISSING;
}

/*
 * Doubles the size of the hashmap, and rehashes all the elements
 */
static int hashmap_rehash(map_t in) {
    int i, old_size;
    kv_pair* curr;

    /* Setup the new elements */
    h_map *m = (h_map *) in;
    kv_pair* temp = (kv_pair *) calloc(2 * m->table_size, sizeof(kv_pair));
    if (!temp) return MAP_OMEM;

    /* Update the array */
    curr = m->data;
    m->data = temp;

    /* Update the size */
    old_size = m->table_size;
    m->table_size = 2 * m->table_size;
    m->size = 0;

    /* Rehash the elements */
    for(i = 0; i < old_size; i++) {
        int status;

        if (curr[i].in_use == 0)
            continue;

        status = m_put(m, curr[i].key, curr[i].data);
        if (status != MAP_OK)
            return status;
        /* update subscribers */
        if (curr[i].last_subscriber > 0) {
            kv_pair *p = (kv_pair *) malloc(sizeof(kv_pair));
            m_get_kv_pair(m, curr[i].key, p);
            for (int k = 0; k < curr[i].last_subscriber; k++) {
                m_sub(m, curr[i].key, curr[i].subscribers[k]);
            }
        }
        /* update data history as well */
        iterator it = curr[i].data_history->front;
        while(it) {
            m_pub(m, curr[i].key, it->data);
            it = it->next;
        }
    }

    free(curr);

    return MAP_OK;
}

/*
 * Add a pointer to the hashmap with some key
 */
int m_put(map_t in, char* key, any_t value) {
    int index;
    h_map* m;

    /* Cast the hashmap */
    m = (h_map *) in;

    /* Find a place to put our value */
    index = hashmap_hash(in, key);
    while(index == MAP_FULL){
        if (hashmap_rehash(in) == MAP_OMEM) {
            return MAP_OMEM;
        }
        index = hashmap_hash(in, key);
    }

    /* Set the data */
    m->data[index].data = value;
    m->data[index].key = key;
    if (m->data[index].in_use == 0) {
        m->data[index].in_use = 1;
        m->size++;
        m->data[index].last_subscriber = 0;
        m->data[index].subscribers = (int *) malloc(sizeof(int) * SUBSCRIBER_SIZE);
        m->data[index].data_history = create_queue();
    }

    return MAP_OK;
}

/* Publish data to a key, these data are sent to all the subscribers already
 * inside in the subscribers array of the pair represented by the key
 */
int m_pub(map_t in, char* key, any_t value) {
    int index;
    h_map* m;

    /* Cast the hashmap */
    m = (h_map *) in;

    /* Find a place to put our value */
    index = hashmap_hash(in, key);
    while(index == MAP_FULL){
        if (hashmap_rehash(in) == MAP_OMEM) {
            return MAP_OMEM;
        }
        index = hashmap_hash(in, key);
    }

    /* Set the data */
    m->data[index].data = value;
    m->data[index].key = key;
    if (m->data[index].in_use == 0) {
        m->data[index].in_use = 1;
        m->size++;
        m->data[index].last_subscriber = 0;
        m->data[index].subscribers = (int *) malloc(sizeof(int) * 64);
        m->data[index].data_history = create_queue();
    }
    enqueue(m->data[index].data_history, value);
    for(int i = 0; i < m->data[index].last_subscriber; i++) {
        send(m->data[index].subscribers[i], m->data[index].data, strlen((char *) m->data[index].data), 0);
    }
    return MAP_OK;
}

/*
 * Get your pointer out of the hashmap with a key
 */
int m_get(map_t in, char *key, any_t *arg) {
    int i, curr;
    h_map* m;

    /* Cast the hashmap */
    m = (h_map *) in;

    /* Find data location */
    curr = hashmap_hash_int(m, key);

    /* Linear probing, if necessary */
    for(i = 0; i < MAX_CHAIN_LENGTH; i++){
        int in_use = m->data[curr].in_use;
        if (in_use == 1){
            if (strcmp(m->data[curr].key, key) == 0){
                *arg = (m->data[curr].data);
                return MAP_OK;
            }
        }

        curr = (curr + 1) % m->table_size;
    }

    *arg = NULL;

    /* Not found */
    return MAP_MISSING;
}

/*
 * subscribe to a key in the keyspace, adding a file descriptor representing a
 * socket to the array of subscribers of the pair identified by key.
 */
int m_sub(map_t in, char *key, int fd) {
    int i, curr, last;
    h_map *m;

    m = (h_map *) in;

    curr = hashmap_hash_int(m, key);
    /* Linear probing, if necessary */
    for(i = 0; i < MAX_CHAIN_LENGTH; i++){
        int in_use = m->data[curr].in_use;
        if (in_use == 1) {
            if (strcmp(m->data[curr].key, key) == 0) {
                last = m->data[curr].last_subscriber;
                if(already_in(m->data[curr].subscribers, fd, last) == -1) {
                    m->data[curr].subscribers[last] = fd;
                    m->data[curr].last_subscriber++;
                }
                return MAP_OK;
            }
        }

        curr = (curr + 1) % m->table_size;
    }

    /* Not found */
    return MAP_MISSING;
}

/*
 * unsubscribe from a key removin the file descriptor representing the socket
 * from the array of subscribers of the pair identified by keyspace
 */
int m_unsub(map_t in, char *key, int fd) {
    int i, curr, last;
    h_map *m;

    m = (h_map *) in;

    curr = hashmap_hash_int(m, key);
    /* Linear probing, if necessary */
    for(i = 0; i < MAX_CHAIN_LENGTH; i++){
        int in_use = m->data[curr].in_use;
        if (in_use == 1) {
            if (strcmp(m->data[curr].key, key) == 0) {
                last = m->data[curr].last_subscriber;
                int index = already_in(m->data[curr].subscribers, fd, last);
                if(index != -1) {
                    // ordering doesn't matter
                    m->data[curr].subscribers[index] = m->data[curr].subscribers[last - 1];
                    m->data[curr].last_subscriber--;
                }
                return MAP_OK;
            }
        }

        curr = (curr + 1) % m->table_size;
    }

    /* Not found */
    return MAP_MISSING;
}

/*
 * subscribe to a key in the keyspace, and consume all values previously
 * published to that key according to an offset giving indication on where to
 * start read the depletion.
 */
int m_sub_from(map_t in, char *key, int fd, int index) {
    int i, curr, last;
    h_map *m;

    m = (h_map *) in;

    curr = hashmap_hash_int(m, key);
    /* Linear probing, if necessary */
    for(i = 0; i < MAX_CHAIN_LENGTH; i++){
        int in_use = m->data[curr].in_use;
        if (in_use == 1){
            if (strcmp(m->data[curr].key, key) == 0) {
                last = m->data[curr].last_subscriber;
                if (already_in(m->data[curr].subscribers, fd, last) == -1) {
                    m->data[curr].subscribers[last] = fd;
                    m->data[curr].last_subscriber++;
                }
                if ((size_t) index < m->data[curr].data_history->last && index >= 0) {
                    iterator it = m->data[curr].data_history->front;
                    while(it) {
                        if (index == 0) {
                            int len = strlen((char *) it->data);
                            send(fd, it->data, len, 0);
                        }
                        else index--;
                        it = it->next;
                    }
                }
                return MAP_OK;
            }
        }

        curr = (curr + 1) % m->table_size;
    }

    /* Not found */
    return MAP_MISSING;
}

/*
 * Iterate the function parameter over each element in the hashmap.  The
 * additional any_t argument is passed to the function as its first
 * argument and the pair is the second.
 */
int m_iterate(map_t in, func f, any_t item) {
    int i;

    /* Cast the hashmap */
    h_map* m = (h_map*) in;

    /* On empty hashmap, return immediately */
    if (m_length(m) <= 0)
        return MAP_MISSING;

    /* Linear probing */
    for(i = 0; i < m->table_size; i++)
        if (m->data[i].in_use != 0) {
            /* any_t data = (any_t) (m->data[i].data); */
            kv_pair data = m->data[i];
            int status = f(item, &data);
            if (status != MAP_OK) {
                return status;
            }
        }

    return MAP_OK;
}

/*
 * Remove an element with that key from the map
 */
int m_remove(map_t in, char* key) {
    int i, curr;
    h_map* m;

    /* Cast the hashmap */
    m = (h_map *) in;

    /* Find key */
    curr = hashmap_hash_int(m, key);

    /* Linear probing, if necessary */
    for(i = 0; i < MAX_CHAIN_LENGTH; i++){

        int in_use = m->data[curr].in_use;
        if (in_use == 1){
            if (strcmp(m->data[curr].key,key)==0){
                /* Blank out the fields */
                m->data[curr].in_use = 0;
                free(m->data[curr].subscribers);
                release_queue(m->data[curr].data_history);
                m->data[curr].last_subscriber = 0;

                /* Reduce the size */
                m->size--;
                return MAP_OK;
            }
        }
        curr = (curr + 1) % m->table_size;
    }

    /* Data not found */
    return MAP_MISSING;
}

/*
 * Iterate the function parameter over each element in the hashmap. The
 * additional any_t argument is passed to the function as its first argument and
 * the key of the current pair is the second, used to perform a prefscan operation.
 * A prefscan operation return all keys that starts with a given prefix.
 */
int m_prefscan(map_t in, func f, any_t item, int fd) {
    int i, flag = 0;

    /* Cast the hashmap */
    h_map* m = (h_map*) in;

    /* On empty hashmap, return immediately */
    if (m_length(m) <= 0)
        return MAP_MISSING;

    /* Linear probing */
    for(i = 0; i < m->table_size; i++)
        if (m->data[i].in_use != 0) {
            kv_pair data = m->data[i];
            int status = f(item, data.key);
            if (status == MAP_OK) {
                flag = 1;
                send(fd, data.data, strlen(data.data), 0);
            } else if (flag == 0) return status;
        }

    return MAP_OK;
}

/*
 * Iterate the function parameter over each element in the hashmap. The
 * additional any_t argument is passed to the function as its first argument and
 * the key of the current pair is the second, used to perform a fuzzyscan operation.
 * A fuzzyscan operation return all keys containing a pattern by fuzzy search.
 */
int m_fuzzyscan(map_t in, func f, any_t item, int fd) {
    int i, status, flag = 0;

    /* Cast the hashmap */
    h_map* m = (h_map*) in;

    /* On empty hashmap, return immediately */
    if (m_length(m) <= 0)
        return MAP_MISSING;

    /* Linear probing */
    for(i = 0; i < m->table_size; i++)
        if (m->data[i].in_use != 0) {
            kv_pair data = m->data[i];
            status = f(item, data.key);
            if (status == MAP_OK) {
                flag = 1;
                send(fd, data.data, strlen(data.data), 0);
            }
        }

    if (flag == 0) return status;
    return MAP_OK;
}

/* callback function used with iterate to clean up the hashmap */
static int destroy(any_t t1, any_t t2) {
    kv_pair *kv = (kv_pair *) t2;

    if (kv) {
        // free key field
        if (kv->key)
            free(kv->key);
        // free value field
        if (kv->data)
            free(kv->data);
        // data history queue
        if (kv->data_history)
            release_queue(kv->data_history);
        // free subscribers array
        if (kv->subscribers)
            free(kv->subscribers);
    } else return MAP_MISSING;
    return MAP_OK;
}

/* Deallocate the hashmap */
void m_release(map_t in){
    h_map* m = (h_map*) in;
    m_iterate(m, destroy, NULL);
    free(m->data);
    free(m);
}

/* Return the length of the hashmap */
int m_length(map_t in){
    h_map* m = (h_map *) in;
    if(m != NULL) return m->size;
    else return 0;
}
