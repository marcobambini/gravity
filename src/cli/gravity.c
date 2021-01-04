//
//  compiler_test.c
//  gravity
//
//  Created by Marco Bambini on 24/03/16.
//  Copyright © 2016 CreoLabs. All rights reserved.
//

#include "gravity_compiler.h"
#include "gravity_optionals.h"
#include "gravity_utils.h"
#include "gravity_core.h"
#include "gravity_vm.h"
#include "gravity_opt_env.h"

#define DEFAULT_OUTPUT "gravity.g"

typedef struct {
    bool                processed;
    bool                is_fuzzy;
    
    uint32_t            ncount;
    uint32_t            nsuccess;
    uint32_t            nfailure;
    
    error_type_t        expected_error;
    gravity_value_t     expected_value;
    int32_t             expected_row;
    int32_t             expected_col;
} unittest_data;

typedef enum  {
    OP_COMPILE,         // just compile source code and exit
    OP_RUN,             // just run an already compiled file
    OP_COMPILE_RUN,     // compile source code and run it
    OP_INLINE_RUN,      // compile and execute source passed inline
    OP_REPL,            // run a read eval print loop
    OP_UNITTEST         // unit test mode
} op_type;

static const char *input_file = NULL;
static const char *output_file = DEFAULT_OUTPUT;
static const char *unittest_folder = NULL;
static const char *test_folder_path = NULL;
static bool quiet_flag = false;

static void report_error (gravity_vm *vm, error_type_t error_type, const char *message, error_desc_t error_desc, void *xdata) {
  (void) vm, (void) xdata;
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

static const char *load_file (const char *file, size_t *size, uint32_t *fileid, void *xdata, bool *is_static) {
    (void) fileid, (void) xdata;

	// this callback is called each time an import statement is parsed
	// file arg represents what user wrote after the import keyword, for example:
	// import "file2"
	// import "file2.gravity"
	// import "../file2"
	// import "/full_path_to_file2"

	// it is callback's responsibility to resolve file path based on current working directory
	// or based on user defined search paths
	// and returns:
	// size of file in *size
	// fileid (if any) in *fileid
	// content of file as return value of the function

	// fileid will then be used each time an error is reported by the compiler
	// so it is responsibility of this function to map somewhere the association
	// between fileid and real file/path name

	// fileid is not used in this example
	// xdata not used here but it the xdata field set in the delegate
	// please note than in this simple example the imported file must be
	// in the same folder as the main input file

    if (is_static) *is_static = false;
	if (!file_exists(file)) return NULL;
	return file_read(file, size);
}

// MARK: - Unit Test -

static void unittest_init (const char *target_file, unittest_data *data) {
    (void) target_file;
    ++data->ncount;
    data->processed = false;
}

static void unittest_cleanup (const char *target_file, unittest_data *data) {
    (void) target_file, (void) data;
}

static void unittest_callback (gravity_vm *vm, error_type_t error_type, const char *description, const char *notes, gravity_value_t value, int32_t row, int32_t col, void *xdata) {
    (void) vm;
    unittest_data *data = (unittest_data *)xdata;
    data->expected_error = error_type;
    data->expected_value = value;
    data->expected_row = row;
    data->expected_col = col;
    
    if (notes) printf("\tNOTE: %s\n", notes);
    printf("\t%s\n", description);
}

static void unittest_error (gravity_vm *vm, error_type_t error_type, const char *message, error_desc_t error_desc, void *xdata) {
    (void) vm;
    
    unittest_data *data = (unittest_data *)xdata;
    if (data->processed == true) return; // ignore 2nd error
    data->processed = true;
    
    const char *type = "NONE";
    if (error_type == GRAVITY_ERROR_SYNTAX) type = "SYNTAX";
    else if (error_type == GRAVITY_ERROR_SEMANTIC) type = "SEMANTIC";
    else if (error_type == GRAVITY_ERROR_RUNTIME) type = "RUNTIME";
    else if (error_type == GRAVITY_WARNING) type = "WARNING";
    
    if (error_type == GRAVITY_ERROR_RUNTIME) printf("\tRUNTIME ERROR: ");
    else printf("\t%s ERROR on %d (%d,%d): ", type, error_desc.fileid, error_desc.lineno, error_desc.colno);
    printf("%s\n", message);
    
    bool same_error = (data->expected_error == error_type);
    bool same_row = (data->expected_row != -1) ? (data->expected_row == error_desc.lineno) : true;
    bool same_col = (data->expected_col != -1) ? (data->expected_col == error_desc.colno) : true;
    
    if (data->is_fuzzy) {
        ++data->nsuccess;
        printf("\tSUCCESS\n");
        return;
    }
    
    if (same_error && same_row && same_col) {
        ++data->nsuccess;
        printf("\tSUCCESS\n");
    } else {
        ++data->nfailure;
        printf("\tFAILURE\n");
    }
}

static const char *unittest_read (const char *path, size_t *size, uint32_t *fileid, void *xdata, bool *is_static) {
    (void) fileid, (void) xdata;
    if (is_static) *is_static = false;
    
    if (file_exists(path)) return file_read(path, size);
    
    // this unittest is able to resolve path only next to main test folder (not in nested folders)
    const char *newpath = file_buildpath(path, test_folder_path);
    if (!newpath) return NULL;
    
    const char *buffer = file_read(newpath, size);
    mem_free(newpath);
    
    return buffer;
}

static void unittest_scan (const char *folder_path, unittest_data *data) {
    DIRREF dir = directory_init(folder_path);
    if (!dir) return;
    #ifdef WIN32
    char outbuffer[MAX_PATH];
    #else
    char *outbuffer = NULL;
    #endif
    const char *target_file;
    while ((target_file = directory_read(dir, outbuffer))) {
        
        // if file is a folder then start recursion
        const char *full_path = file_buildpath(target_file, folder_path);
        if (is_directory(full_path)) {
            // skip disabled folder
            if (strcmp(target_file, "disabled") == 0) continue;
            unittest_scan(full_path, data);
            continue;
        }
        
        // test only files with a .gravity extension
        if (strstr(full_path, ".gravity") == NULL) continue;
        data->is_fuzzy = (strstr(full_path, "/fuzzy/") != NULL);
        
        // load source code
        size_t size = 0;
        const char *source_code = file_read(full_path, &size);
        assert(source_code);
        
        // start unit test
        unittest_init(target_file, data);
        
        // compile and run source code
        printf("\n%d\tTest file: %s\n", data->ncount, target_file);
        printf("\tTest path: %s\n", full_path);
        mem_free(full_path);
        
        // initialize compiler and delegates
        gravity_delegate_t delegate = {
            .xdata = (void *)data,
            .error_callback = unittest_error,
            .unittest_callback = unittest_callback,
            .loadfile_callback = unittest_read
        };
        
        gravity_compiler_t *compiler = gravity_compiler_create(&delegate);
        gravity_closure_t *closure = gravity_compiler_run(compiler, source_code, size, 0, false, false);
        gravity_vm *vm = gravity_vm_new(&delegate);
        gravity_compiler_transfer(compiler, vm);
        gravity_compiler_free(compiler);
        
        if (closure) {
            if (gravity_vm_runmain(vm, closure)) {
                data->processed = true;
                gravity_value_t result = gravity_vm_result(vm);
                if (data->is_fuzzy || gravity_value_equals(result, data->expected_value)) {
                    ++data->nsuccess;
                    printf("\tSUCCESS\n");
                } else {
                    ++data->nfailure;
                    printf("\tFAILURE\n");
                }
                gravity_value_free(NULL, data->expected_value);
            }
        }
        gravity_vm_free(vm);
        
        // case for empty files or simple declarations test
        if (!data->processed) {
            ++data->nsuccess;
            printf("\tSUCCESS\n");
        }
        
        // cleanup unitest
        unittest_cleanup(target_file, data);
    }
}

// MARK: - General -

static void print_version (void) {
    printf("Gravity version %s (%s)\n", GRAVITY_VERSION, GRAVITY_BUILD_DATE);
}

static void print_help (void) {
    printf("Usage: gravity [options] [arguments...]\n");
    printf("\n");
    printf("To start the REPL (not yet supported):\n");
    printf("  gravity\n");
    printf("\n");
    printf("To compile and execute file:\n");
    printf("  gravity example.gravity\n");
    printf("\n");
    printf("Available options are:\n");
    printf("  --version          show version information and exit\n");
    printf("  --help             show command line usage and exit\n");
    printf("  -c input_file      compile input_file\n");
    printf("  -o output_file     specify output file name (default to gravity.json)\n");
    printf("  -x input_file      execute input_file (JSON format expected)\n");
    printf("  -i source_code     compile and execute source_code string\n");
    printf("  -q                 don't print result and execution time\n");
    printf("  -t folder          run unit tests from folder\n");
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
        else if ((strcmp(argv[i], "-i") == 0) && (i+1 < argc)) {
            input_file = argv[++i];
            type = OP_INLINE_RUN;
        }
        else if (strcmp(argv[i], "-q") == 0) {
            quiet_flag = true;
        }
        else if ((strcmp(argv[i], "-t") == 0) && (i+1 < argc)) {
            unittest_folder = argv[++i];
            type = OP_UNITTEST;
        }
        else {
            input_file = argv[i];
            type = OP_COMPILE_RUN;
        }
    }

    return type;
}

// MARK: - Special Modes -

static void gravity_repl (void) {

    printf("REPL not yet implemented.\n");
    exit(0);

    /*
    // setup compiler/VM delegate
    gravity_delegate_t delegate = {
        .error_callback = report_error,
    };

    gravity_compiler_t    *compiler = gravity_compiler_create(&delegate);
    gravity_vm    *vm = gravity_vm_new(&delegate);
    char        *line = NULL;
    int            length = 0;

    printf("Welcome to Gravity v%s\n", GRAVITY_VERSION);
    while((line = readline("> ", &length)) != NULL) {
        // to be implemented
        //    gravity_compiler_eval(compiler, vm, line, length);
        free(line);
    }

    gravity_compiler_free(compiler);
    gravity_vm_free(vm);
    */
}

static void gravity_unittest (void) {
    unittest_data data = {
        .ncount = 0,
        .nsuccess = 0,
        .nfailure = 0
    };
    
    if (unittest_folder == NULL) {
        printf("Usage: gravity -t /path/to/unitest/\n");
        exit(-1);
    }
    
    // print console header
    printf("==============================================\n");
    printf("Gravity Unit Test Mode\n");
    printf("Gravity version %s\n", GRAVITY_VERSION);
    printf("Build date: %s\n", GRAVITY_BUILD_DATE);
    printf("==============================================\n");
    
    mem_init();
    nanotime_t tstart = nanotime();
    test_folder_path = unittest_folder;
    unittest_scan(unittest_folder, &data);
    nanotime_t tend = nanotime();
    
    double result = ((double)((data.nsuccess * 100)) / (double)data.ncount);
    printf("\n\n");
    printf("==============================================\n");
    printf("Total Tests: %d\n", data.ncount);
    printf("Total Successes: %d\n", data.nsuccess);
    printf("Total Failures: %d\n", data.nfailure);
    printf("Result: %.2f %%\n", result);
    printf("Time: %.4f ms\n", millitime(tstart, tend));
    printf("==============================================\n");
    printf("\n");
    
    // If we have 1 or more failures, return an error.
    if (data.nfailure != 0) {
        exit(1);
    }
    
    exit(0);
}

// MARK: -

int main (int argc, const char* argv[]) {
    // parse arguments and return operation type
    op_type type = parse_args(argc, argv);

    // special repl case
    if (type == OP_REPL) gravity_repl();
    
    // special unit test mode
    if (type == OP_UNITTEST) gravity_unittest();

    // initialize memory debugger (if activated)
    mem_init();

    // closure to execute/serialize
    gravity_closure_t *closure = NULL;

    // optional compiler
    gravity_compiler_t *compiler = NULL;

    // setup compiler/VM delegate
    gravity_delegate_t delegate = {
        .error_callback = report_error,
        .loadfile_callback = load_file
    };

    // create VM
    gravity_vm *vm = gravity_vm_new(&delegate);

    // pass argc and argv to the ENV class
    gravity_env_register_args(vm, argc, argv);

    // check if input file is source code that needs to be compiled
    if ((type == OP_COMPILE) || (type == OP_COMPILE_RUN) || (type == OP_INLINE_RUN)) {

        // load source code
        size_t size = 0;
        const char *source_code = NULL;

        if (type == OP_INLINE_RUN) {
            source_code = input_file;
            size = strlen(input_file);
        } else {
            source_code = file_read(input_file, &size);
        }

        // sanity check
        if (!source_code || !size) {
            printf("Error loading %s %s\n", (type == OP_INLINE_RUN) ? "source" : "file", input_file);
            goto cleanup;
        }

        // create closure to execute inline code
        if (type == OP_INLINE_RUN) {
            char *buffer = mem_alloc(NULL, size+1024);
            assert(buffer);
            size = snprintf(buffer, size+1024, "func main() {%s};", input_file);
            source_code = buffer;
        }

        // create compiler
        compiler = gravity_compiler_create(&delegate);

        // compile source code into a closure
        closure = gravity_compiler_run(compiler, source_code, size, 0, false, true);
        if (!closure) goto cleanup;

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

    if (gravity_vm_runmain(vm, closure)) {
        gravity_value_t result = gravity_vm_result(vm);
        double t = gravity_vm_time(vm);

        char buffer[512];
        gravity_value_dump(vm, result, buffer, sizeof(buffer));
        if (!quiet_flag) {
            printf("RESULT: %s (in %.4f ms)\n\n", buffer, t);
        }
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
