/*
 * HashMap designed to allow placement, movement, and location lookup for another array of data
 * Functionally similar to C++'s unordered_multimap
 *
 * The links array functions like a single linked list, but instead of storing data the array index of each link is used
 * Each link in the links array corresponds to an element of the underlying array
 * Each pointer in the hashSlots array points to the link with the same index as the first item at the hashed location, or NULL if nothing is there
 * Links point to the link with the same index as the next item at the same hashed location
 */
#include "htw_geomap.h"
#include "htw_core.h"
#include "htw_random.h"

static size_t getItemIndex(htw_SpatialStorage *ss, htw_Link *link);

htw_SpatialStorage *htw_geo_createSpatialStorage(size_t maxItemCount) {
    htw_SpatialStorage *newStorage = calloc(1, sizeof(htw_SpatialStorage));
    newStorage->maxItemCount = maxItemCount;
    // Gives a load factor between 0.25 and 0.5
    newStorage->hashSlotCount = htw_nextPow(maxItemCount * 2); // TODO: allow some control over hash table size; this is a decent compromise
    newStorage->hashBitMask = newStorage->hashSlotCount - 1; // bits up to hashSlotCount = 1
    newStorage->links = calloc(maxItemCount, sizeof(htw_Link));
    newStorage->hashSlots = calloc(newStorage->hashSlotCount, sizeof(htw_Link*));
    return newStorage;
}

void htw_geo_spatialInsert(htw_SpatialStorage *ss, htw_geo_GridCoord coord, size_t itemIndex) {
    size_t hashSlotIndex = xxh_hash2d(0, coord.x, coord.y) & ss->hashBitMask;
    htw_Link **head = &ss->hashSlots[hashSlotIndex];
    while (*head != NULL) {
        head = &(*head)->next;
    }
    *head = &ss->links[itemIndex];
}

void htw_geo_spatialRemove(htw_SpatialStorage *ss, htw_geo_GridCoord coord, size_t itemIndex) {
    size_t hashSlotIndex = xxh_hash2d(0, coord.x, coord.y) & ss->hashBitMask;
    htw_Link **head = &ss->hashSlots[hashSlotIndex];
    while (*head != NULL) {
        size_t headIndex = getItemIndex(ss, *head);
        if (headIndex == itemIndex) {
            htw_Link *next = (*head)->next;
            (*head)->next = NULL;
            *head = next;
            break;
        }
        head = &(*head)->next;
    }
}

void htw_geo_spatialMove(htw_SpatialStorage *ss, htw_geo_GridCoord oldCoord, htw_geo_GridCoord newCoord, size_t itemIndex) {
    htw_geo_spatialRemove(ss, oldCoord, itemIndex);
    htw_geo_spatialInsert(ss, newCoord, itemIndex);
}

size_t htw_geo_spatialItemCountAt(htw_SpatialStorage *ss, htw_geo_GridCoord coord) {
    size_t hashSlotIndex = xxh_hash2d(0, coord.x, coord.y) & ss->hashBitMask;
    size_t itemCount = 0;
    htw_Link **head = &ss->hashSlots[hashSlotIndex];
    while (*head != NULL) {
        head = &(*head)->next;
        itemCount++;
    }
    return itemCount;
}

size_t htw_geo_spatialFirstIndex(htw_SpatialStorage *ss, htw_geo_GridCoord coord) {
    size_t hashSlotIndex = xxh_hash2d(0, coord.x, coord.y) & ss->hashBitMask;
    htw_Link *head = ss->hashSlots[hashSlotIndex];
    if (head != NULL) {
        return getItemIndex(ss, head);
    } else {
        // FIXME: can't return null or false, how to correctly convey when there is nothing at the provided coord?
        // Should either be caller's responsibility to check that count at this location > 0 first
        // Or should return '-1' and rely on caller to check for OOB indicies
        return -1;
    }
}

size_t htw_geo_spatialNextIndex(htw_SpatialStorage *ss, size_t previousIndex) {

}

static size_t getItemIndex(htw_SpatialStorage *ss, htw_Link *link) {
    return link - ss->links;
}
