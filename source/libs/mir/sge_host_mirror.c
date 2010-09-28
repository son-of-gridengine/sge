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

#include "sge.h"
#include "sgermon.h"
#include "sge_log.h"

#include "sge_host.h"

#include "sge_mirror.h"
#include "sge_host_mirror.h"

/****** Eventmirror/host/host_update_master_list() *****************************
*  NAME
*     host_update_master_list() -- update the master hostlists
*
*  SYNOPSIS
*     bool 
*     host_update_master_list(sge_object_type type, sge_event_action action,
*                             lListElem *event, void *clientdata)
*
*  FUNCTION
*     Update the global master lists of hosts
*     based on an event.
*     The function is called from the event mirroring interface.
*     Updates admin, submit or execution host list depending
*     on the event received.
*
*  INPUTS
*     sge_object_type type     - event type
*     sge_event_action action - action to perform
*     lListElem *event        - the raw event
*     void *clientdata        - client data
*
*  RESULT
*     bool - true, if update is successfull, else false
*
*  NOTES
*     The function should only be called from the event mirror interface.
*
*  SEE ALSO
*     Eventmirror/--Eventmirror
*     Eventmirror/sge_mirror_update_master_list()
*     Eventmirror/sge_mirror_update_master_list_host_key()
*******************************************************************************/
bool 
host_update_master_list(sge_object_type type, sge_event_action action,
                        lListElem *event, void *clientdata)
{
   lList **list;
   const lDescr *list_descr;
   int     key_nm;

   const char *key;


   DENTER(TOP_LAYER, "host_update_master_list");
   
   list_descr = lGetListDescr(lGetList(event, ET_new_version));
   
   switch (type) {
      case SGE_TYPE_ADMINHOST:
         list = &Master_Adminhost_List;
/*         list_descr = AH_Type;*/
         key_nm = AH_name;
         break;
      case SGE_TYPE_EXECHOST:
         list = &Master_Exechost_List;
/*         list_descr = EH_Type; */
         key_nm = EH_name;
         break;
      case SGE_TYPE_SUBMITHOST:
         list = &Master_Submithost_List;
/*         list_descr = SH_Type; */
         key_nm = SH_name;
         break;
      default:
         return false;
   }
 
   key = lGetString(event, ET_strkey);

   if (sge_mirror_update_master_list_host_key(list, list_descr, key_nm, key, 
                                              action, event) != SGE_EM_OK) {
      DEXIT;
      return false;
   }

   DEXIT;
   return true;
}

