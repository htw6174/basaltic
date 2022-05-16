#include <stdio.h>
#include <stdlib.h>
#include "htw_core.h"
#include "htw_geomap.h"

void testMap() {
    Map *myMap = createMap(15, 5);
    printMap(myMap);
}

int main(int argc, char *argv[])
{
    testMap();
    return 0;
}
