/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2012 Intel Corporation. All rights reserved.
 
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
#include <stdio.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include "pin.H"

#define INS_START 0x0
#define INS_END 0x10000000
#define POST_WRITE_MEM() { \
	if(waddr != NULL) { \
		ADDRINT dst2; \
		PIN_SafeCopy(&dst2, waddr, sizeof(ADDRINT)); \
		fprintf(trace, "%08x\n", dst2); \
		waddr = NULL; \
	} \
}

FILE * trace;
ADDRINT * waddr = NULL;

VOID mem_write(VOID * ip, VOID * addr, UINT32 is_req)
{
	std::stringstream ss;
	ADDRINT dst;

	POST_WRITE_MEM();
	PIN_SafeCopy(&dst, addr, sizeof(ADDRINT));
	ss << "\tMem W " << addr << " ( = " << setw(8) << setfill('0') << hex << dst << " ) = ";
	fprintf(trace, "%s", ss.str().c_str());
	waddr = (ADDRINT*)addr;
	if(is_req) {
		waddr -= 1;
	}
}

VOID mem_read(VOID * ip, VOID * addr)
{
	std::stringstream ss;
	ADDRINT dst;

	POST_WRITE_MEM();
	PIN_SafeCopy(&dst, addr, sizeof(ADDRINT));
	ss << "\tMem R " << addr << " = " << setw(8) << setfill('0') << hex <<  dst << endl;
	fprintf(trace, "%s", ss.str().c_str());
}

VOID print_ins(std::string *msg)
{
	POST_WRITE_MEM();
	fprintf(trace, "%s\n", (*msg).c_str());
}

std::string trace_ins(INS ins)
{
	std::stringstream ss;

	ss << "0x" << setfill('0') << setw(8) << uppercase << hex << INS_Address(ins) << "\t"
		<< left << setfill(' ') << setw(40) << INS_Disassemble(ins);
/*
	if(INS_MaxNumRRegs(ins) > 0) {
		ss << "\tReg R =";
		for(UINT32 i = 0; i < INS_MaxNumRRegs(ins); i++) {
			ss << " " << REG_StringShort(INS_RegR(ins, i));
		}
		ss << endl;
	}
	if(INS_MaxNumWRegs(ins) > 0) {
		ss << "\tReg W =";
		for(UINT32 i = 0; i < INS_MaxNumWRegs(ins); i++) {
			ss << " " << REG_StringShort(INS_RegW(ins, i));
		}
	}
*/
	return ss.str();
}

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID *v)
{
    // Insert a call to printip before every instruction, and pass it the IP
	ADDRINT address = INS_Address(ins);
	if(address >= INS_START && address <= INS_END) {
	    INS_InsertCall(
		ins, IPOINT_BEFORE, (AFUNPTR)print_ins,
		IARG_PTR, new std::string(trace_ins(ins)),
		IARG_END);

	    // Iterate over each memory operand of the instruction.
	    UINT32 memOperands = INS_MemoryOperandCount(ins);
	    for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
			if (INS_MemoryOperandIsRead(ins, memOp)) {
				INS_InsertPredicatedCall(
				ins, IPOINT_BEFORE, (AFUNPTR)mem_read,
				IARG_INST_PTR,
				IARG_MEMORYOP_EA, memOp,
				IARG_END);
			}

			UINT32 is_req = INS_HasRealRep(ins);   // included REP, REPZ, REPNZ
			if(is_req) {
				// write_op is before read_op in rep prefix
				if (INS_MemoryOperandIsWritten(ins, memOp)) {
					INS_InsertPredicatedCall(
							ins, IPOINT_AFTER, (AFUNPTR)mem_write,
							IARG_INST_PTR,
							IARG_MEMORYOP_EA, memOp,
							IARG_UINT32, is_req,
							IARG_END);
				}
			} else {
				if (INS_MemoryOperandIsWritten(ins, memOp)) {
					INS_InsertPredicatedCall(
							ins, IPOINT_BEFORE, (AFUNPTR)mem_write,
							IARG_INST_PTR,
							IARG_MEMORYOP_EA, memOp,
							IARG_UINT32, is_req,
							IARG_END);
				}
			}
	    }
	}
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
    POST_WRITE_MEM();
    fprintf(trace, "#eof\n");
    fclose(trace);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    PIN_ERROR("This Pintool prints every instruction executed\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char * argv[])
{
    trace = fopen("itrace2.out", "w");

    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) {
	return Usage();
    }

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();
    
    return 0;
}
