#ifndef VECTOR_HEADER__H_
#define VECTOR_HEADER__H_

struct vector;

extern const struct vector_api vector;

struct vector_api {
    struct vector * (*new)(size_t initial);
    void (*delete)(struct vector *);

    /* Accessors */
    size_t (*size)(struct vector *);  /* Return number of elements in vector */

    /* Stack based access */
    void * (*top)(struct vector *); /* Return top element on the vector (with push/pop) */
    void * (*push)(struct vector *, void * data); /* Append to end of vector */
    void * (*pop)(struct vector *);               /* Remove from end of vector */

    /* Get/Set index (like bracket notation) */
    void * (*index)(struct vector *, size_t index);
    void * (*sindex)(struct vector *, size_t index, void * data);
};

#endif
