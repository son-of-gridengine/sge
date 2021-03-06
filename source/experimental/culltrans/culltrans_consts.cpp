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
// culltrans_consts.cpp
// generates the constants for element codes (QU_xxx, JB_xxx,...)
// in file elem_codes.idl

#include <map>
#include <set>
#include <string>
#include <iostream>
#include <fstream>
#include "culltrans_repository.h"
#include "culltrans.h"

extern "C" {
#include "sge_boundaries.h"
}

#ifdef HAVE_STD
using namespace std;
#endif

bool writeConsts() {
  cout << "Creating consts for element codes." << endl;
   
   // open output file
   ofstream out("elem_codes.idl");
   if(!out) {
      cerr << "Could not open output file 'elem_codes.idl'." << endl;
      return false;
   }

   // write Header
   const string indent = "   ";
   out << "// " << "elem_codes.idl" << endl;
   out << "// this file is automatically generated. DO NOT EDIT" << endl << endl;
   out << "#ifndef elem_codes_idl" << endl;
   out << "#define elem_codes_idl" << endl << endl;

   out << "module GE {" << endl;

   // write the constants
   map<int, string>::iterator it;
   int i;
   for(i=0; i<last_qidl_only; i++)
      if((it = constants.find(i)) != constants.end())
         out << indent << "const unsigned long " << it->second << " = " << i << ";" << endl;

   // write footer
   out << "};" << endl << endl;

   out << "#endif // elem_codes_idl" << endl;
   out.close();

   return true;
}
