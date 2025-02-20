/**
 *
 * Simple tool to read plane registration and type information
 * from various databases
 *
 * Currently, we are just reading the FAA database files.
 * @see http://www.faa.gov/licenses_certificates/aircraft_certification/aircraft_registry/releasable_aircraft_download/
 *
 * Copy the MASTER.txt and ACFTREF.txt files into the runtime directory for this to work.
 * If they are not found, then the lookup will simply be skipped, and will return a NULL record.  It
 * should not affect operation of the client application.
 *
 * Add more later.
 *
 * The MIT License (MIT)
 * Copyright (c) 2019 Bob Jamison
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 * associateddocumentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "planedb.h"

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif



//##########################################################################
//# U T I L I T Y
//##########################################################################

/**
 * Display formatted error message.  Use it like printf():
 *  err("you had %d errors\n", 3);
 */
static void err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

/**
 * Non-allocated buffer for reading strings
 */
#define INSIZE 1024
static char inbuf[INSIZE];

/**
 * Parse up to 8 hex characters into an int.
 * @param str the string to read
 * @param offset the position in the string to start reading
 * @return the int value represented by the hex pattern
 */
static int parse_hex(char *str, int offset)
{
    int val = 0;
    int i;
    for (i=0 ; i < 8 ; i++)
        {
        char c = str[i + offset];
        if (c >= '0' && c <= '9')
            val = (val << 4) + (c - '0');
        else if (c >= 'a' && c <= 'f')
            val = (val << 4) + (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            val = (val << 4) + (c - 'A' + 10);
        else
            break;
        }
    return val;
}

/**
 * Read a decimal integer from a string
 * @param str the string to read
 * @param offset the position in the string to start reading
 * @return the int value represented by the digits
 */
static int parse_int(char *str, int offset)
{
    int val = 0;
    int i;
    for (i=0 ; i < 10 ; i++)
        {
        char c = str[i + offset];
        if (c >= '0' && c <= '9')
            val = (val * 10) + (c - '0');
        else
            break;
        }
    return val;
}


/**
 * Read some characters from a string, delimited by a starting and
 * ending position.  Training white space will be trimmed.
 * @param str the string to read
 * @p0 the starting position for reading
 * @p1 the end position for reading
 * @return a newly-allocated string containing the desired characters.
 *    This should be free()d eventually.
 */
static char *pickup(char *str, int p0, int p1)
{
    int last = p1-1;
    while(last >= p0)
        {
        if (!isspace(str[last]))
            break;
        else
            last--;
        }
    char *word = strndup(&(str[p0]), last-p0+1);
    if (!word)
        err("cannot allocate string");
    return word;
}


//##########################################################################
//# AIRCRAFT TYPE TABLE
//##########################################################################

/**
 * A mapping of type numbers to descriptions
 */
static char *typeTable[] = {
/*0*/ "None",
/*1*/ "Glider",
/*2*/ "Balloon",
/*3*/ "Blimp/Dirigible",
/*4*/ "Fixed wing single engine",
/*5*/ "Fixed wing multi engine",
/*6*/ "Rotorcraft",
/*7*/ "Weight-shift-control",
/*8*/ "Powered Parachute",
/*9*/ "Gyroplane"
};


/**
 * Create a TypeInfo object.
 * @return a pointer to a newly-allocated typeinfo structure if
 *    successful else NULL
 */
static TypeInfo *typeInfoCreate()
{
    TypeInfo *ti = (TypeInfo *)malloc(sizeof(TypeInfo));
    if (!ti)
        return NULL;
    ti->id           = 0;
    ti->manufacturer = NULL;
    ti->model        = NULL;
    ti->type         = 0;
    ti->nrseats      = 0;
    return ti;
}

/**
 * Delete a TypeInfo object, and deallocate any strings it might have.
 * @return TRUE always
 */
static int typeInfoDelete(TypeInfo *ti)
{
    if (!ti)
        return TRUE;
    if (ti->manufacturer) free(ti->manufacturer);
    if (ti->model)        free(ti->model);
    free(ti);
    return TRUE;
}

/**
 * Print a TypeInfo object.
 * @return TRUE always
 */
static int typeInfoPrint(TypeInfo *ti)
{
    if (!ti)
        printf("None\n");
	printf("  ## Type\n");
    if (ti->manufacturer)  printf("    Manufacturer   : %s\n", ti->manufacturer);
    if (ti->model)         printf("    Model name     : %s\n", ti->model);
                           printf("    Type           : %d - %s\n", ti->type, typeTable[ti->type]);
                           printf("    Seats          : %d\n", ti->nrseats);

    return TRUE;
}


/**
 * Look up a TypeInfo record by its model number.  Use caching.
 * @param db this
 * @param id the model number to look for.
 * @return a TypeInfo object if the model is found, else NULL.
 */
static TypeInfo *type_lookup(PlaneDb *db, int id)
{
    TypeInfo *rec = db->types;
    if (id == db->last_model)
        return db->last_ti;
    while (rec)
        {
        if (rec->id == id)
            {
            db->last_model = id;
            db->last_ti = rec;
            return rec;
            }
        rec = rec->next;
        }
    return NULL;
}

/**
 * Load TypeInfo data from a file.  This is currently coded for the FAA
 * ACRFTREF.txt file
 * @param db this
 * @return TRUE if successful, else FALSE
 */
static int load_types(PlaneDb *db)
{
	char *fname = "ACFTREF.txt";
    FILE *f = fopen(fname, "r");
    if (!f)
        {
        err("cannot open file '%s'", fname);
        return FALSE;
        }
    TypeInfo *prev = NULL;
    while (!feof(f))
        {
        char *str = fgets(inbuf, INSIZE, f);
        if (!str)
            continue;
        if (strlen(str) < 68)
            continue;
        TypeInfo *ti = typeInfoCreate();
        if (!ti)
            {
            err("cannot allocate TypeInfo");
            fclose(f);
            return FALSE;
            }
        ti->id = parse_int(str, 0);
        ti->manufacturer = pickup(str, 8, 38);
        if (!ti->manufacturer)
            return FALSE;
        ti->model = pickup(str, 39, 59);
        if (!ti->model)
            return FALSE;
        ti->type = parse_int(str, 60);
        ti->nrseats = parse_int(str, 72);
        if (prev)
            prev->next = ti;
        else
            db->types = ti;
        prev = ti;
        }
    fclose(f);
    return TRUE;
}

//##########################################################################
//# REGISTRATION DB
//##########################################################################


/**
 * Create a PlaneInfo object.
 * @return a pointer to a newly-allocated PlaneInfo object if
 *    successful else NULL
 */
static PlaneInfo *planeInfoCreate()
{
    PlaneInfo *pi = (PlaneInfo *)malloc(sizeof(PlaneInfo));
    if (!pi)
        return NULL;
    pi->id         = 0;
    pi->nnum       = NULL;
    pi->model      = 0;
    pi->registrant = NULL;
    return pi;
}

/**
 * Delete a PlaneInfo object, and deallocate any strings it might have.
 * @return TRUE always
 */
static int planeInfoDelete(PlaneInfo *pi)
{
    if (!pi)
        return TRUE;
    if (pi->nnum)       free(pi->nnum);
    if (pi->registrant) free(pi->registrant);
    free(pi);
    return TRUE;
}

/**
 * Print a PlaneInfo object.
 * @param db "this".   Supplied so that the model number can be looked up.
 * @param pi the PlaneInfo to print.
 * @return TRUE always
 */
int planeInfoPrint(PlaneDb *db, PlaneInfo *pi)
{
    if (!pi)
        printf("None\n");
	printf("  ## Registration\n");
    if (pi->nnum)       printf("    N-Number       : %s\n", pi->nnum);
    if (pi->registrant) printf("    Registrant     : %s\n", pi->registrant);
    if (pi->model)      printf("    Model          : %d\n", pi->model);
    TypeInfo *ti = type_lookup(db, pi->model);
    if (!ti)
        printf("No model info\n");
    else
        typeInfoPrint(ti);
    return TRUE;
}


/**
 * Load PlaneInfo data from a file.  This is currently coded for the FAA
 * MASTER.txt file
 * @param db this
 * @return TRUE if successful, else FALSE
 */
static int load_planes(PlaneDb *db)
{
	char *fname = "MASTER.txt";
    FILE *f = fopen(fname, "r");
    if (!f)
         {
         err("cannot open file '%s'", fname);
         return FALSE;
         }
    PlaneInfo *prev = NULL;
    while (!feof(f))
        {
        char *str = fgets(inbuf, INSIZE, f);
        if (!str)
            continue;
        if (strlen(str) < 610)
            continue;
        PlaneInfo *pi = planeInfoCreate();
        if (!pi)
            {
            err("cannot allocate PlaneInfo");
            fclose(f);
            return FALSE;
            }
        pi->id = parse_hex(str, 601);
        pi->nnum = pickup(str, 0, 5);
        if (!pi->nnum)
            return FALSE;
        pi->model = parse_int(str, 37);
        pi->registrant = pickup(str, 58, 107);
        if (!pi->registrant)
            return FALSE;
        if (prev)
            prev->next = pi;
        else
            db->planes = pi;
        prev = pi;
        }
    fclose(f);
    return TRUE;
}


//##########################################################################
//# MAIN DATABASE
//##########################################################################





/**
 * Create and initialize a PlaneDb context.
 * @return a newly-allocated PlaneDb object if successful, else NULL.  This
 *   value should be passed to planedb_close() when procesing is completed.
 */
PlaneDb *planedb_init()
{
    PlaneDb *db = (PlaneDb *) malloc(sizeof(PlaneDb));
    if (!db)
        return NULL;
    db->types  = (TypeInfo *)0;
    db->planes = (PlaneInfo *)0;
    if (!load_types(db))
        {
        planedb_close(db);
        return NULL;
        }
    if (!load_planes(db))
        {
        planedb_close(db);
        return NULL;
        }
    return db;
}


/**
 * Search the registration database for a PlaneInfo record with
 * given ICAO id.
 * @param db the PlaneDb context.
 * @param icao the ICAO hex string to look for.
 * @return the PlaneInfo object associated with the given ICAO
 *   if successful, else NULL.
 */
PlaneInfo *planedb_lookup(PlaneDb *db, char *icao)
{
    if (!db || !icao)
        return NULL;
    int id = parse_hex(icao, 0);
    if (id == db->last_icao)
        return db->last_pi;
    PlaneInfo *rec = db->planes;
    while (rec)
        {
        if (rec->id == id)
            {
            db->last_icao = id;
            db->last_pi = rec;
            return rec;
            }
        rec = rec->next;
        }
    return NULL;
}

/**
 * Delete the PlaneDb object, and any allocated resources it might have
 * @param db the PlaneDb context to delete.
 * @return TRUE always
 */
int planedb_close(PlaneDb *db)
{
    if (!db)
        return FALSE;
    TypeInfo *tnext;
    TypeInfo *trec;
    for (trec = db->types ; trec ; trec = tnext)
        {
        tnext = trec->next;
        typeInfoDelete(trec);
        }
    PlaneInfo *pnext;
    PlaneInfo *prec;
    for (prec = db->planes ; prec ; prec = pnext)
        {
        pnext = prec->next;
        planeInfoDelete(prec);
        }
    free(db);
    return TRUE;
}


/**
 * Compile with -DSTANDALONE to create a standalone tool.
*/
#ifdef STANDALONE

/**
 * Show proper command-line invocation of this app
 */
static void usage()
{
    printf("Usage:  planedb <icao code>\n");
}


/**
 * Perform a DB lookup of the given ICAO string.
 * Print if found
 * @param icao the ICAO string for the search
 * @return the PlaneInfo associated with the ICAO string
 *    if successful, else NULL.
 */
int lookup(char *icao)
{
    printf("%s\n", icao);
    PlaneDb *db = planedb_init();
    if (!db)
        {
        printf("Could not initialize plane database\n");
        return FALSE;
        }
    PlaneInfo *pi = planedb_lookup(db, icao);
    if (!pi)
        printf("Plane not found\n");
    else
        planeInfoPrint(db, pi);
    planedb_close(db);
}


/**
 * Good old main() function.  O how that takes me back.
 */
int main(int argc, char **argv)
{
    if (argc == 2)
        lookup(argv[1]);
    else
        usage();
    return 0;
}

#endif
