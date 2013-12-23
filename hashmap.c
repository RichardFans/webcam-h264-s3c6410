
#include <stdlib.h>
#include <stddef.h>
#include "hashmap.h"

static Hashmap_Item create_item(void *key, void *value)
{
	Hashmap_Item item = (Hashmap_Item) malloc(sizeof(struct hashmap_item));
	item->key = key;
	item->value = value;
	return item;
}

static int hash(void * ptr, int size)
{
	return (int) (((long) ptr) % size);
}

static void rehash(Hashmap hm)
{
	Hashmap_Item * current, * buckets;
	Hashmap_Item item, next;
	size_t s, size;
	int i, index;

	current = hm->buckets;
	s = hm->size;
	size = s << 1;

	buckets = calloc(size, sizeof(Hashmap_Item));

	for (i = 0; i < s; i++) {
		for (item = current[i]; item != NULL; item = next) {
			index = hash(item->key, size);
			next = item->next;
			item->next = buckets[index];
			buckets[index] = item;
		}
	}

	free(hm->buckets);
	hm->buckets = buckets;
	hm->size = size;
}

Hashmap hashmap_create(size_t size)
{
	Hashmap hm = (Hashmap) malloc(sizeof(struct hashmap));
	hm->buckets = calloc(size, sizeof(Hashmap_Item));
	hm->size = size;
	hm->count = 0;
	return hm;
}

void * hashmap_get(Hashmap hm, void *key)
{
	int index = hash(key, hm->size);
	Hashmap_Item item;

	for (item = hm->buckets[index]; item != NULL; item = item->next) {
		if (item->key == key)
			return item->value;
	}

	return NULL;
}

void hashmap_set(Hashmap hm, void *key, void *value)
{
	Hashmap_Item item;
	Hashmap_Item * p;
	int index = hash(key, hm->size);

	p = &(hm->buckets[index]);

	for (item = *p; item != NULL; item = item->next) {
		if (item->key == key) {/* key already exists */
			item->value = value;
			return;
		}
	}

	item = *p;
	(*p) = create_item(key, value);
	(*p)->next = item;

	if (++hm->count >= hm->size * 3 / 4)
		rehash(hm);
}

void hashmap_each(Hashmap hm, void fn(void *, void *, int))
{
	int i;
	Hashmap_Item item;

	for (i = 0; i < hm->size; i++) {
		for (item = hm->buckets[i]; item != NULL; item = item->next) {
			(*fn)(item->value, item->key, i);
		}
	}
}

void hashmap_delete(Hashmap hm, void * key)
{
	int index = hash(key, hm->size);
	Hashmap_Item item, next;

	item = hm->buckets[index];

	if (item == NULL)
		return;

	if (item->next == NULL) {
		free(item);
		hm->buckets[index] = NULL;
		hm->count--;
		return;
	}

	while (item != NULL) {
		next = item->next;
		if (next != NULL && next->key == key) {
			item->next = next->next;
			hm->count--;
			free(next);
			return;
		}
		item = next;
	}
}

void hashmap_free(Hashmap hm)
{
	int i;
	Hashmap_Item item, next;

	for (i = 0; i < hm->size; i++) {
		for (item = hm->buckets[i]; item != NULL;) {
			next = item->next;
			free(item);
			item = next;
		}
	}

	free(hm->buckets);
	free(hm);
}
