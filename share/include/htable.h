#ifndef HTABLE_HEADER__H_
#define HTABLE_HEADER__H_

extern const struct htable_api htable;

struct htable;

struct htable_api {
    /* Create/destroy hash tables */
    struct htable * (*new)(size_t init_sz);
    void (*free)(struct htable * htab);
    void (*free_cb)(struct htable * htab, void (*callback)(const char * key, void * data));

    /* Store/Lookup/Delete items */
    void * (*store)(struct htable * htab, const char * key, void * data);
    void * (*lookup)(struct htable * htab, const char * key);
    void * (*delete)(struct htable * htab, const char * key);

    /* Get load factor of hashtable (for performance analysis) */
    int (*load)(struct htable * htab);
    /* 
     * direct - return number of items in table that are directly indexed (hash lookup only)
     * linked - return number of items in table that require traversing a linked list to access
     * total  - return total number of items in the hash table  (should be direct + linked)
     */
    size_t (*direct)(struct htable * htab);
    size_t (*linked)(struct htable * htab);
    size_t (*total)(struct htable * htab);
    size_t (*size)(struct htable * htab); /* actual size of hash table */

    /* Iterate over items (no gauranteed order) and call callback for each item */
    void (*foreach)(struct htable * htab, int (*callback)(const char * key, void * data));
};

#endif
