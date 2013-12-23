
#ifndef HASHMAP_HEADER
#define HASHMAP_HEADER

typedef struct hashmap_item *Hashmap_Item;

struct hashmap_item {
	Hashmap_Item next;
	void *key;
	void *value;
};

struct hashmap {
	Hashmap_Item * buckets;
	size_t size;
	size_t count;
};

typedef struct hashmap *Hashmap;

Hashmap hashmap_create(size_t);
void * hashmap_get(Hashmap, void *);
void hashmap_set(Hashmap, void *, void *);
void hashmap_each(Hashmap, void fn(void *, void *, int));
void hashmap_delete(Hashmap, void *);
void hashmap_free(Hashmap);

#endif
