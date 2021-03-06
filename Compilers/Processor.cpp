#include "Processor.hpp"

// Using
//===========================================
using std::cout;
using std::cin;
using std::map;
using std::string;
using std::list;
using std::pair;
//===========================================


// Structures
//===========================================
struct argument {
//! 0 - constant, 1 - register
	char type = 0;
//! 1 if []
	char isPointer = 0;
//! If this is a constant then its value. If it is a register then its number (ax - 2, bx - 3, cx - 4)
	int value = 0;
//! 1 - if this argument is presented by the marker
	char isMarker = 0;
//! name of this marker
	tString marker = { };
//! length from end of bytecoded function to start of the markers address
	int marker_delta = 0;
//! relative address - from the end of the function to marker, absolute address - markers address
	char isRelative = 0;
};

// small struct to store data about places in code where after compiling may be written addresses of markers.
struct marker_empty_field {
//! name of this marker
	tString marker_name;
//! position where the address will be written
	int file_pos;
//! end position of the function from this marker is used
	int function_end_pos;
//! if 1 then there will be length from the end function position to marker address.
	char isRelative;
};
struct tProcessor {
	// Fields
	//-----------------------
	// RAM
	char *ram;
	char *carriage;
	unsigned code_size;

	// Stack
	tStack<PROCESSOR_TYPE, STACK_SIZE> mem_stack;

	// Registers
	PROCESSOR_TYPE *regs;

	// Flags
	bool JE_FLAG = 0;
	bool JNE_FLAG = 0;

	// Invokation
	bool invoking = false;

	// Window
	bool windowCreated;
	//-----------------------


	// Constructor and destructor
	//-----------------------
	tProcessor(tFile *exeFile_);
	~tProcessor();
	//-----------------------

	// Functions
	//-----------------------
	void putToRam(unsigned place, PROCESSOR_TYPE val);// Puts value to given place in the memory.
	PROCESSOR_TYPE getFromRam(unsigned place);// Gives element located in memory.
	void tSetCarriage(unsigned byte);// Sets execution carriage to given byte.
	char tGetc();// Reads symbol from execution carriage. And moves it further.
	unsigned tGetCurrentByte();// Gives current byte where carriage stands in the RAM.
	void tStop();//! Sets carriage to the end of the code. So if it is invoking right now then invokation will stops.
	void tInvoke();// Invokes program.
	//-----------------------

};
//===========================================


// Constants
//===========================================
// Translator
const int TOTAL_RPC = 4;
const int REWRITE_POINTER_COMMANDS[TOTAL_RPC] = { 49, 50, 51, 52 };

// Symbol that will be ignored
const unsigned SRC_IGNORE_SYMBS_QUANT = 3;
const char SRC_IGNORE_SYMBS[SRC_IGNORE_SYMBS_QUANT] = { ' ', ',', '\t' };

// Prototype of function
typedef void (*tProcFunction)(PROCESSOR_TYPE**, unsigned, tProcessor*);

// Imports
const char imprt[MAX_IMPORT_FILES][MAX_IMPORT_FUNCTIONS + 2][MAX_IMPORT_NAME_LEN] = {
		{ "kernel32.dll", "ExitProcess", "\0" },
		{ "kernel32.dll", "WriteConsoleA", "\0" },
		{ "kernel32.dll", "ReadConsoleA", "\0" },
		{ "kernel32.dll", "GetStdHandle", "\0" },
};
//===========================================
// Gives address for given symbolic value of the register.
char tGetRegisterIndex(tString args) {
	CHECK (args.tSize() < 2, "Arguments are NULL!");

	char s1 = args.tGetc(0);
	char s2 = args.tGetc(1);

	// If bp is register
	if (s1 == 'b' && s2 == 'p') {
		return 1;
	}

	if (s2 != 'x') {
		return -1;
	}

	char index = (s1 - 'a' + 1);

	if (index < 1 || (unsigned) index > REGS_SIZE) {
		return -1;
	}

	return index + 1;
}

bool may_rewrite(int id) {
	bool rewrite = 0;
	for (int i = 0; i < TOTAL_RPC; i++) {
		if (id == REWRITE_POINTER_COMMANDS[i]) {
			rewrite = 1;
		}
	}
	return rewrite;
}

tString tGetRegisterByIndex(char ind) {
	tString reg = "BAD_REGISTER";

	if (ind == 1) {
		return "bp";
	}
	if (ind == 2) {
		return "ax";
	}
	if (ind == 3) {
		return "bx";
	}
	if (ind == 4) {
		return "cx";
	}

	return reg;
}

tProcessor::tProcessor(tFile *exeFile_) : ram(NULL),
								  carriage(NULL),
								  code_size(0),
								  mem_stack( { }),
								  regs(NULL),
								  JE_FLAG(0),
								  JNE_FLAG(0),
								  invoking(0),
								  windowCreated(0) {
		regs = (PROCESSOR_TYPE*) calloc(REGS_SIZE, sizeof(PROCESSOR_TYPE));
		ram = (char*) calloc(RAM_SIZE, sizeof(char));

		exeFile_->tStartMapping();
		tCopyBuffers(exeFile_->tGetBuffer(), ram, code_size = exeFile_->tGetSize());
		exeFile_->tStopMapping();

		carriage = ram;
	}

	// Puts value to given place in the memory.
	void tProcessor::putToRam(unsigned place, PROCESSOR_TYPE val) {
		tWriteBytes<PROCESSOR_TYPE>(val, ram + code_size + sizeof(PROCESSOR_TYPE) * place);
	}

	// Gives element located in memory.
	PROCESSOR_TYPE tProcessor::getFromRam(unsigned place) {
		PROCESSOR_TYPE v = tConvertBytes<PROCESSOR_TYPE>(ram + code_size + sizeof(PROCESSOR_TYPE) * place);
		return v;
	}

	// Sets execution carriage to given byte.
	void tProcessor::tSetCarriage(unsigned byte) {
		CHECK (!invoking, "Program is not invoked!");

		carriage = ram + byte;
	}

	// Reads symbol from execution carriage. And moves it further.
	char tProcessor::tGetc() {
		if (invoking) {
			char res = *carriage;
			carriage++;
			return res;
		} else {
			THROW_T_EXCEPTION("Program is not invoked!");
			return -1;
		}
	}

	// Gives current byte where carriage stands in the RAM.
	unsigned tProcessor::tGetCurrentByte() {
		return carriage - ram;
	}

	tProcessor::~tProcessor() {
		delete regs;
		delete ram;
	}

	// Sets carriage to the end of the code.
	// So if it is invoking right now then invokation will stops.
	void tProcessor::tStop() {
		if (invoking) {
			carriage = ram + code_size;
		} else {
			THROW_T_EXCEPTION("Program is not invoked!");
		}
	}

	// Invokes program.
	void tProcessor::tInvoke() {
		cout << std::dec;

		// Update state
		invoking = true;

		// Pointers for arguments
		PROCESSOR_TYPE *arg_buf[MAX_ARGS];
		PROCESSOR_TYPE constant_pointers[MAX_ARGS];

		// Setting BP register to be right after code in RAM and ensure there will be space for 2 parameters.
		regs[(unsigned) tGetRegisterIndex(tString("bp"))] = 0;

		// Starting main loop of the invocation
		// Invocation will be stopped ONLY WHEN CARRIAGE IS STANDING RIGHT IN THE END OF THE CODE.
		// So invocation can continue even if carriage is not executing code.
		while (carriage != ram + code_size) {
			// Reading id of function.
			char id = tGetc();
			// Generation of normal c++ function.
			tProcFunction func = NULL;

#define T_PROC_FUNC(NAME, ID, CODE, CODE_EXE) if (id == ID) {func = [](PROCESSOR_TYPE**args, unsigned totalArgs, tProcessor*proc) CODE;}
#include "StandartDefs.h"
#include "cmd.tlang"
#include "StandartUndefs.h"
#undef T_PROC_FUNC

			if (func == NULL) {
				continue;
			}

			//nm.out();

			// Reading byte where quantity of arguments is stored.
			unsigned total_args = (unsigned) (tGetc());

			// Parsing every argument and preparing argument array.
			for (unsigned i = 0; i < total_args; i++) {
				// Reading flag that says if this parameter lives in RAM.
				char isRam = tGetc();
				// Reading argument parameter.
				// If it is zero then next 4 bytes will be used.
				// If it is not zero then we will use register
				char arg_param = tGetc();

				PROCESSOR_TYPE *arg_p = NULL;

				// Arguments are 4 bytes after
				if (arg_param == (char) 0) {
					constant_pointers[i] = tConvertBytes<PROCESSOR_TYPE>(carriage);
					carriage += 4;
					arg_p = &constant_pointers[i];
				}
				// Argument is register
				else {
					arg_p = &(regs[(unsigned) arg_param]);
				}

				// If ram flag is not zero then value will be used as address for RAM
				if (isRam == 0) {
					*(arg_buf + i) = arg_p;
				} else {
					*(arg_buf + i) = (PROCESSOR_TYPE*) (ram + (unsigned) (*arg_p));
				}
			}

			// Launch function
			func(arg_buf, total_args, this);
		}

		invoking = false;
	}


// Generates map where all targets in this source file are stored.
map<tString, unsigned>* tGenJmpsLib(tFile *src) {
	NOT_NULL(src);

	map<tString, unsigned> *result = new map<tString, unsigned>;

	while (src->tHasMoreSymbs()) {
		tString line = src->tReadLine();

		for (unsigned len = 0; len < line.tSize(); len++) {
			if ((line)[len] == ':') {
				unsigned total_divs = 0;
				tString *divs = line.tSubstring(0, len).tParse(SRC_IGNORE_SYMBS,
															   SRC_IGNORE_SYMBS_QUANT,
															   total_divs);

				CHECK (divs == NULL || total_divs < 1 || divs[total_divs - 1].tSize() == 0, "Compilation error! Something is wrong with jmps!");

				(*result)[divs[total_divs - 1].tSubstring(0, divs[total_divs - 1].tSize() - 2)] =
						src->tGetCurrentByte();

				delete divs;

				break;
			}
		}
	}

	return result;
}

//! Generates nice looking name for marker by index.
tString genMarker(int index) {
	tString res = "MARKER";
	res += index;
	return res;
}

//! Generates map: marker name -> src code location
map<int, tString>* getMarkers(tFile *exe) {
	NOT_NULL(exe);

	map<int, tString> *markers = new map<int, tString>;
	exe->tStartMapping();
	while (exe->tHasMoreSymbs()) {
		(*markers)[exe->tGetCurrentByte()] = genMarker(exe->tGetCurrentByte());

		// Reading id of function.
		if (0 == (unsigned) exe->tGetc()) {
			break;
		}

		// Reading byte where quantity of arguments is stored.
		unsigned total_args = (unsigned) (exe->tGetc());

		// Parsing every argument and preparing argument array.
		for (unsigned i = 0; i < total_args; i++) {
			// Reading flag that says if this parameter lives in RAM.
			exe->tGetc();
			// Reading argument parameter.
			// If it is zero then next 4 bytes will be used.
			// If it is not zero then we will use register
			char arg_param = exe->tGetc();

			// Arguments are 4 bytes after
			if (arg_param == (char) 0) {
				exe->tMovePointer(4);
			}
		}
	}
	exe->tStopMapping();
	return markers;
}

void comDisasm_TEXE_to_TASM(tFile *exe, tFile *&src) {
	NOT_NULL(exe);
	NOT_NULL(src);

	map<int, tString> *markers = getMarkers(exe);

	exe->tStartMapping();
	src->tStartMapping(1000);

	tString curr = "";

	while (exe->tHasMoreSymbs()) {
		// Writing marker
		src->tWriteLine((*markers)[exe->tGetCurrentByte()] + ":");

		// Reading id of function.
		char id = exe->tGetc();
		// Generation of normal c++ function.
		tProcFunction func = NULL;

		tString func_name = "";

#define T_PROC_FUNC(NAME, ID, CODE, CODE_EXE) if (id == ID) {func_name = #NAME;func = [](PROCESSOR_TYPE**args, unsigned totalArgs, tProcessor*proc) CODE;}
#include "StandartDefs.h"
#include "cmd.tlang"
#include "StandartUndefs.h"
#undef T_PROC_FUNC

		if (func == NULL) {
			cout << "NO SUCH DISASM FUNCTION " << std::dec << (int) id << "\n";
			continue;
		}

		curr += func_name;

		curr += " ";

		// Reading byte where quantity of arguments is stored.
		unsigned total_args = (unsigned) (exe->tGetc());

		// Parsing every argument and preparing argument array.
		for (unsigned i = 0; i < total_args; i++) {
			// Reading flag that says if this parameter lives in RAM.
			char isRam = exe->tGetc();
			// Reading argument parameter.
			// If it is zero then next 4 bytes will be used.
			// If it is not zero then we will use register
			char arg_param = exe->tGetc();

			if (isRam != 0) {
				curr += "[";
			}

			// Arguments are 4 bytes after
			if (arg_param == (char) 0) {
				int val = tConvertBytes<PROCESSOR_TYPE>(exe->tGetCurrentPointer());
				exe->tMovePointer(4);
				if ((isRam != 0 || may_rewrite(id)) && markers->count(val)) {
					curr += (*markers)[val];
				} else {
					curr += val;
				}
			}
			// Argument is register
			else {
				curr += tGetRegisterByIndex(arg_param);
			}

			if (isRam != 0) {
				curr += "]";
			}

			if (i + 1 < total_args) {
				curr += ", ";
			}
		}

		src->tWriteLine(curr);
		curr = "";
	}

	tShrink(src);
	exe->tStopMapping();
}

void comCompile_TASM_to_TEXE(tFile *source, tFile *&exe) {
	NOT_NULL(source);
	NOT_NULL(exe);

	// File init
	source->tStartMapping();
	exe->tStartMapping(2 * tGetFileSize(source->tGetName()));

	// Jumps library
	list<pair<tString, unsigned>> jmp_later;
	map<tString, unsigned> *jmp_points = tGenJmpsLib(source);

	// Ensuring end. target is in this file.
	(*jmp_points)[tString("end.")] = 1;

	source->tFlip();

	while (source->tHasMoreSymbs()) {
		tString line = source->tReadLine();
		unsigned total_divs = 0;
		tString *divs = line.tParse(SRC_IGNORE_SYMBS, SRC_IGNORE_SYMBS_QUANT, total_divs);

		// Checking if it is jump
		for (unsigned i = 0; i < total_divs; i++) {
			// double dot index :
			unsigned dd_index = divs[i].tSize() - 1;
			if (divs[i].tGetc(dd_index) == ':') {
				tString cutted = divs[i].tSubstring(0, dd_index - 1);
				(*jmp_points)[cutted] = exe->tGetCurrentByte();

				delete divs;
				total_divs = 0;
				break;
			}
		}

		if (total_divs < 1) {
			delete divs;
			continue;
		}

		// Checking for comments
		if (divs[0].tSize() > 1 && divs[0].tGetc(0) == '/' && divs[0].tGetc(1) == '/') {
			delete divs;
			continue;
		}

		int id = -1;

#define T_PROC_FUNC(NAME, ID, CODE, CODE_PE) if (divs[0] == tString(#NAME)) {id = ID;}
#include "StandartDefs.h"
#include "cmd.tlang"
#include "StandartUndefs.h"
#undef T_PROC_FUNC

		if (id == -1 || total_divs - 1 > MAX_ARGS) {
			delete divs;
			THROW_T_EXCEPTION("Compilation error!");
		}

		// Writing id of this command
		exe->tWritec(id);

		// Writing quantity of parameters
		exe->tWritec((char) (total_divs - 1));
		for (unsigned i = 1; i < total_divs; i++) {

			char isRam = 0;
			if (divs[i].tGetc(0) == '[' && divs[i].tGetc(divs[i].tSize() - 1) == ']') {
				isRam = (char) (127);
				divs[i] = divs[i].tSubstring(1, divs[i].tSize() - 1);
			}

			char address = (divs[i].tSize() > 1 ? tGetRegisterIndex(divs[i]) : -1);

			exe->tWritec(isRam);

			if (address != -1) {
				// Writing type of argument. In this case it will be register.
				exe->tWritec(address);
			} else {
				// Writing type of argument. In this case there will be 4 bytes after.
				exe->tWritec((char) (0));

				if ((*jmp_points).count(divs[i]) > 0) {
					pair<tString, unsigned> p = {
												  divs[i],
												  exe->tGetCurrentByte()
												};
					jmp_later.push_back(p);
					exe->tMovePointer(4);
					continue;
				}

				PROCESSOR_TYPE value = 0;
				if (typeid(PROCESSOR_TYPE) == typeid(int)) {
					value = divs[i].tToInt();
				}
				if (typeid(PROCESSOR_TYPE) == typeid(float)) {
					value = divs[i].tToFloat();
				}
				tWriteBytes<PROCESSOR_TYPE>(value, exe->tGetCurrentPointer());
				exe->tMovePointer(4);

			}

		}

		delete divs;
	}

	(*jmp_points)[tString("end.")] = exe->tGetCurrentByte();
	exe->tWritec((char) 0);

	while (!jmp_later.empty()) {
		pair<tString, unsigned> p = jmp_later.front();
		jmp_later.pop_front();
		int address = (*jmp_points)[p.first];
		unsigned byte = p.second;

		tWriteBytes<PROCESSOR_TYPE>(address, exe->tGetBuffer() + byte);
	}

	exe->tMovePointer(-1);
	tShrink(exe);
	source->tStopMapping();
}

void comCompile_TASM_to_EXE(tFile *source, tFile *exe_) {
	NOT_NULL(source);
	NOT_NULL(exe_);

	PE_Maker *maker= new PE_Maker(exe_, 0x400, 0x200, 4, imprt);

// File init
	source->tStartMapping();
// Jumps library
	list<marker_empty_field> jmp_later = {};
	map<tString, unsigned> *jmp_points = tGenJmpsLib(source);

// Ensuring end. target is in this file.
	(*jmp_points)[tString("end.")] = 1;

	source->tFlip();

	while (source->tHasMoreSymbs()) {
		tString line = source->tReadLine();

		unsigned total_divs = 0;
		tString *divs = line.tParse(SRC_IGNORE_SYMBS, SRC_IGNORE_SYMBS_QUANT, total_divs);

		// Checking if it is jump
		for (unsigned i = 0; i < total_divs; i++) {
			// double dot index :
			unsigned dd_index = divs[i].tSize() - 1;

			// Checking if it is an only one marker without double dot
			if (divs[i].tContains("end.") && total_divs == 1) {
				(*jmp_points)["end."] = maker->getCodeCarriageLocation();
				maker->wcif("ExitProcess");
				continue;
			}
			// Checking if it is a marker
			if (divs[i].tGetc(dd_index) == ':') {
				// cut marker name
				tString cutted = divs[i].tSubstring(0, dd_index - 1);
				// set file address for this marker
				(*jmp_points)[cutted] = maker->getCodeCarriageLocation();
				// drop of this div and ignore the end of this line!
				total_divs = 0;
				break;
			}
		}

		// if the ONLY div was a marker
		if (total_divs < 1) {
			delete divs;
			continue;
		}

		// If there is a comment
		if (divs[0].tSize() > 1 && divs[0].tGetc(0) == '/' && divs[0].tGetc(1) == '/') {
			delete divs;
			continue;
		}

		// If there is too many arguments
		if (total_divs - 1 > MAX_ARGS) {
			delete divs;
			THROW_T_EXCEPTION("Compilation error, too many arguments!");
		}

		// total arguments that is passed to function.
		int total_args = (total_divs - 1);
		argument args[MAX_ARGS] = {};

		// ANALYZING ARGUMENTS
		for (unsigned i = 1; i < total_divs; i++) {
			argument curr = { };

			// checking if it is pointer
			if (divs[i].tGetc(0) == '[' && divs[i].tGetc(divs[i].tSize() - 1) == ']') {
				curr.isPointer = 1;
				// cut div
				divs[i] = divs[i].tSubstring(1, divs[i].tSize() - 2);
			}

			// try to determine register
			char address = (divs[i].tSize() > 1 ? tGetRegisterIndex(divs[i]) : -1);

			if (address != -1) {
				// REGISTER
				curr.type = 1;
				curr.value = address;
			} else {
				// CONSTANT or MARKER
				curr.type = 0;
				// If it is marker
				if ((*jmp_points).count(divs[i]) > 0) {
					curr.marker = divs[i];
					curr.isMarker = 1;
				} else {
					curr.value = divs[i].tToInt();
				}
			}

			// add this argument
			args[i - 1] = curr;
		}

		// just for easier access
		argument &arg1 = args[0];
		argument &arg2 = args[1];

		// CAN USE: maker, args, arg1, arg2, total_args
		// WRITING PE INSTRUCTION
#define WB(A) maker->wcb(A);
#define WDW(A) maker->wcdw(A);

#define T_PROC_FUNC(NAME, ID, CODE, CODE_PE) if (divs[0] == tString(#NAME)) {CODE_PE;}
#include "StandartDefs.h"
#include "cmd.tlang"
#include "StandartUndefs.h"
#undef T_PROC_FUNC

#undef WB
#undef WDW

		// Analyze markers
		for (int i = 0; i < total_args; i++) {
			// if the argument is presented with a marker then register request to write its address later
			if (args[i].isMarker == 1) {
				marker_empty_field p = {
										 args[i].marker,// name of the marker
										 maker->getCodeCarriageLocation() - args[i].marker_delta, // actual place where address may be insert
										 maker->getCodeCarriageLocation(), // place where function is ending
										 args[i].isRelative // if need relative address
									   };
				jmp_later.push_back(p);
			}
		}

		delete divs;
	}

	// Analyze markers requests
	while (!jmp_later.empty()) {
		marker_empty_field p = jmp_later.front();

		jmp_later.pop_front();
		int marker_file_address = (*jmp_points)[p.marker_name]; // address of the point
		int file_pos = p.file_pos; // address of the caller
		int end_func_pos = p.function_end_pos;
		// if relative
		if (p.isRelative) {
			tWriteBytes<PROCESSOR_TYPE>(marker_file_address - end_func_pos,
									    maker->getPointerInFile(file_pos));
		} else {
			tWriteBytes<PROCESSOR_TYPE>(maker->getCodeVA() + marker_file_address - maker->getCodeLocation(),
										maker->getPointerInFile(file_pos));
		}
	}

	source->tStopMapping();
	delete maker;
}

void comTranslateFast_TEXE_to_EXE(tFile *texe, tFile *exe) {
	NOT_NULL(texe);
	NOT_NULL(exe);

	// File init
	texe->tStartMapping();
	PE_Maker *maker = new PE_Maker(exe, 0x400, 0x200, 4, imprt);

	// Commands starts
	map<int, int> texe_to_exe = {};

	// FIRST BYPASS!!!
	while (texe->tHasMoreSymbs()) {
		texe_to_exe[texe->tGetCurrentByte()] = maker->getCodeCarriageLocation();

		int id = (int) texe->tGetc();
		int total_args = (int) texe->tGetc();

		argument args[MAX_ARGS] = {};

		// ANALYZING ARGUMENTS
		for (int i = 0; i < total_args; i++) {
			argument curr = { };

			char isPointer = texe->tGetc();
			char argParam = texe->tGetc();

			// checking if it is pointer
			if (isPointer != 0) {
				curr.isPointer = 1;
			}

			if (argParam != 0) {
				// REGISTER
				curr.type = 1;
				curr.value = argParam;
			} else {
				// CONSTANT or MARKER
				curr.type = 0;
				curr.value = tConvertBytes<PROCESSOR_TYPE>(texe->tGetCurrentPointer());
				texe->tMovePointer(4);
			}

			// add this argument
			args[i] = curr;
		}

		// just for easier access
		argument &arg1 = args[0];
		argument &arg2 = args[1];

		// CAN USE: maker, args, arg1, arg2, total_args
		// WRITING PE INSTRUCTION
#define WB(A) maker->wcb(A);
#define WDW(A) maker->wcdw(A);

#define T_PROC_FUNC(NAME, ID, CODE, CODE_PE) if (ID == id) {CODE_PE;}
#include "StandartDefs.h"
#include "cmd.tlang"
#include "StandartUndefs.h"
#undef T_PROC_FUNC

#undef WB
#undef WDW
	}

	maker->flipCodeCarriage();
	texe->tFlip();

	// SECOND BYPASS!!!
	while (texe->tHasMoreSymbs()) {
		int id = (int) texe->tGetc();
		int total_args = (int) texe->tGetc();

		argument args[MAX_ARGS] = {};

		// ANALYZING ARGUMENTS
		for (int i = 0; i < total_args; i++) {
			argument curr = { };

			char isPointer = texe->tGetc();
			char argParam = texe->tGetc();

			// checking if it is pointer
			if (isPointer != 0) {
				curr.isPointer = 1;
			}
			if (argParam != 0) {
				// REGISTER
				curr.type = 1;
				curr.value = argParam;
			} else {
				// CONSTANT or MARKER
				curr.type = 0;
				curr.value = tConvertBytes<PROCESSOR_TYPE>(texe->tGetCurrentPointer());
				texe->tMovePointer(4);
			}

			// add this argument
			args[i] = curr;
		}

		// just for easier access
		argument &arg1 = args[0];
		argument &arg2 = args[1];

		// CAN USE: maker, args, arg1, arg2, total_args
		// WRITING PE INSTRUCTION
#define WB(A) maker->wcb(A);
#define WDW(A) maker->wcdw(A);

#define T_PROC_FUNC(NAME, ID, CODE, CODE_PE) if (ID == id) {CODE_PE;}
#include "StandartDefs.h"
#include "cmd.tlang"
#include "StandartUndefs.h"
#undef T_PROC_FUNC

#undef WB
#undef WDW

		bool rewrite = may_rewrite(id);
		if (rewrite == 1) {
			int texe_to = arg1.value;
			int texe_from = texe->tGetCurrentByte();

			int exe_to = texe_to_exe[texe_to];
			int exe_from = texe_to_exe[texe_from];

			int rel = exe_to - exe_from;

			tWriteBytes<int>(rel, exe->tGetBuffer() + exe_from - 4);
		}
	}

	texe->tStopMapping();
	delete maker;
}

void comTranslateSlow_TEXE_to_EXE(tFile *texe, tFile *exe) {
	NOT_NULL(texe);
	NOT_NULL(exe);

	tFile *text_disasm = new tFile("disasm");
	comDisasm_TEXE_to_TASM(texe, text_disasm);
	comCompile_TASM_to_EXE(text_disasm, exe);
}

void comRun_TEXE(tFile * texe) {
	NOT_NULL(texe);

	tProcessor * proc = new tProcessor(texe);

	proc->tInvoke();
}

void comRun_EXE(tFile * exe) {
	system(exe->tGetName());
}

