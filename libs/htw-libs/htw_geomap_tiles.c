#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "htw_core.h"
#include "htw_geomap.h"

// TODO: Replace with double array; initialize with maximum allowed cell types
// Being able to use a list is convenient, but not as practical
typedef struct {
    int id;
    char* name;
} tileDef;

static List *tileDefs;

/*
 Tile definitions format (simplified v1):
 Comments are from any unquoted '#' to end of line
 One definition per line
 [id] "[descriptive name]"
 */
// Return value is a unique ID that can be used to unload definitions later
// TODO: Sort list after parsing whole file; screen for duplicate ids; consider changing spec to automatically assign ids?
void *htw_loadTileDefinitions (char *path) {
    FILE *defs = fopen(path, "r");
    tileDefs = createList(sizeof( tileDef ), 0);
    tileDef *def;

    int id = 0;
    int currentNum = 0;

    int nameCursor = 0;
    char name[HTW_GEO_TILE_NAME_MAX_LENGTH];

    char cursor;
    int step = 0;

    while ((cursor = getc(defs)) != EOF) {
        switch (step) {
            case 0: { // search for comment or start of definition
                if (cursor == '#') // comment; skip to next line
                    step = -1;
                if (isdigit(cursor)) { // start of id found
                    def = malloc(sizeof( tileDef ));
                    id = charToInt(cursor);
                    step++;
                }
                break;
            }
            case 1: { // parse digits in id
                if (isdigit(cursor)) {
                    // Parse current digit as int
                    currentNum = charToInt(cursor);
                    // Promote earlier numbers to next 10's place and add current digit
                    id = (id * 10) + currentNum;
                }
                else { // end of id digits; add to def and reset
                    def->id = id;
                    id = 0;
                    currentNum = 0;
                    step++;
                }
                break;
            }
            case 2: { // search for start of name
                if (cursor == '"') // start of name found
                    step++;
                break;
            }
            case 3: {
                if (cursor != '"') {
                    name[nameCursor++] = cursor;
                }
                else { // end of name; add definition, reset, and skip to end of line
                    name[nameCursor] = '\0';
                    char *defName = malloc(sizeof(char) * nameCursor);
                    strcpy(defName, name);
                    def->name = defName;
                    nameCursor = 0;
                    pushItem( tileDefs, def);
                    printf("Added cell definition for: %s (%i)\n", def->name, def->id);
                    step = -1;
                }
                break;
            }
            default: {
                if (cursor == '\n') // Start searching for next comment or definition
                    step = 0;
            }
        }

    }

    return malloc(1);
}

htw_geo_MapTile *htw_createTile (int id) {
    htw_geo_MapTile *c = malloc(sizeof(htw_geo_MapTile));
    c->id = id;
    c->content = 0;
    return c;
}

// TODO: Ensure that list is sorted before using this function
char *htw_getTileName (int id) {
    tileDef *targetDef = ( tileDef*)getItem( tileDefs, id);
    return targetDef->name;
}
