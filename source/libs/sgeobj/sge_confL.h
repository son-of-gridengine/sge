#ifndef __SGE_CONFL_H
#define __SGE_CONFL_H

/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "sge_boundaries.h"
#include "cull.h"

#ifdef  __cplusplus
extern "C" {
#endif

/* *INDENT-OFF* */ 

enum {
   CONF_hname = CONF_LOWERBOUND,
   CONF_version,
   CONF_entries                 /* Points to CF_Type list */
};

ILISTDEF(CONF_Type, Configuration, SGE_CONFIG_LIST)
   SGE_HOST(CONF_hname, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SPOOL)
   SGE_ULONG(CONF_version, CULL_DEFAULT | CULL_SPOOL)
   SGE_LIST(CONF_entries, CF_Type, CULL_DEFAULT | CULL_SPOOL)
LISTEND 

NAMEDEF(CONFN)
   NAME("CONF_hname")
   NAME("CONF_version")
   NAME("CONF_entries")
NAMEEND

#define CONFS sizeof(CONFN)/sizeof(char*)

/*
 * configuration list 
 */
enum {
   CF_name = CF_LOWERBOUND,  /* name of configuration element */
   CF_value,                 /* value of configuration element */
   CF_sublist,               /* sub-list of type CF_Type */
   CF_local                  /* global value can be overridden */
};

SLISTDEF(CF_Type, ConfigEntry)
   SGE_STRING(CF_name, CULL_PRIMARY_KEY | CULL_HASH | CULL_UNIQUE | CULL_SUBLIST)
   SGE_STRING(CF_value, CULL_DEFAULT | CULL_SUBLIST)
   SGE_LIST(CF_sublist, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_ULONG(CF_local, CULL_DEFAULT)
LISTEND 

NAMEDEF(CFN)
   NAME("CF_name")
   NAME("CF_value")
   NAME("CF_sublist")
   NAME("CF_local")
NAMEEND

/* *INDENT-ON* */

#define CFS sizeof(CFN)/sizeof(char*)
#ifdef  __cplusplus
}
#endif
#endif                          /* __SGE_CONFL_H */