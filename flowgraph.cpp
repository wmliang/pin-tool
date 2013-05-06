/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2013 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
//
// This tool counts the number of times a routine is executed and 
// the number of instructions executed in a routine
//

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string.h>
#include <vector>
#include <algorithm>
#include "pin.H"
using namespace std;

/* ===================================================================== */
/* Global                                                                */
/* ===================================================================== */
#define OUTPUT_FILE "flowgraph.out"
#define PROCESS_NAME "vscan"
ofstream outFile;
UINT64 RtnOffset = 0;

// Structure for routine
typedef struct RtnCount
{
    string _name;
    string _image;
} RTN_COUNT;

vector<RTN_COUNT*> RtnList;
vector<string> ExceptList;

int IsFound(string str)
{
    vector<string>::iterator it;
    it = find(ExceptList.begin(), ExceptList.end(), str);
    if(it == ExceptList.end()) {
		return 0;
    } else {
		return 1;
    }
}

// This function is called before every routine is executed
VOID call_in(RTN_COUNT * rc)
{
    if(IsFound(rc->_name)) {
		return;
    }

    if ( !rc->_image.compare(PROCESS_NAME) ) {
		for (UINT64 i=0;i<RtnOffset;i++) {
	    	outFile << " ";
		}
		outFile << "->" << rc->_name << endl;
		RtnOffset++;
    }
}

// This function is called after every routine is executed
VOID return_out(RTN_COUNT * rc)
{
    if(IsFound(rc->_name)) {
		return;
    }

    if ( !rc->_image.compare(PROCESS_NAME) ) {
		for (UINT64 i=0;i<RtnOffset-1;i++) {
			outFile << " ";
		}
		outFile << "<-" << rc->_name << endl;
		if( RtnOffset == 0)
			return;
		RtnOffset--;
    }

}
    
const char * StripPath(const char * path)
{
    const char * file = strrchr(path,'/');
    if (file) {
        return file+1;
    } else {
        return path;
    }
}

// Pin calls this function every time a new rtn is executed
VOID Routine(RTN rtn, VOID *v)
{
    
    // Allocate a counter for this routine
    RTN_COUNT * rc = new RTN_COUNT;

    // The RTN goes away when the image is unloaded, so save it now
    // because we need it in the fini
    rc->_name = RTN_Name(rtn);
    rc->_image = StripPath(IMG_Name(SEC_Img(RTN_Sec(rtn))).c_str());
    RtnList.push_back(rc);
            
    RTN_Open(rtn);
            
    // Insert a call at the entry/exit point of a routine
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)call_in,
					IARG_PTR, rc,
					IARG_END);
    RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)return_out,
					IARG_PTR, rc,
					IARG_END);
    
    RTN_Close(rtn);
}

// This function is called when the application starts
VOID Init()
{
    outFile.open(OUTPUT_FILE);
	outFile << hex;
	outFile.setf(ios::showbase);
    outFile << "----=Start of Flow Graph=----" << endl;

	// Routine in ExceptList will not appear
    ExceptList.push_back(".plt");
    ExceptList.push_back("__do_global_ctors_aux");
    ExceptList.push_back("__do_global_dtors_aux");
    ExceptList.push_back("lstat");
    ExceptList.push_back("fstat");
    ExceptList.push_back("__fstat");
    ExceptList.push_back("__lstat");
    ExceptList.push_back("__stat");
    ExceptList.push_back("_start");
    ExceptList.push_back("_init");
    ExceptList.push_back("_fini");
    ExceptList.push_back("__libc_csu_init");
    ExceptList.push_back("call_gmon_start");
    ExceptList.push_back("frame_dummy");
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
    outFile << "----=End of Flow Graph=----" << endl;
    outFile.close();

	// Free memory
	RTN_COUNT *tmp;
	while (!RtnList.empty()) {
		tmp = RtnList.back();
		delete tmp;
		RtnList.pop_back();
	}
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr << "This Pintool figure out the flow graph for each routine druing the execution" << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char * argv[])
{
    // Initialize symbol table code, needed for rtn instrumentation
    PIN_InitSymbols();

    // Initialization
    Init();

    // Initialize pin
    if (PIN_Init(argc, argv)) {
		return Usage();
    }

    // Register Routine to be called to instrument rtn
    RTN_AddInstrumentFunction(Routine, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
