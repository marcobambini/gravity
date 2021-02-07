//
//  console.c
//  gravity
//
//  Created by Marco Bambini on 23/02/15.
//  Copyright (c) 2015 CreoLabs. All rights reserved.
//

#include "console.h"

// MUST BE CALLED FROM main file
const char *current_filepath (const char *base, const char *target_file) {
	
	// if it is an absolute path then return the path itself
	if (target_file[0] == '/') return target_file;
	
	static char buffer[4096];
	static size_t skip = strlen("GravityObjC/ObjC/main.c");
	
	// __FILE__ macro contains full path to main.c file
	// for example: /Users/marco/SQLabs/Butterfly/gravity/main/main.c
	
	snprintf(buffer, strlen(base) - skip, "%s", base);
	strcat(buffer, "/shared/");
	strcat(buffer, target_file);
	
	return buffer;
}

void report_log (const char *message, void *xdata) {
	#pragma unused(xdata)
	printf("LOG: %s\n", message);
}

void report_error (gravity_vm *vm, error_type_t error_type, const char *message, error_desc_t error_desc, void *xdata) {
	#pragma unused(vm, xdata)
    
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
