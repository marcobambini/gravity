//
//  compiler_test.c
//  gravity
//
//  Created by Marco Bambini on 24/03/16.
//  Copyright © 2016 CreoLabs. All rights reserved.
//

#include "gravity_compiler.h"
#include "gravity_core.h"
#include "gravity_vm.h"

#define DEFAULT_OUTPUT "gravity.json"
typedef enum  {
	OP_COMPILE,			// just compile source code and exit
	OP_RUN,				// just run an already compiled file
	OP_COMPILE_RUN,		// compile source code and run it
	OP_REPL				// run a read eval print loop
} op_type;

static const char *input_file = NULL;
static const char *output_file = DEFAULT_OUTPUT;

static void report_error (error_type_t error_type, const char *message, error_desc_t error_desc, void *xdata) {
	#pragma unused(xdata)
	const char *type = "N/A";
	switch (error_type) {
		case GRAVITY_ERROR_NONE: type = "NONE"; break;
		case GRAVITY_ERROR_SYNTAX: type = "SYNTAX"; break;
		case GRAVITY_ERROR_SEMANTIC: type = "SEMANTIC"; break;
		case GRAVITY_ERROR_RUNTIME: type = "RUNTIME"; break;
		case GRAVITY_WARNING: type = "WARNING"; break;
		case GRAVITY_ERROR_IO: type = "I/O"; break;
	}
	
	if (error_type == GRAVITY_ERROR_RUNTIME) printf("RUNTIME ERROR: ");
	else printf("%s ERROR on %d (%d,%d): ", type, error_desc.fileid, error_desc.lineno, error_desc.colno);
	printf("%s\n", message);
}

static void print_version (void) {
	printf("Gravity version %s (%s)\n", GRAVITY_VERSION, GRAVITY_BUILD_DATE);
}

static void print_help (void) {
	printf("usage: gravity [options]\n");
	printf("no options means enter interactive mode (not yet supported)\n");
	printf("Available options are:\n");
	printf("--version          show version information and exit\n");
	printf("--help             show command line usage and exit\n");
	printf("-c input_file      compile input_file (default to gravity.json)\n");
	printf("-o output_file     specify output file name\n");
	printf("-x input_file      execute input_file (JSON format expected)\n");
	printf("file_name          compile file_name and executes it\n");
}

static void gravity_repl (void) {
	printf("REPL not yet implemented.\n");
}

static op_type parse_args (int argc, const char* argv[]) {
	if (argc == 1) return OP_REPL;
	
	if (argc == 2 && strcmp(argv[1], "--version") == 0) {
		print_version();
		exit(0);
	}
	
	if (argc == 2 && strcmp(argv[1], "--help") == 0) {
		print_help();
		exit(0);
	}
	
	op_type type = OP_RUN;
	for (int i=1; i<argc; ++i) {
		if ((strcmp(argv[i], "-c") == 0) && (i+1 < argc)) {
			input_file = argv[++i];
			type = OP_COMPILE;
		}
		else if ((strcmp(argv[i], "-o") == 0) && (i+1 < argc)) {
			output_file = argv[++i];
		}
		else if ((strcmp(argv[i], "-x") == 0) && (i+1 < argc)) {
			input_file = argv[++i];
			type = OP_RUN;
		}
	}
	
	if (input_file == NULL) {
		input_file = argv[1];
		return OP_COMPILE_RUN;
	}
	return type;
}

// MARK: -

int main (int argc, const char* argv[]) {
	// parse arguments and return operation type
	op_type type = parse_args(argc, argv);
	
	// special repl case
	if (type == OP_REPL) {
		gravity_repl();
		exit(0);
	}
	
	// initialize memory debugger (if activated)
	mem_init();
	
	// closure to execute/serialize
	gravity_closure_t *closure = NULL;
	
	// optional compiler
	gravity_compiler_t *compiler = NULL;
	
	// setup compiler/VM delegate
	gravity_delegate_t delegate = {.error_callback = report_error};
	
	// create VM
	gravity_vm *vm = gravity_vm_new(&delegate);
	
	// check if input file is source code that needs to be compiled
	if ((type == OP_COMPILE) || (type == OP_COMPILE_RUN)) {
		size_t size = 0;
		
		// load source code
		const char *source_code = file_read(input_file, &size);
		if (!source_code) {
			printf("Error loading file %s", input_file);
			goto cleanup;
		}
		
		// create compiler
		compiler = gravity_compiler_create(&delegate);
		
		// compile source code into a closure
		closure = gravity_compiler_run(compiler, source_code, size, 0, false);
		
		// check if closure needs to be serialized
		if (type == OP_COMPILE) {
			bool result = gravity_compiler_serialize_infile(compiler, closure, output_file);
			if (!result) printf("Error serializing file %s\n", output_file);
			goto cleanup;
		}
		
		// op is OP_COMPILE_RUN so transfer memory from compiler to VM
		gravity_compiler_transfer(compiler, vm);
		
	} else if (type == OP_RUN) {
		// unserialize file
		closure = gravity_vm_loadfile(vm, input_file);
		if (!closure) {
			printf("Error while loading compile file %s\n", input_file);
			goto cleanup;
		}
	}
	
	// sanity check
	assert(closure);
	
	if (gravity_vm_run(vm, closure)) {
		gravity_value_t result = gravity_vm_result(vm);
		double t = gravity_vm_time(vm);
		
		char buffer[512];
		gravity_value_dump(result, buffer, sizeof(buffer));
		printf("RESULT: %s (in %.4f ms)\n\n", buffer, t);
	}
	
cleanup:
	if (compiler) gravity_compiler_free(compiler);
	if (vm) gravity_vm_free(vm);
	gravity_core_free();
	
	#if GRAVITY_MEMORY_DEBUG
	size_t current_memory = mem_leaks();
	if (current_memory != 0) {
		printf("--> VM leaks: %zu bytes\n", current_memory);
		mem_stat();
	} else {
		printf("\tNo VM leaks found!\n");
	}
	#endif
	
	return 0;
}
