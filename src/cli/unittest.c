//
//  unittest.c
//  gravity
//
//  Created by Marco Bambini on 23/03/16.
//  Copyright © 2016 CreoLabs. All rights reserved.
//

#include "gravity_compiler.h"
#include "gravity_utils.h"
#include "gravity_core.h"
#include "gravity_vm.h"

typedef struct {
	bool			processed;
	
	uint32_t		ncount;
	uint32_t		nsuccess;
	uint32_t		nfailure;
	
	error_type_t	expected_error;
	gravity_value_t expected_value;
	int32_t			expected_row;
	int32_t			expected_col;
} test_data;

static void unittest_init (const char *target_file, test_data *data) {
	#pragma unused(target_file)
	++data->ncount;
	data->processed = false;
}

static void unittest_cleanup (const char *target_file, test_data *data) {
	#pragma unused(target_file,data)
}

static void	unittest_callback (error_type_t error_type, const char *description, const char *notes, gravity_value_t value, int32_t row, int32_t col, void *xdata) {
	test_data *data = (test_data *)xdata;
	data->expected_error = error_type;
	data->expected_value = value;
	data->expected_row = row;
	data->expected_col = col;
	
	if (notes) printf("\tNOTE: %s\n", notes);
	printf("\t%s\n", description);
}

// MARK: -

static void callback_error (error_type_t error_type, const char *message, error_desc_t error_desc, void *xdata) {
	test_data *data = (test_data *)xdata;
	
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
	
	if (same_error && same_row && same_col) {
		++data->nsuccess;
		printf("\tSUCCESS\n");
	} else {
		++data->nfailure;
		printf("\tFAILURE\n");
	}
}

static const char *callback_read (const char *path, size_t *size, uint32_t *fileid, void *xdata) {
	#pragma unused(fileid,xdata)
	return file_read(path, size);
}

static void test_folder (const char *folder_path, test_data *data) {
	DIRREF dir = directory_init(folder_path);
	if (!dir) return;
	
	const char *target_file;
	while ((target_file = directory_read(dir))) {
		// if file is a folder then start recursion
		const char *full_path = file_buildpath(target_file, folder_path);
		if (is_directory(full_path)) {
			// skip disabled folder
			if (strcmp(target_file, "disabled") == 0) continue;
			
			test_folder(full_path, data);
			continue;
		}
		
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
			.error_callback = callback_error,
			.unittest_callback = unittest_callback,
			.loadfile_callback = callback_read
		};
		
		gravity_compiler_t *compiler = gravity_compiler_create(&delegate);
		gravity_closure_t *closure = gravity_compiler_run(compiler, source_code, size, 0, false);
		gravity_vm *vm = gravity_vm_new(&delegate);
		gravity_compiler_transfer(compiler, vm);
		gravity_compiler_free(compiler);
		
		if (closure) {
			if (gravity_vm_run(vm, closure)) {
				data->processed = true;
				gravity_value_t result = gravity_vm_result(vm);
				if (gravity_value_equals(result, data->expected_value)) {
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

int main (int argc, const char* argv[]) {
	test_data data = {
		.ncount = 0,
		.nsuccess = 0,
		.nfailure = 0
	};
	
	if (argc != 2) {
		printf("Usage: unittest /path/to/unitest/\n");
		return 0;
	}
	
	// print console header
	printf("==============================================\n");
	printf("Gravity UnitTest\n");
	printf("Gravity version %s\n", GRAVITY_VERSION);
	printf("Build date: %s\n", GRAVITY_BUILD_DATE);
	printf("==============================================\n");
	
	mem_init();
	nanotime_t tstart = nanotime();
	test_folder(argv[1], &data);
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
	
	return 0;
}
