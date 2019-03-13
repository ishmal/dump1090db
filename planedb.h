#ifndef _PLANEDB_H_
#define _PLANEDB_H_

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


#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
typedef struct TypeInfoDef TypeInfo;    
struct TypeInfoDef
{
    struct TypeInfoDef *next;
    int id;        //the manufacturer, model, and series code as an int
    char *manufacturer;  // Manufacturer name
    char *model;         // Model name
    int type;            // Integer type
    int nrseats;         // Max number of seats
};

/**
 *  PlaneInfo data record
 */
typedef struct PlaneInfoDef PlaneInfo;
struct PlaneInfoDef
{
    struct PlaneInfoDef *next;
    int  id;        //the icao code in int form
    char *nnum;     // The N-Number string
    int  model;     // The model name
    char *registrant; // Name of the registrant
};

/**
 *  The PlaneDb context.   Created by planedb_init(), used with planedb_liikup(),
 *  and destroyed by planedb_close()
 */
typedef struct
{
    TypeInfo    *types;
    int     last_model;
    TypeInfo  *last_ti;
    PlaneInfo  *planes;
    int      last_icao;
    PlaneInfo *last_pi;
} PlaneDb;


/**
 * Create and initialize a PlaneDb context.
 * @return a newly-allocated PlaneDb object if successful, else NULL.  This
 *   value should be passed to planedb_close() when procesing is completed.
 */
PlaneDb *planedb_init();

/** 
 * Search the registration database for a PlaneInfo record with
 * given ICAO id.    
 * @param db the PlaneDb context.
 * @param icao the ICAO hex string to look for.
 * @return the PlaneInfo object associated with the given ICAO
 *   if successful, else NULL.
 */
PlaneInfo *planedb_lookup(PlaneDb *db, char *icao);

/**
 * Delete the PlaneDb object, and any allocated resources it might have
 * @param db the PlaneDb context to delete.
 * @return TRUE always
 */
int planedb_close(PlaneDb *db);


/**
 * Print a PlaneInfo object.
 * @param db "this".   Supplied so that the model number can be looked up.
 * @param pi the PlaneInfo to print.
 * @return TRUE always
 */
int planeInfoPrint(PlaneDb *db, PlaneInfo *pi);


#ifdef __cplusplus
}
#endif



#endif /* _PLANEDB_H_ */

