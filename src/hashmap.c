/**
 * ${project-name} - a GNU/Linux console-based file manager
 *
 * Assaciative arrays support
 *
 * Copyright 2008 Sergey I. Sharybin <g.ulairi@gmail.com>
 * Copyright 2008 Alex A. Smirnov <sceptic13@gmail.com>
 *
 * This program can be distributed under the terms of the GNU GPL.
 * See the file COPYING.
 */

#include "hashmap.h"

#include <wchar.h>

/********
 * Macroses
 */

/**
 * Calculate value of hash function for key
 */
#define HASH(_hashmap, _key) \
  (_hashmap->hash_func (_hashmap, _key))

/**
 * Compares two keys
 */
#define KEYCMP(_hashmap, _a, _b) \
  (_hashmap->key_comparator (_a, _b))

#define WCK_MAGICK_CONST 43

/********
 * Internal stuff
 */

/**
 * Return entry of hash map with specified key
 *
 * @param __hashmap - in which hash map search element
 * @param __key - key of element to return
 * @param __hash - hash value of element (need for optimized calls)
 */
static hashmap_entry_t*
get_entry_full (const hashmap_t *__hashmap, const void *__key, hash_t __hash)
{
  if (!__hashmap)
    {
      return NULL;
    }

  hashmap_entry_t *cur = __hashmap->data[__hash];

  while (cur)
    {
      if (!KEYCMP (__hashmap, cur->key, __key))
        {
          return cur;
        }
      cur = cur->next;
    }

  return NULL;
}

/**
 * Return entry of hash map with specified key
 *
 * @param __hashmap - in which hash map search element
 * @param __key - key of element to return
 */
static hashmap_entry_t*
get_entry (const hashmap_t *__hashmap, const void *__key)
{
  if (!__hashmap)
    {
      return NULL;
    }

  return get_entry_full (__hashmap, __key, HASH (__hashmap, __key));
}

/**
 * Free element of hash map
 *
 * @param __hashmap - for which map element belongs to
 * @param __element - element to be freed
 * @param __deleter - deleter which will be applied on stored data
 */
static void
free_element (hashmap_t *__hashmap, hashmap_entry_t *__element,
              hashmap_deleter __deleter)
{
  if (!__hashmap || !__element)
    {
      return;
    }

  /* Destroy element */
  if (__deleter)
    {
      __deleter (__element->value);
    }

  /* Destroy key */
  if (__hashmap->key_deleter)
    {
      __hashmap->key_deleter (__element->key);
    }

  free (__element);
}

/********
 * User's backend
 */

/**
 * Create new hash map
 *
 * @param __hash_func - function for calculating hash-values
 * of elements' keys
 * @param __deleter - default deleter for elements
 * @param __key_comparator - comparator of two keys
 * @param __key_duplicator - function, which duplicates key
 * @param __data_length - length of array to store elements
 * @return pointer to new hash map if succeed, NULL otherwise
 */
hashmap_t*
hashmap_create (hashmap_hash_func __hash_func,
                hashmap_deleter __deleter,
                hashmap_key_deleter __key_deleter,
                hashmap_keycmp __key_comparator,
                hashmap_keydup __key_duplicator,
                __u32_t __data_length)
{
  hashmap_t *res;

  /* Hash map couldn't work without hash function */
  /* It also couldn't work if array is zero-length */
  /* And also it couldn't work without functions manages keys */
  if (!__hash_func || !__data_length ||
      !__key_comparator || !__key_duplicator)
    {
      return NULL;
    }

  /* Allocate memory for hash map */
  MALLOC_ZERO (res, sizeof (hashmap_t));

  MALLOC_ZERO (res->data, sizeof (hashmap_entry_t*) * __data_length);

  res->data_length = __data_length;

  res->hash_func = __hash_func;
  res->deleter = __deleter;
  res->key_comparator = __key_comparator;
  res->key_duplicator = __key_duplicator;

  return res;
}

/**
 * Destroy hashmap
 *
 * @param __hashmap - hash map to be destroyed
 */
void
hashmap_destroy (hashmap_t *__hashmap)
{
  if (!__hashmap)
    {
      return;
    }

  hashmap_destroy_full (__hashmap, __hashmap->deleter);
}

/**
 * Destroy hashmap
 *
 * @param __hashmap - hash map to be destroyed
 * @param __deleter - deleter of each element in map
 */
void
hashmap_destroy_full (hashmap_t *__hashmap, hashmap_deleter __deleter)
{
  __u32_t i;
  hashmap_entry_t *cur, *tmp;

  if (!__hashmap)
    {
      return;
    }

  /* Go through all elements in data */
  for (i = 0; i < __hashmap->data_length; i++)
    {
      cur = __hashmap->data[i];
      while (cur)
        {
          tmp = cur;
          cur = cur->next;
          free_element (__hashmap, tmp, __deleter);
        }
    }

  /* Free memory used by data */
  free (__hashmap->data);

  /* Free memory used by map */
  free (__hashmap);
}

/**
 * Set value of element, which has specified key.
 * If element with such key does not exists, creates new entry in
 * hash map and stores value in it.
 *
 * @param __hashmap - in which hash map element will be set
 * @param __key - key of element
 * @param __value - data to be stored
 */
void
hashmap_set (hashmap_t *__hashmap, const void *__key, const void *__value)
{
  if (!__hashmap)
    {
      return;
    }

  hashmap_set_full (__hashmap, __key, __value, __hashmap->deleter);
}

/**
 * Set value of element, which has specified key.
 * If element with such key does not exists, creates new entry in
 * hash map and stores value in it.
 *
 * @param __hashmap - in which hash map element will be set
 * @param __key - key of element
 * @param __value - data to be stored
 * @param __deleter - deleter for element. Used if element with
 * specified key is already defined. In this case previously set vlaue
 * will be destroued with 
 */
void
hashmap_set_full (hashmap_t *__hashmap, const void *__key,
                  const void *__value, hashmap_deleter __deleter)
{
  if (!__hashmap)
    {
      return;
    }

  hash_t h = HASH (__hashmap, __key);
  hashmap_entry_t *elem = get_entry_full (__hashmap, __key, h);

  if (elem)
    {
      /* Element has been already set */

      /* Delete old value */
      if (__deleter)
        {
          __deleter (elem->value);
        }

      /* Set new value */
      elem->value = (void*) __value;
    }
  else
    {
      /* Need to create new entry */
      hashmap_entry_t *new = malloc (sizeof (hashmap_entry_t));

      new->key = __hashmap->key_duplicator (__key);

      new->value = (void*) __value;
      new->next = __hashmap->data[h];

      __hashmap->data[h] = new;
    }
}

/**
 * Get value of element, specified by it's key
 *
 * @param __hashmap - from which hash map value should be gotten
 * @param __key - key of element
 * @return value of element specified by it's key it exists, or
 * NULL otherwise
 */
void*
hashmap_get (const hashmap_t *__hashmap, const void *__key)
{
  hashmap_entry_t *elem = get_entry (__hashmap, __key);

  if (elem)
    {
      return elem->value;
    }

  return NULL;
}

/**
 * Test if element with specified key defined in hash map
 *
 * @param __hashmap - in which hashmap do this testing
 * @param __key - key of element, which presence should be tested
 * @return TRUE is element is defined, FALSE otherwise
 */
BOOL
hashmap_isset (const hashmap_t *__hashmap, const void *__key)
{
  return get_entry (__hashmap, __key) != NULL;
}

/**
 * Uset element of hash map specified by its key
 *
 * @param __hashmap - from which hasm map unset element
 * @param __key - key of element to unset
 * @return 0 if successful, otherwise -1
 */
int
hashmap_unset (hashmap_t *__hashmap, const void *__key)
{
  if (!__hashmap)
    {
      return -1;
    }

  hashmap_unset_full (__hashmap, __key, __hashmap->deleter);
  return 0;
}

/**
 * Uset element of hash map specified by its key
 *
 * @param __hashmap - from which hasm map unset element
 * @param __key - key of element to unset
 * @param __deleter - deleter of element
 */
void
hashmap_unset_full (hashmap_t *__hashmap, const void *__key,
                    hashmap_deleter __deleter)
{
  if (!__hashmap)
    {
      return;
    }

  hash_t h = HASH (__hashmap, __key);
  hashmap_entry_t *cur = __hashmap->data[h], *prev = NULL;

  while (cur)
    {
      if (!KEYCMP (__hashmap, cur->key, __key))
        {
          /* Drop element from list */
          if (prev)
            {
              prev->next = cur->next;
            }
          else
            {
              __hashmap->data[h] = cur->next;
            }

          free_element (__hashmap, cur, __deleter);

          return;
        }

      prev = cur;
      cur = cur->next;
    }
}

/**
 * Uset all elements in hash map
 *
 * @param __hashmap - from which hasm map unset element
 */
void
hashmap_unset_all (hashmap_t *__hashmap)
{
  if (!__hashmap)
    {
      return;
    }

  hashmap_unset_all_full (__hashmap, __hashmap->deleter);
}

/**
 * Uset all elements in hash map
 *
 * @param __hashmap - from which hasm map unset element
 * @param __deleter - deleter of element
 */
void
hashmap_unset_all_full (hashmap_t *__hashmap, hashmap_deleter __deleter)
{
  __u32_t i;
  hashmap_entry_t *cur, *tmp;

  if (!__hashmap)
    {
      return;
    }

  /* For each value of hash... */
  for (i = 0; i < __hashmap->data_length; i++)
    {
      /* ...go through list and free each element */
      cur = __hashmap->data[i];
      while (cur)
        {
          tmp = cur;
          cur = cur->next;
          free_element (__hashmap, tmp, __deleter);
        }
      __hashmap->data[i] = 0;
    }
}

/*********
 * Wrappers for wchar-ed keys
 */

/**
 * Calculate hash function for wchar-ed keys
 *
 * @param __hashmap - hashmap for which key belongs to
 * @param __key - key to caclucate hash for
 * @return hash of key
 */
static hash_t
wck_hash_func (const hashmap_t *__hashmap, const void *__key)
{
  hash_t h = 0;
  size_t i, n;
  wchar_t *key = (wchar_t*) __key;

  for (i = 0, n = wcslen (key); i < n; i++)
    {
      h = (h * WCK_MAGICK_CONST + key[i]) % __hashmap->data_length;
    }

  return h;
}

/**
 * Destroy key
 *
 * @param __key - key to be destroyed
 */
static void
wck_key_deleter (void *__key)
{
  free (__key);
}

/**
 * Compare two wchar-ed keys
 *
 * @param __key1, __key2 - keys to compare
 * @return see wcscmp()
 */
static short
wck_key_comparator (const void *__key1, const void *__key2)
{
  return wcscmp (__key1, __key2);
}

/**
 * Duplicate specified key
 *
 * @param __key - key to be duplicated
 * @return clone of key
 */
static void*
wck_keydup (const void *__key)
{
  return wcsdup (__key);
}

/**
 * Create new hashmap with wide-chared keys
 *
 * @param __deleter - deleter of elements
 * @param __data_length - length of array for data
 * @return new hash map
 */
hashmap_t*
hashmap_create_wck (hashmap_deleter __deleter,
                    __u32_t __data_length)
{
  return hashmap_create (wck_hash_func, __deleter, wck_key_deleter,
                         wck_key_comparator, wck_keydup, __data_length);
}
