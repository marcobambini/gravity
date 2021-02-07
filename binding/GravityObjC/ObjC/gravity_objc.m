//
//  gravity_objc.c
//  GravityObjC
//
//  Created by Marco Bambini on 07/02/21.
//

#import <Foundation/Foundation.h>
#import <objc/runtime.h>

#import "gravity_macros.h"
#import "gravity_utils.h"
#import "gravity_core.h"
#import "gravity_hash.h"
#import "gravity_objc.h"

// MARK: - Macros -

#define GRAVITY_BRIDGE_DEGUB            0
#define GRAVITY_BRIDGE_DEGUB_XDATA      0
#define GRAVITY_BRIDGE_DEBUG_MEMORY     0
#define GRAVITY_BRIDGE_RETAIN           1

#if GRAVITY_BRIDGE_RETAIN
#define RETAIN_OBJC_VALUE(v)            (void *)CFBridgingRetain(v)
#define FREE_OBJC_VALUE(v)              CFBridgingRelease((CFTypeRef)v)
#else
#define RETAIN_OBJC_VALUE(v)            ((__bridge void*)v)
#define FREE_OBJC_VALUE(v)
#endif

#if GRAVITY_BRIDGE_DEGUB
#define DEBUG_BRIDGE(...)               NSLog(__VA_ARGS__)
#else
#define DEBUG_BRIDGE(...)
#endif

#if GRAVITY_BRIDGE_DEGUB_XDATA
#define DEBUG_XDATA(...)                NSLog(__VA_ARGS__)
#else
#define DEBUG_XDATA(...)
#endif

#define BRIDGE_NAME                     "ObjC"
#define BRIDGE_LOAD                     "register"
#define BRIDGE_EXECUTE                  "exec"          // MUST BE equal to GRAVITY_INTERNAL_EXEC_NAME

#define RETURN_VALUE(_v,_i)             do {gravity_vm_setslot(vm, _v, _i); return true;} while(0)
#define RETURN_NOVALUE()                return true
#define RETURN_ERROR(...)               do {                                                                        \
                                            char buffer[4096];                                                        \
                                            snprintf(buffer, sizeof(buffer), __VA_ARGS__);                            \
                                            gravity_fiber_seterror(gravity_vm_fiber(vm), (const char *) buffer);    \
                                            return false;                                                            \
                                        } while(0)

#define CHECK_INT(_v)                   (VALUE_ISA_INT(_v) || VALUE_ISA_BOOL(_v) || VALUE_ISA_NULL(_v))
#define CHECK_FLOAT(_v)                 (VALUE_ISA_FLOAT(_v))
#define CHECK_NUMBER(_v)                (CHECK_INT(_v) || CHECK_FLOAT(_v))
#define CONVERT_NUMBER(_v)              VALUE_ISA_FLOAT(_v) ? _v.f : _v.n
#define SANITY_CHECK_VALUE(_v)          if (VALUE_ISA_NULL(_v) || (VALUE_ISA_NOTVALID(_v))) return nil;

#define RETURN_NIL_ON_NULL              1

// MARK: - Common native ObjC type -

// opaque data types
typedef struct objc_bridge_var_t objc_bridge_var_t;
typedef struct objc_bridge_func_t objc_bridge_func_t;

typedef NS_ENUM(uint32_t, objc_bridge_type) {
    OBJC_BRIDGE_TYPE_UNKNOWN        =    0,             // unhandled case
    
    // basic C types: https://en.wikipedia.org/wiki/C_data_types
    OBJC_BRIDGE_TYPE_VOID           =    1,             // no return type
    OBJC_BRIDGE_TYPE_INT8           =    2,             // char, signed char
    OBJC_BRIDGE_TYPE_INT16          =    3,             // short, short int, signed short, signed short int
    OBJC_BRIDGE_TYPE_INT32          =    4,             // int, long, long int, signed long, signed long int
    OBJC_BRIDGE_TYPE_INT64          =    5,             // long long, long long int, signed long long, signed long long int
    OBJC_BRIDGE_TYPE_UINT8          =    6,             // unsigned char
    OBJC_BRIDGE_TYPE_UINT16         =    7,             // unsigned short, unsigned short int
    OBJC_BRIDGE_TYPE_UINT32         =    8,             // unsigned int, unsigned long, unsigned long int
    OBJC_BRIDGE_TYPE_UINT64         =    9,             // unsigned long long, unsigned long long int
    OBJC_BRIDGE_TYPE_FLOAT          =    10,            // float
    OBJC_BRIDGE_TYPE_DOUBLE         =    11,            // double
    OBJC_BRIDGE_TYPE_LDOUBLE        =    12,            // long double (unused)
    OBJC_BRIDGE_TYPE_BOOL           =    13,            // bool, boolean, Boolean, BOOL
    OBJC_BRIDGE_TYPE_VPTR           =    14,            // void*
    OBJC_BRIDGE_TYPE_CPTR           =    15,            // char*
    
    // ObjC basic types
    OBJC_BRIDGE_TYPE_INIT           =    16,            // implicit return value of init
    OBJC_BRIDGE_TYPE_ID             =    17,            // for OBJC_BRIDGE_TYPE_ID it is object responsibility to sanity check it
    OBJC_BRIDGE_TYPE_CLASS          =    18,            // UNUSED
    OBJC_BRIDGE_TYPE_SEL            =    19,            // UNUSED
    OBJC_BRIDGE_TYPE_NSINTEGER      =    20,            // on 32-bit systems, these are defined to be 32-bit signed/unsigned,
    OBJC_BRIDGE_TYPE_NSUINTEGER     =    21,            // and on 64-bit systems, they are 64-bit integers.
    OBJC_BRIDGE_TYPE_NSNUMBER       =    22,
    OBJC_BRIDGE_TYPE_NSSTRING       =    23,
    OBJC_BRIDGE_TYPE_NSARRAY        =    24,
    OBJC_BRIDGE_TYPE_NSDICTIONARY   =    25,
    OBJC_BRIDGE_TYPE_NSDATE         =    26,
    OBJC_BRIDGE_TYPE_NSDATA         =    27,
    
    // Struct based types
    OBJC_BRIDGE_TYPE_POINT          =    28,
    OBJC_BRIDGE_TYPE_RECT           =    29,
    OBJC_BRIDGE_TYPE_SIZE           =    30,
    OBJC_BRIDGE_TYPE_EDGEINSETS     =    31,
    OBJC_BRIDGE_TYPE_OFFSET         =    32,
    OBJC_BRIDGE_TYPE_RANGE          =    33,
    
    // Custom complex types
    OBJC_BRIDGE_TYPE_IMAGE          =    34,
    OBJC_BRIDGE_TYPE_COLOR          =    35,
    OBJC_BRIDGE_TYPE_GRADIENT       =    36,
    OBJC_BRIDGE_TYPE_MOVIE          =    37,
    OBJC_BRIDGE_TYPE_SOUND          =    38,
    OBJC_BRIDGE_TYPE_FONT           =    39,
    
    OBJC_BRIDGE_TYPE_GRAVITY        =    40,             // pass through of native gravity objects
    OBJC_BRIDGE_TYPE_CLOSURE        =    41,             // must be closure function (or a runtime error is generated)
    
    OBJC_BRIDGE_TYPE_USER           =   100,             // MUST BE LATEST ENTRY means handled by the bridge conversion protocol
} ;

// MARK: - Internal Prototypes -

static inline gravity_value_t convert_id2gravity (gravity_vm *vm, id value);
static inline NSValue *convert_gravity2nsrangevalue (gravity_vm *vm, gravity_value_t value);
static inline NSDictionary *convert_gravity2nsdictionary (gravity_vm *vm, gravity_value_t v);
static inline NSArray* convert_gravity2nsarray (gravity_vm *vm, gravity_value_t v);

bool bridge_initinstance (gravity_vm *vm, void *xdata, gravity_value_t ctx, gravity_instance_t *instance, gravity_value_t args[], int16_t nargs);
bool bridge_setvalue (gravity_vm *vm, void *xdata, gravity_value_t target, const char *key, gravity_value_t value);
bool bridge_getvalue (gravity_vm *vm, void *xdata, gravity_value_t target, const char *key, uint32_t rindex);
bool bridge_setundef (gravity_vm *vm, void *xdata, gravity_value_t target, const char *key, gravity_value_t value);
bool bridge_getundef (gravity_vm *vm, void *xdata, gravity_value_t target, const char *key, uint32_t rindex);
bool bridge_execute  (gravity_vm *vm, void *xdata, gravity_value_t ctx, gravity_value_t args[], int16_t nargs, uint32_t rindex);
void bridge_blacken  (gravity_vm *vm, void *xdata);
void *bridge_duplicate (gravity_vm *vm, void *xdata);
void bridge_free (gravity_vm *vm, gravity_object_t *obj);
bool bridge_equals (gravity_vm *vm, void *obj_1, void *obj_2);
void *bridge_clone (gravity_vm *vm, void *xdata);
const char *bridge_string (gravity_vm *vm, void *xdata, uint32_t *len);

// xdata
objc_bridge_var_t *objc_bridge_var_new (objc_bridge_type type, const char *key);
void objc_bridge_var_set_type (objc_bridge_var_t *xdata, objc_bridge_type type);
void objc_bridge_var_free (objc_bridge_var_t *xdata);

objc_bridge_func_t *objc_bridge_func_new (SEL selector, const char *name, uint16_t nargs, objc_bridge_type rettype);
void objc_bridge_func_set_name (objc_bridge_func_t *xdata, const char *name);
void objc_bridge_func_set_rettype (objc_bridge_func_t *xdata, objc_bridge_type rettype);
void objc_bridge_func_set_argtype (objc_bridge_func_t *xdata, objc_bridge_type type, uint8_t index);
void objc_bridge_func_set_argvalue (objc_bridge_func_t *xdata, id value, uint8_t index);
void objc_bridge_func_free (objc_bridge_func_t *xdata);

// conversion
gravity_instance_t *bridge_instance_byclassname (gravity_vm *vm, id value, const char* name, uint32_t length);
gravity_value_t bridge_objc2gravity (gravity_vm *vm, id obj, objc_bridge_type type);
id bridge_gravity2objc (gravity_vm *vm, gravity_value_t value, objc_bridge_type type);

const char *bridge_property_name(gravity_vm *vm, gravity_class_t *c, const char *key);
objc_bridge_type bridge_property_type(gravity_vm *vm, gravity_class_t *c, const char *key);

gravity_class_t *objc_class_load (gravity_vm *vm, const char *name);

// MARK: - Internal Types -

typedef NS_ENUM(uint8_t, objc_bridge_tag) {
    OBJC_BRIDGE_TAG_METHOD      = 0,
    OBJC_BRIDGE_TAG_PROPERTY    = 1
};

struct objc_bridge_func_t {
    objc_bridge_tag     tag;
    SEL                 selector;
    void                *invocation;
    
    // exposed name (for better error reporting)
    const char          *name;
    
    objc_bridge_type    rettype;
    NSUInteger          retlength;
    
    uint16_t            nargs;
    objc_bridge_type    *argtype;
    void                **argvalue;
} objc_bridge_func_s;

struct objc_bridge_var_t {
    objc_bridge_tag     tag;
    objc_bridge_type    type;
    const char          *key;
    
    // exposed name (for better error reporting)
    const char          *name;
} objc_bridge_var_s;

// MARK: - Core functions -

static const char *objc_build_function_name (const char *name, char *buffer, size_t bsize) {
    size_t len = strlen(name);
    if (len > bsize) len = bsize - 1;
    
    bzero(buffer, bsize);
    for (size_t i=0; i<len; ++i) {
        if (name[i] == ':') break;
        buffer[i] = name[i];
    }
    
    return buffer;
}

static objc_bridge_type objc_decode_type (const char *c) {
    int idx = 0;
    
    // take in account Objective-C annotations for method parameters and return values
    // from: https://developer.apple.com/library/mac/documentation/Cocoa/Conceptual/ObjCRuntimeGuide/Articles/ocrtTypeEncodings.html
    // explanation: http://stackoverflow.com/questions/5609564/objective-c-in-out-inout-byref-byval-and-so-on-what-are-they
    switch (c[0]) {
        case 'r': // const
        case 'n': // in
        case 'N': // inout
        case 'o': // out
        case 'O': // bycopy
        case 'R': // byref
        case 'V': // oneway
            ++idx;
    }
    
    switch (c[idx]) {
        case 'c': return OBJC_BRIDGE_TYPE_INT8;
        case 'i': return OBJC_BRIDGE_TYPE_INT32;
        case 's': return OBJC_BRIDGE_TYPE_INT16;
        case 'l': return OBJC_BRIDGE_TYPE_INT32;
        case 'q': return OBJC_BRIDGE_TYPE_INT64;
        case 'C': return OBJC_BRIDGE_TYPE_UINT8;
        case 'I': return OBJC_BRIDGE_TYPE_UINT32;
        case 'S': return OBJC_BRIDGE_TYPE_UINT16;
        case 'L': return OBJC_BRIDGE_TYPE_UINT32;
        case 'Q': return OBJC_BRIDGE_TYPE_UINT64;
        case 'f': return OBJC_BRIDGE_TYPE_FLOAT;
        case 'd': return OBJC_BRIDGE_TYPE_DOUBLE;
        case 'B': return OBJC_BRIDGE_TYPE_BOOL;
        case 'v': return OBJC_BRIDGE_TYPE_VOID;
        case '@': {
            if (c[idx+1] == '"') {
                // FOUNDATION
                if (strncmp(&c[idx+2], "NSString", 8) == 0) return OBJC_BRIDGE_TYPE_NSSTRING;
                if (strncmp(&c[idx+2], "NSMutableString", 15) == 0) return OBJC_BRIDGE_TYPE_NSSTRING;
                if (strncmp(&c[idx+2], "NSNumber", 8) == 0) return OBJC_BRIDGE_TYPE_NSNUMBER;
                if (strncmp(&c[idx+2], "NSDecimalNumber", 15) == 0) return OBJC_BRIDGE_TYPE_NSNUMBER;
                if (strncmp(&c[idx+2], "NSArray", 7) == 0) return OBJC_BRIDGE_TYPE_NSARRAY;
                if (strncmp(&c[idx+2], "NSMutableArray", 14) == 0) return OBJC_BRIDGE_TYPE_NSARRAY;
                if (strncmp(&c[idx+2], "NSDictionary", 12) == 0) return OBJC_BRIDGE_TYPE_NSDICTIONARY;
                if (strncmp(&c[idx+2], "NSMutableDictionary", 19) == 0) return OBJC_BRIDGE_TYPE_NSDICTIONARY;
                if (strncmp(&c[idx+2], "NSData", 6) == 0) return OBJC_BRIDGE_TYPE_NSDATA;
                if (strncmp(&c[idx+2], "NSMutableData", 13) == 0) return OBJC_BRIDGE_TYPE_NSDATA;
                if (strncmp(&c[idx+2], "NSDate", 6) == 0) return OBJC_BRIDGE_TYPE_NSDATE;
                // IMAGE
                if (strncmp(&c[idx+2], "NSImage", 7) == 0) return OBJC_BRIDGE_TYPE_IMAGE;
                if (strncmp(&c[idx+2], "UIImage", 7) == 0) return OBJC_BRIDGE_TYPE_IMAGE;
                if (strncmp(&c[idx+2], "CREOImage", 9) == 0) return OBJC_BRIDGE_TYPE_IMAGE;
                // COLOR
                if (strncmp(&c[idx+2], "NSColor", 7) == 0) return OBJC_BRIDGE_TYPE_COLOR;
                if (strncmp(&c[idx+2], "UIColor", 7) == 0) return OBJC_BRIDGE_TYPE_COLOR;
                if (strncmp(&c[idx+2], "CREOColor", 9) == 0) return OBJC_BRIDGE_TYPE_COLOR;
                // GRADIENT
                if (strncmp(&c[idx+2], "NSGradient", 10) == 0) return OBJC_BRIDGE_TYPE_GRADIENT;
                if (strncmp(&c[idx+2], "UIGradient", 10) == 0) return OBJC_BRIDGE_TYPE_GRADIENT;
                if (strncmp(&c[idx+2], "CREOGradient", 12) == 0) return OBJC_BRIDGE_TYPE_GRADIENT;
                // FONT
                if (strncmp(&c[idx+2], "NSFont", 6) == 0) return OBJC_BRIDGE_TYPE_FONT;
                if (strncmp(&c[idx+2], "UIFont", 6) == 0) return OBJC_BRIDGE_TYPE_FONT;
                if (strncmp(&c[idx+2], "CREOFont", 8) == 0) return OBJC_BRIDGE_TYPE_FONT;
                // SOUND
                if (strncmp(&c[idx+2], "NSSound", 7) == 0) return OBJC_BRIDGE_TYPE_SOUND;
                if (strncmp(&c[idx+2], "UISound", 7) == 0) return OBJC_BRIDGE_TYPE_SOUND;
                if (strncmp(&c[idx+2], "CREOSound", 9) == 0) return OBJC_BRIDGE_TYPE_SOUND;
                // MOVIE
                if (strncmp(&c[idx+2], "CREOMovie", 9) == 0) return OBJC_BRIDGE_TYPE_MOVIE;
                // RECT
                if (strncmp(&c[idx+2], "CREORect", 8) == 0) return OBJC_BRIDGE_TYPE_RECT;
                // POINT
                if (strncmp(&c[idx+2], "CREOPoint", 9) == 0) return OBJC_BRIDGE_TYPE_POINT;
                // SIZE
                if (strncmp(&c[idx+2], "CREOSize", 8) == 0) return OBJC_BRIDGE_TYPE_SIZE;
                
            } return OBJC_BRIDGE_TYPE_ID;
        }
        case '{':
            if (c[idx+1] == '"') {
//                if (strncmp(&c[idx+2], "NSPoint", 7) == 0) return OBJC_BRIDGE_TYPE_POINT;
//                if (strncmp(&c[idx+2], "CGPoint", 7) == 0) return OBJC_BRIDGE_TYPE_POINT;
//
//                if (strncmp(&c[idx+2], "NSRect", 6) == 0) return OBJC_BRIDGE_TYPE_RECT;
//                if (strncmp(&c[idx+2], "CGRect", 6) == 0) return OBJC_BRIDGE_TYPE_RECT;
//
//                if (strncmp(&c[idx+2], "NSSize", 6) == 0) return OBJC_BRIDGE_TYPE_SIZE;
//                if (strncmp(&c[idx+2], "CGSize", 6) == 0) return OBJC_BRIDGE_TYPE_SIZE;
//
//                if (strncmp(&c[idx+2], "NSEdgeInsets", 12) == 0) return OBJC_BRIDGE_TYPE_EDGEINSETS;
//                if (strncmp(&c[idx+2], "UIEdgeInsets", 12) == 0) return OBJC_BRIDGE_TYPE_EDGEINSETS;
//
//                if (strncmp(&c[idx+2], "UIOffset", 8) == 0) return OBJC_BRIDGE_TYPE_OFFSET;
//
//                if (strncmp(&c[idx+2], "NSRange", 7) == 0) return OBJC_BRIDGE_TYPE_RANGE;
            } return OBJC_BRIDGE_TYPE_UNKNOWN;
            
        case '*': return OBJC_BRIDGE_TYPE_CPTR;
        case '#': return OBJC_BRIDGE_TYPE_CLASS;
        case ':': return OBJC_BRIDGE_TYPE_SEL;
        case '^': return OBJC_BRIDGE_TYPE_VPTR;
        case '?': return OBJC_BRIDGE_TYPE_UNKNOWN;
        //case '[': return OBJC_BRIDGE_TYPE_ARRAY;
        //case '(': return OBJC_BRIDGE_TYPE_UNION;
        //case 'b': return OBJC_BRIDGE_TYPE_BIT;
    }
    
    return OBJC_BRIDGE_TYPE_UNKNOWN;
}

static objc_bridge_type objc_decode_attributes (const char *attributes, bool *readonly, NSMutableDictionary *toskip) {
    const char *p = attributes;
    
    *readonly = false;
    objc_bridge_type type = OBJC_BRIDGE_TYPE_UNKNOWN;
    
    while (p[0]) {
        switch (p[0]) {
            case 'T': {
                // property type
                type = objc_decode_type(&p[1]);
            } break;
            
            case 'V': {
                // property name
            } break;
            
            case 'R': {
                // property readonly flag
                *readonly = true;
            } break;
                
            case 'G':
            case 'S': {
                // property custom getter/setter names
                // must be added to toSkip dictionary
                NSString *customName = [NSString stringWithUTF8String:&p[1]];
                customName = [customName substringToIndex:[customName length] - 1];
                toskip[customName] = [NSNull null];
            } break;
        }
        
        // skip next
        while (p[0]) {
            ++p; if (p[0] == ',') {++p; break;}
        }
    }
    
    return type;
}

// for some strange reasons some properties are reported as methods for example UIView alpha
static BOOL objc_check_fake_method (Class native_class, gravity_class_t *c, NSString *name, Method m, NSMutableDictionary *toskip) {
    // check if this method is not really a method but a property
    // for example UIView.h defines alpha as a CGFloat property
    // but runtime reports alpha as a pair of methods (a getter and a setter)
    
    // skip init cases
    if ([name hasPrefix:@"init"]) return NO;
    
    BOOL isFake = NO;
    NSString *getterName = NULL;
    NSString *setterName = NULL;
    Method     getter = NULL;
        
    if ([name hasPrefix:@"set"]) {
        setterName = name;
        NSString *temp = [name substringFromIndex:3];
        NSString *firstChar = [[temp substringToIndex:1] lowercaseString];
        getterName = [firstChar stringByAppendingString:[temp substringFromIndex:1]];
        getterName = [getterName substringToIndex:[getterName length] - 1];
        getter = class_getInstanceMethod(native_class, NSSelectorFromString(setterName));
        isFake = (getter != nil);
    } else {
        getter = m;
        getterName = name;
        NSString *firstChar = [[name substringToIndex:1] uppercaseString];
        setterName = [firstChar stringByAppendingString:[name substringFromIndex:1]];
        setterName = [NSString stringWithFormat:@"set%@:", setterName];
        Method setter = class_getInstanceMethod(native_class, NSSelectorFromString(setterName));
        isFake = (setter != nil);
    }
    if (!isFake) return NO;
    
    // so it seems a fake method
    
    // check number of arguments (it is a getter so they must be 2, self, _CMD)
    unsigned int nparams = method_getNumberOfArguments(getter);
    if (nparams != 2) return NO;
    
    // check return type
    char buffer[1024];
    method_getReturnType(getter, buffer, sizeof(buffer));
    objc_bridge_type type = objc_decode_type(buffer);
    
    // a getter that returns a void cannot be a property
    if (type == OBJC_BRIDGE_TYPE_VOID) return NO;
    
    // let's convert it to a property using the getter return value
    // create gravity property and bind it to the class
    const char *property_name = [getterName UTF8String];
    objc_bridge_var_t *xdata = objc_bridge_var_new(type, NULL);
    
    bool readonly = false;
    gravity_closure_t *fget = gravity_closure_new(NULL, gravity_function_new_bridged(NULL, NULL, (void *)xdata));
    gravity_closure_t *fset = (readonly) ? NULL : fget;
    gravity_closure_t *closure = gravity_closure_new(NULL, gravity_function_new_special(NULL, NULL, GRAVITY_BRIDGE_INDEX, fget, fset));
    gravity_class_bind(c, property_name, VALUE_FROM_OBJECT(closure));
    
    // set names to skip for next loops
    toskip[getterName] = [NSNull null];
    toskip[setterName] = [NSNull null];
    
    return YES;
}

static void objc_class_scan (gravity_vm* vm, Class native_class, gravity_class_t *c) {
    #pragma unused(vm)
    
    // setup a toSkip dictionary in order to not process custom getter and setter
    NSMutableDictionary *toskip = [[NSMutableDictionary alloc] init];
    
    DEBUG_BRIDGE(@"Scanning class: %@", NSStringFromClass(native_class));
    
    // process properties
    unsigned int n = 0;
    objc_property_t *plist = class_copyPropertyList(native_class, &n);
    for (unsigned int i=0; i<n; ++i) {
        const char *name = property_getName(plist[i]);
        const char *attributes = property_getAttributes(plist[i]);
        
        // reserved internal objc properties to skip
        if (name[0] == '.') continue;
        if (name[0] == '_') continue;
        
        // since it is a property we need to skip getter and setter methods
        // setup standard getter and setter names
        NSString *propertyName = [NSString stringWithUTF8String:name];
        
        // don't know why but UIView reports some properties twice
        // so I need to check for duplicates here
        if (toskip[propertyName]) continue;
        
        // standard getter
        toskip[propertyName] = [NSNull null];
        
        // standard setter
        NSString *firstChar = [propertyName substringToIndex:1];
        NSString *standardSetter = [[firstChar uppercaseString] stringByAppendingString:[propertyName substringFromIndex:1]];
        toskip[[NSString stringWithFormat:@"set%@:", standardSetter]] = [NSNull null];
        
        DEBUG_BRIDGE(@"Property %d/%d: %@", i, n, propertyName);
        
        bool readonly;
        objc_bridge_type type = objc_decode_attributes(attributes, &readonly, toskip);
        objc_bridge_var_t *xdata = objc_bridge_var_new(type, NULL);
        
        gravity_closure_t *getter = gravity_closure_new(NULL, gravity_function_new_bridged(NULL, NULL, (void *)xdata));
        gravity_closure_t *setter = (readonly) ? NULL : getter;
        gravity_closure_t *closure = gravity_closure_new(NULL, gravity_function_new_special(NULL, NULL, GRAVITY_BRIDGE_INDEX, getter, setter));
        gravity_class_bind(c, name, VALUE_FROM_OBJECT(closure));
    }
    if (plist) free(plist);
    
    // process methods
    Method *mlist = class_copyMethodList(native_class, &n);
    for (unsigned int i=0; i<n; ++i) {
        char buffer[1024];
        SEL selector = method_getName(mlist[i]);
        const char *selname = sel_getName(selector);
        unsigned int nparams = method_getNumberOfArguments(mlist[i]);
        
        // reserved internal objc methods to skip
        if (selname[0] == '.') continue;
        if (selname[0] == '_') continue;
        
        // check if method is a getter/setter
        NSString *methodName = [NSString stringWithUTF8String:selname];
        if (toskip[methodName]) continue;
        
        // for some strange reasons some properties are reported as methods for example UIView alpha
        if (objc_check_fake_method(native_class, c, methodName, mlist[i], toskip)) continue;
        
        // allocate xdata
        // nparams-2 because there are two implicit parameters (self and _cmd)
        method_getReturnType(mlist[i], buffer, sizeof(buffer));
        objc_bridge_func_t *m = objc_bridge_func_new(selector, NULL, nparams-2, objc_decode_type(buffer));
        
        // get and decode arguments
        for (unsigned int j=2; j<nparams; ++j) {
            method_getArgumentType(mlist[i], j, buffer, sizeof(buffer));
            m->argtype[j-2] = objc_decode_type(buffer);
        }
        
        // from exposeName:withName: to exposeName
        const char *name = objc_build_function_name(selname, buffer, sizeof(buffer));
        
        // check for special init methods
        if (string_casencmp(name, "init", 4) == 0) {
            if (nparams == 2) snprintf(buffer, sizeof(buffer), "%s", CLASS_INTERNAL_INIT_NAME);
            else snprintf(buffer, sizeof(buffer), "%s%d", CLASS_INTERNAL_INIT_NAME, nparams-2);
            name = buffer;
            m->rettype = OBJC_BRIDGE_TYPE_INIT;
        }
        
        // bind bridged function to class
        gravity_closure_t *closure = gravity_closure_new(NULL, gravity_function_new_bridged(NULL, NULL, (void *)m));
        gravity_class_bind(c, name, VALUE_FROM_OBJECT(closure));
        
        DEBUG_BRIDGE(@"Method %d/%d: %s", i, n, name);
    }
    if (mlist) free(mlist);
}

// dynamically load an objc class specified by name into vm
// class is parsed only if not yet loaded into vm
gravity_class_t *objc_class_load (gravity_vm *vm, const char *name) {
    
    // check if class is already loaded into VM
    gravity_value_t v = gravity_vm_getvalue(vm, name, (uint32_t)strlen(name));
    if (VALUE_ISA_VALID(v)) return VALUE_AS_CLASS(v);
    
    // lookup class into objc runtime (sanity check)
    Class native_class = objc_getClass(name);
    if (native_class == NULL) {
        gravity_vm_seterror(vm, "Unable to find class name %s in Objective-C runtime", name);
        return NULL;
    }
    
    // recursively scan class hierarchy
    gravity_class_t *result = NULL;
    gravity_class_t *base = NULL;
    while (native_class) {
        DEBUG_BRIDGE(@"Loading class %s", name);
        
        // create gravity class
        gravity_class_t *c = gravity_class_new_pair(vm, name, NULL, 0, 0);
        gravity_class_setxdata(c, RETAIN_OBJC_VALUE(native_class));
        
        // instance
        objc_class_scan(vm, native_class, c);
        
        // meta
        objc_class_scan(vm, objc_getMetaClass(name), c->objclass);
        
        // classes loaded from bridge are globally availables
        gravity_vm_setvalue(vm, name, VALUE_FROM_OBJECT(c));
        
        // c is overwritten in the loop, so save the first class and returns it
        if (!result) result = c;
        
        // set super class
        if (base) gravity_class_setsuper(base, c);
        
        // check for superclass
        native_class = class_getSuperclass(native_class);
        if (!native_class) break;
        
        if (native_class == [NSObject class]) break;
        
        // check if superclass is already loaded into gravity
        name = class_getName(native_class);
        gravity_value_t _v = gravity_vm_getvalue(vm, name, (uint32_t)strlen(name));
        if (VALUE_ISA_VALID(_v)) {
            DEBUG_BRIDGE(@"Loading class %s (already found in hierarchy)", name);
            // super class is already registered in gravity runtime
            // so set c super and stop loop
            gravity_class_setsuper(c, (gravity_class_t *)VALUE_AS_OBJECT(_v));
            break;
        }
        
        // save base to set super
        base = c;
    }
    
    return result;
}

static bool objc_load (gravity_vm* vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)
    
    const char *nativeName = VALUE_AS_CSTRING(args[1]);
    gravity_gc_setenabled(vm, false);
    gravity_class_t *c = objc_class_load(vm, nativeName);
    gravity_gc_setenabled(vm, true);
    if (!c) return false;
    
    RETURN_VALUE(VALUE_FROM_OBJECT(c), rindex);
    return true;
}

static bool objc_exec (gravity_vm* vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // sanity check parameters
    if (!VALUE_ISA_INSTANCE(args[1])) RETURN_ERROR("objc.exec 1st parameter must be an instance");
    if (!VALUE_ISA_STRING(args[2])) RETURN_ERROR("objc.exec 2nd parameter must be a string");
    
    // unbox parameters
    gravity_instance_t *instance = VALUE_AS_INSTANCE(args[1]);
    gravity_string_t *message = VALUE_AS_STRING(args[2]);
    
    // sanity check objc
    if (!instance->xdata) RETURN_ERROR("objc.exec 1st parameter must be an objc object");
    
    // sanity check selector
    SEL selector = NSSelectorFromString(@(message->s));
    id obj = (__bridge id)instance->xdata;
    if (![obj respondsToSelector:selector]) {
        RETURN_ERROR("objc object does not respond to given selector");
    }
    
    // retrieve method from objc runtime
    Method m = class_getInstanceMethod([obj class], selector);
    if (!m) RETURN_ERROR("objc object does not respond to given selector");
    
    // decode method
    char            buffer[1024];
    unsigned int    nparams = method_getNumberOfArguments(m);
    
    // allocate xdata
    // nparams-2 because there are two implicit parameters (self and _cmd)
    method_getReturnType(m, buffer, sizeof(buffer));
    objc_bridge_func_t *data = objc_bridge_func_new(selector, NULL, nparams-2, objc_decode_type(buffer));
    
    // get and decode arguments
    for (unsigned int j=2; j<nparams; ++j) {
        method_getArgumentType(m, j, buffer, sizeof(buffer));
        data->argtype[j-2] = objc_decode_type(buffer);
    }
    
    // execute objc selector
    args[2] = args[1];
    bool result = bridge_execute(vm, (void *)data, args[1], &args[2], nargs-2, rindex);
    
    // free temp data
    objc_bridge_func_free(data);
    
    return result;
}

// MARK: - Public functions -

void objc_register (gravity_vm *vm) {
    // register bridge delegate into VM
    gravity_delegate_t *delegate = gravity_vm_delegate(vm);
    delegate->bridge_initinstance = bridge_initinstance;
    delegate->bridge_getvalue = bridge_getvalue;
    delegate->bridge_setvalue = bridge_setvalue;
    delegate->bridge_getundef = bridge_getundef;
    delegate->bridge_setundef = bridge_setundef;
    delegate->bridge_execute = bridge_execute;
    delegate->bridge_blacken = bridge_blacken;
    delegate->bridge_equals = bridge_equals;
    delegate->bridge_string = bridge_string;
    delegate->bridge_free = bridge_free;
    delegate->bridge_clone = bridge_clone;
    
    // register objc.loadClass method
    gravity_gc_setenabled(vm, false);
    
    // register objc class
    gravity_class_t *c = gravity_class_new_pair(vm, BRIDGE_NAME, NULL, 0, 0);
    
    // register class_load
    gravity_closure_t *closure1 = gravity_closure_new(vm, gravity_function_new_internal(vm, NULL, objc_load, 0));
    gravity_class_bind(gravity_class_get_meta(c), BRIDGE_LOAD, VALUE_FROM_OBJECT(closure1));
    
    // register exec
    gravity_closure_t *closure2 = gravity_closure_new(vm, gravity_function_new_internal(vm, NULL, objc_exec, 0));
    gravity_class_bind(gravity_class_get_meta(c), BRIDGE_EXECUTE, VALUE_FROM_OBJECT(closure2));
    
    gravity_vm_setvalue(vm, BRIDGE_NAME, VALUE_FROM_OBJECT(c));
    gravity_gc_setenabled(vm, true);
}

// MARK: - xdata Management -

objc_bridge_var_t *objc_bridge_var_new (objc_bridge_type type, const char *key) {
    objc_bridge_var_t *xdata = (objc_bridge_var_t *) mem_alloc(NULL, sizeof(objc_bridge_var_t));
    
    xdata->tag = OBJC_BRIDGE_TAG_PROPERTY;
    xdata->type = type;
    xdata->key = key; // objc real property name (if different from the exposed one)
    
    return xdata;
}

void objc_bridge_var_set_type (objc_bridge_var_t *xdata, objc_bridge_type type) {
    xdata->type = type;
}

void objc_bridge_var_free (objc_bridge_var_t *xdata) {
    if (xdata->key) mem_free(xdata->key);
    mem_free(xdata);
}

#pragma mark -

objc_bridge_func_t *objc_bridge_func_new (SEL selector, const char *name, uint16_t nargs, objc_bridge_type rettype) {
    objc_bridge_func_t *xdata = mem_alloc(NULL, sizeof(objc_bridge_func_t));
    
    xdata->tag = OBJC_BRIDGE_TAG_METHOD;
    xdata->selector = selector;
    xdata->nargs = nargs;
    xdata->rettype = rettype;
    xdata->argtype = NULL;
    xdata->argvalue = NULL;
    xdata->name = (name) ? string_dup(name) : NULL;
    
    if (nargs) xdata->argtype = (objc_bridge_type *)calloc(nargs, sizeof(objc_bridge_type));
    return xdata;
}

void objc_bridge_func_set_name (objc_bridge_func_t *xdata, const char *name) {
    xdata->name = (name) ? string_dup(name) : NULL;
}

const char *objc_bridge_get_exposed_name (objc_bridge_func_t *xdata) {
    if (xdata->rettype == OBJC_BRIDGE_TYPE_INIT) return "init";
    if (xdata->name) return xdata->name;
    if (xdata->selector) return NSStringFromSelector(xdata->selector).UTF8String;
    return "N/A";
}

void objc_bridge_func_set_rettype (objc_bridge_func_t *xdata, objc_bridge_type rettype) {
    xdata->rettype = rettype;
}

void objc_bridge_func_set_argtype (objc_bridge_func_t *xdata, objc_bridge_type type, uint8_t index) {
    assert(index < xdata->nargs);
    xdata->argtype[index] = type;
}

void objc_bridge_func_set_argvalue (objc_bridge_func_t *xdata, id value, uint8_t index) {
    assert(index < xdata->nargs);
    if (!xdata->argvalue) xdata->argvalue = (void **)calloc(xdata->nargs, sizeof(void *));
    xdata->argvalue[index] = (void *)CFBridgingRetain(value);
}

void objc_bridge_func_free (objc_bridge_func_t *xdata) {
    if (xdata->invocation) CFBridgingRelease((CFTypeRef)xdata->invocation);
    if (xdata->argtype) free(xdata->argtype);
    if (xdata->name) mem_free(xdata->name);
    if (xdata->argvalue) {
        for (uint32_t i=0; i<xdata->nargs; ++i) {
            if (xdata->argvalue[i]) CFBridgingRelease((CFTypeRef)xdata->argvalue[i]);
        }
        free(xdata->argvalue);
    }
    mem_free(xdata);
}

// MARK: - Gravity => ObjC -

static inline id convert_gravity2id (gravity_vm *vm, gravity_value_t value) {
    if (VALUE_ISA_INT(value)) return @(value.n);
    if (VALUE_ISA_FLOAT(value)) return @(value.f);
    if (VALUE_ISA_BOOL(value)) return [NSNumber numberWithBool:(value.n)];
    if ((VALUE_ISA_NULL(value)) || (VALUE_ISA_UNDEFINED(value))) return nil;
    if (VALUE_ISA_STRING(value)) return [NSString stringWithUTF8String:VALUE_AS_CSTRING(value)];
    if (VALUE_ISA_RANGE(value)) return convert_gravity2nsrangevalue(vm, value);
    if (VALUE_ISA_MAP(value)) return convert_gravity2nsdictionary(vm, value);
    if (VALUE_ISA_LIST(value)) return convert_gravity2nsarray(vm, value);
    
    if (!VALUE_ISA_INSTANCE(value)) return nil;
    return (__bridge id)gravity_value_xdata(value);
}

static inline id convert_gravity2type (gravity_vm *vm, gravity_value_t value, objc_bridge_type type) {
    id obj = convert_gravity2id(vm, value);
    // can be nil
    return obj;
}

static inline NSRange convert_gravity2nsrange (gravity_vm *vm, gravity_value_t value) {
    #pragma unused(vm)
    if (VALUE_ISA_RANGE(value)) {
        gravity_range_t *r = VALUE_AS_RANGE(value);
        return NSMakeRange((NSUInteger)r->from, (NSUInteger)r->to);
    }
    return NSMakeRange(0, 0);
}

static inline NSValue *convert_gravity2nsrangevalue (gravity_vm *vm, gravity_value_t value) {
    NSRange range = convert_gravity2nsrange(vm, value);
    return [NSValue valueWithRange:NSMakeRange(range.location, range.length)];
}

static void convert_nsdictionary_callback (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value, void *data1, void *data2) {
    #pragma unused (hashtable)
    NSMutableDictionary *d = (__bridge NSMutableDictionary*)data1;
    gravity_vm *vm = (gravity_vm *)data2;
    d[@(VALUE_AS_CSTRING(key))] = convert_gravity2id(vm, value);
}

static inline NSArray* convert_gravity2nsarray (gravity_vm *vm, gravity_value_t v) {
    #pragma unused(vm)
    
    #if RETURN_NIL_ON_NULL
    if (VALUE_ISA_NULL(v) || VALUE_ISA_UNDEFINED(v)) return nil;
    #endif
    
    NSMutableArray *r = [NSMutableArray array];
    if (VALUE_ISA_LIST(v)) {
        gravity_list_t *list = VALUE_AS_LIST(v);
        for (uint32_t i=0; i<marray_size(list->array); ++i) {
            id obj = convert_gravity2id(vm, marray_get(list->array, i));
            if (obj) [r addObject:obj];
        }
    }
    return r;
}

static inline NSDictionary *convert_gravity2nsdictionary (gravity_vm *vm, gravity_value_t v) {
    #pragma unused(vm)
    
    #if RETURN_NIL_ON_NULL
    if (VALUE_ISA_NULL(v) || VALUE_ISA_UNDEFINED(v)) return nil;
    #endif
    
    NSMutableDictionary *d = [NSMutableDictionary dictionary];
    if (VALUE_ISA_MAP(v)) {
        gravity_map_t *map = VALUE_AS_MAP(v);
        gravity_hash_iterate2(map->hash, convert_nsdictionary_callback, (__bridge void *)d, (void*)vm);
    }
    return d;
}

static inline id convert_gravity2obj (gravity_vm *vm, gravity_value_t value, objc_bridge_type type) {
    #if RETURN_NIL_ON_NULL
    if (VALUE_ISA_NULL(value) || VALUE_ISA_UNDEFINED(value)) {
        // STRUCT BASED VALUE
        return nil;
    }
    #endif
    
    switch (type) {
        case OBJC_BRIDGE_TYPE_UNKNOWN:
        case OBJC_BRIDGE_TYPE_VOID:
        case OBJC_BRIDGE_TYPE_VPTR:
        case OBJC_BRIDGE_TYPE_CPTR:
        case OBJC_BRIDGE_TYPE_CLASS:
        case OBJC_BRIDGE_TYPE_SEL: {
            NSLog(@"Unsupported conversion in gravity2obj");
            return nil;
        }
            
        case OBJC_BRIDGE_TYPE_CLOSURE: {
            return nil;
        }
        
        case OBJC_BRIDGE_TYPE_BOOL: {
            gravity_value_t v = convert_value2bool(vm, value);
            SANITY_CHECK_VALUE(v);
            return @((BOOL)v.n);
        };
            
        case OBJC_BRIDGE_TYPE_INT8: {
            gravity_value_t v = convert_value2int(vm, value);
            SANITY_CHECK_VALUE(v);
            return @((int8_t)v.n);
        };
            
        case OBJC_BRIDGE_TYPE_INT16: {
            gravity_value_t v = convert_value2int(vm, value);
            SANITY_CHECK_VALUE(v);
            return @((int16_t)v.n);
        };
            
        case OBJC_BRIDGE_TYPE_INT32: {
            gravity_value_t v = convert_value2int(vm, value);
            SANITY_CHECK_VALUE(v);
            return @((int32_t)v.n);
        };
            
        case OBJC_BRIDGE_TYPE_INT64: {
            gravity_value_t v = convert_value2int(vm, value);
            SANITY_CHECK_VALUE(v);
            return @((int64_t)v.n);
        };
        
        case OBJC_BRIDGE_TYPE_UINT8: {
            gravity_value_t v = convert_value2int(vm, value);
            SANITY_CHECK_VALUE(v);
            return @((uint8_t)v.n);
        };
            
        case OBJC_BRIDGE_TYPE_UINT16: {
            gravity_value_t v = convert_value2int(vm, value);
            SANITY_CHECK_VALUE(v);
            return @((uint16_t)v.n);
        };
            
        case OBJC_BRIDGE_TYPE_UINT32: {
            gravity_value_t v = convert_value2int(vm, value);
            SANITY_CHECK_VALUE(v);
            return @((uint32_t)v.n);
        };
            
        case OBJC_BRIDGE_TYPE_UINT64: {
            gravity_value_t v = convert_value2int(vm, value);
            SANITY_CHECK_VALUE(v);
            return @((uint64_t)v.n);
        };
            
        case OBJC_BRIDGE_TYPE_FLOAT: {
            gravity_value_t v = convert_value2float(vm, value);
            SANITY_CHECK_VALUE(v);
            return @((float)v.f);
        };
        
        case OBJC_BRIDGE_TYPE_LDOUBLE:
        case OBJC_BRIDGE_TYPE_DOUBLE: {
            gravity_value_t v = convert_value2float(vm, value);
            SANITY_CHECK_VALUE(v);
            return @((double)v.f);
        };
        
        case OBJC_BRIDGE_TYPE_NSINTEGER: {
            gravity_value_t v = convert_value2int(vm, value);
            SANITY_CHECK_VALUE(v);
            return @((NSInteger)v.n);
        };
        case OBJC_BRIDGE_TYPE_NSUINTEGER: {
            gravity_value_t v = convert_value2int(vm, value);
            SANITY_CHECK_VALUE(v);
            return @((NSUInteger)v.n);
        };
            
        case OBJC_BRIDGE_TYPE_NSNUMBER: {
            if (VALUE_ISA_INT(value)) return @(value.n);
            else if (VALUE_ISA_FLOAT(value)) return @(value.f);
            
            gravity_value_t v = convert_value2int(vm, value);
            SANITY_CHECK_VALUE(v);
            return @(v.n);
        };
            
        case OBJC_BRIDGE_TYPE_NSSTRING: {
            gravity_value_t v = convert_value2string(vm, value);
            SANITY_CHECK_VALUE(v);
            return [NSString stringWithUTF8String:VALUE_AS_CSTRING(v)];
        }
            
        case OBJC_BRIDGE_TYPE_NSARRAY:
            return convert_gravity2nsarray(vm, value);
            
        case OBJC_BRIDGE_TYPE_NSDICTIONARY:
            return convert_gravity2nsdictionary(vm, value);
            
        case OBJC_BRIDGE_TYPE_RANGE:
            return convert_gravity2nsrangevalue(vm, value);
        
        case OBJC_BRIDGE_TYPE_POINT:
        case OBJC_BRIDGE_TYPE_RECT:
        case OBJC_BRIDGE_TYPE_SIZE:
        case OBJC_BRIDGE_TYPE_OFFSET:
        case OBJC_BRIDGE_TYPE_EDGEINSETS:
        case OBJC_BRIDGE_TYPE_FONT:
        case OBJC_BRIDGE_TYPE_SOUND:
        case OBJC_BRIDGE_TYPE_MOVIE:
        case OBJC_BRIDGE_TYPE_GRADIENT:
        case OBJC_BRIDGE_TYPE_NSDATE:
        case OBJC_BRIDGE_TYPE_NSDATA:
        case OBJC_BRIDGE_TYPE_IMAGE:
        case OBJC_BRIDGE_TYPE_COLOR:
            return convert_gravity2type(vm, value, type);
        
        case OBJC_BRIDGE_TYPE_GRAVITY:
        case OBJC_BRIDGE_TYPE_INIT:
        case OBJC_BRIDGE_TYPE_ID: return convert_gravity2id(vm, value);
            
        case OBJC_BRIDGE_TYPE_USER:
            return convert_gravity2type(vm, value, OBJC_BRIDGE_TYPE_USER);
    }
    
    return convert_gravity2type(vm, value, type);
}

// MARK: - ObjC => Gravity -

static inline gravity_value_t convert_nsnumber2gravity (gravity_vm *vm, NSNumber *value) {
    const char *internal = [value objCType];
    switch (internal[0]) {
        case 'c': return bridge_objc2gravity(vm, value, OBJC_BRIDGE_TYPE_BOOL);
        case 'i': return bridge_objc2gravity(vm, value, OBJC_BRIDGE_TYPE_INT32);
        case 's': return bridge_objc2gravity(vm, value, OBJC_BRIDGE_TYPE_INT16);
        case 'l': return bridge_objc2gravity(vm, value, OBJC_BRIDGE_TYPE_INT32);
        case 'q': return bridge_objc2gravity(vm, value, OBJC_BRIDGE_TYPE_INT64);
        case 'C': return bridge_objc2gravity(vm, value, OBJC_BRIDGE_TYPE_UINT8);
        case 'I': return bridge_objc2gravity(vm, value, OBJC_BRIDGE_TYPE_UINT32);
        case 'S': return bridge_objc2gravity(vm, value, OBJC_BRIDGE_TYPE_UINT16);
        case 'L': return bridge_objc2gravity(vm, value, OBJC_BRIDGE_TYPE_UINT32);
        case 'Q': return bridge_objc2gravity(vm, value, OBJC_BRIDGE_TYPE_UINT64);
        case 'f': return bridge_objc2gravity(vm, value, OBJC_BRIDGE_TYPE_FLOAT);
        case 'd': return bridge_objc2gravity(vm, value, OBJC_BRIDGE_TYPE_DOUBLE);
        case 'B': return bridge_objc2gravity(vm, value, OBJC_BRIDGE_TYPE_BOOL);
    }
    return VALUE_FROM_NULL;
}

static inline gravity_value_t convert_nsstring2gravity (gravity_vm *vm, NSString *value) {
    return VALUE_FROM_STRING(vm, value.UTF8String, (uint32_t)[value lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
}

static inline gravity_value_t convert_creo2gravity (gravity_vm *vm, id value, objc_bridge_type type, BOOL value_retained) {
    #ifdef CREO_PROJECT
    if ([value respondsToSelector:@selector(runtimeInstance)]) {
        // if the creo objects already has an associated gravity instance, return it
        CREORuntimeInstance *runtimeInstance = [(id<CREORuntimeInstanceProtocol>)value runtimeInstance];
        gravity_instance_t *instance = (gravity_instance_t *)runtimeInstance.instance;
        if (instance) return VALUE_FROM_OBJECT(instance);
    }
    
    if ((!value) || ([value isKindOfClass:[NSNull class]])) return VALUE_FROM_NULL;
    if ([value isKindOfClass:[NSNumber class]]) return convert_nsnumber2gravity(vm, value);
    if ([value isKindOfClass:[NSString class]]) return convert_nsstring2gravity(vm, value);
    
    id<CREORuntimeDelegate> delegate = (__bridge id<CREORuntimeDelegate>)(gravity_vm_getdata(vm));
    Class c = (type != OBJC_BRIDGE_TYPE_UNKNOWN) ? [delegate classByTag:type] : nil;
    if (!c) c = [value class];
    
    NSString *name = [delegate classExposedNameByRealName:NSStringFromClass(c)];
    gravity_value_t v = gravity_vm_getvalue(vm, name.UTF8String, (uint32_t)name.length);
    if (!VALUE_ISA_CLASS(v)) return VALUE_FROM_NULL;
    
    gravity_class_t *c2 = VALUE_AS_CLASS(v);
    gravity_instance_t *instance = gravity_instance_new(vm, c2);
    gravity_instance_setxdata(instance, (value_retained) ? (__bridge void *)value : RETAIN_OBJC_VALUE(value));
    set_runtime_instance(vm, value, instance);
    return VALUE_FROM_OBJECT(instance);
    #else
    #pragma unused (vm, value, type)
    return VALUE_FROM_NULL;
    #endif
}

static inline gravity_value_t convert_nsarray2gravity (gravity_vm *vm, NSArray *r) {
    NSUInteger count = r.count;
    gravity_list_t *list = gravity_list_new(vm, (uint32_t)count);
    if (!list) return VALUE_FROM_NULL;
    
    for (id obj in r) {
        gravity_value_t v = convert_id2gravity(vm, obj);
        marray_push(gravity_value_t, list->array, v);
    }
    
    return VALUE_FROM_OBJECT(list);
}

static inline gravity_value_t convert_nsdictionary2gravity (gravity_vm *vm, NSDictionary *d) {
    NSUInteger count = d.allKeys.count;
    gravity_map_t *map = gravity_map_new(vm, (uint32_t)count);
    
    for (NSString *key in d.allKeys) {
        id obj = d[key];
        gravity_value_t v = bridge_objc2gravity(vm, obj, OBJC_BRIDGE_TYPE_ID);
        gravity_value_t k = VALUE_FROM_STRING(vm, key.UTF8String, (uint32_t)[key lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
        gravity_hash_insert(map->hash, k, v);
    }
    
    return VALUE_FROM_OBJECT(map);
}

static inline gravity_value_t convert_nsvalue2gravity (gravity_vm *vm, id obj, objc_bridge_type type) {
    // called ONLY when I am sure that obj isKindOfClass NSValue
    NSValue *value = (NSValue*)obj;
    
    if (((type == OBJC_BRIDGE_TYPE_UNKNOWN) || (type == OBJC_BRIDGE_TYPE_RANGE)) && (strcmp(value.objCType, @encode(NSRange)) == 0)) {
        NSRange v = [value rangeValue];
        return VALUE_FROM_OBJECT(gravity_range_new(vm, v.location, v.length, true));
    }
    
    #ifdef CREO_PROJECT
    Class c = nil;
    
    if (((type == OBJC_BRIDGE_TYPE_UNKNOWN) || (type == OBJC_BRIDGE_TYPE_POINT)) && (strcmp(value.objCType, @encode(CGPoint)) == 0)) c = CREOPoint.class;
    else if (((type == OBJC_BRIDGE_TYPE_UNKNOWN) || (type == OBJC_BRIDGE_TYPE_RECT)) && (strcmp(value.objCType, @encode(CGRect)) == 0)) c = CREORect.class;
    else if (((type == OBJC_BRIDGE_TYPE_UNKNOWN) || (type == OBJC_BRIDGE_TYPE_SIZE)) && (strcmp(value.objCType, @encode(CGSize)) == 0)) c = CREOSize.class;
    else if (((type == OBJC_BRIDGE_TYPE_UNKNOWN) || (type == OBJC_BRIDGE_TYPE_OFFSET)) && (strcmp(value.objCType, @encode(UIOffset)) == 0)) c = CREOOffset.class;
    else if (((type == OBJC_BRIDGE_TYPE_UNKNOWN) || (type == OBJC_BRIDGE_TYPE_EDGEINSETS)) && (strcmp(value.objCType, @encode(UIEdgeInsets)) == 0)) c = CREOEdgeInsets.class;
    
    if (c) {
        CREOStruct *creoObj = (CREOStruct *)[[c alloc] init];
        [creoObj setValue:value];
        return convert_creo2gravity(vm, creoObj, type, NO);
    }
    #endif
    
    return VALUE_FROM_NULL;
}

static inline gravity_value_t convert_id2gravity (gravity_vm *vm, id value) {
    // not sure if its a good idea to not trigger a crash in this case
    if (!value) return VALUE_FROM_NULL;
    
    // NSNumber case
    if ([value isKindOfClass:[NSNumber class]]) {
        return convert_nsnumber2gravity(vm, value);
    }
    
    // NSString case
    if ([value isKindOfClass:[NSString class]]) {
        return convert_nsstring2gravity(vm, value);
    }
    
    // NSArray case
    if ([value isKindOfClass:[NSArray class]]) {
        return convert_nsarray2gravity(vm, value);
    }
    
    // NSDictionary case
    if ([value isKindOfClass:[NSDictionary class]]) {
        return convert_nsdictionary2gravity(vm, value);
    }
    
    // NSValue case
    if ([value isKindOfClass:[NSValue class]]) {
        return convert_nsvalue2gravity(vm, value, OBJC_BRIDGE_TYPE_UNKNOWN);
    }
    
    // NSNull case
    if ([value isKindOfClass:[NSNull class]]) {
        return VALUE_FROM_NULL;
    }

    return convert_creo2gravity(vm, value, OBJC_BRIDGE_TYPE_UNKNOWN, NO);
}

// MARK: - Bridge Utils -

id bridge_gravity2objc (gravity_vm *vm, gravity_value_t value, objc_bridge_type type) {
    // must be protected because it is used by Creo in every event
    gravity_gc_setenabled(vm, false);
    id v = convert_gravity2obj(vm, value, type);
    gravity_gc_setenabled(vm, true);
    return v;
}

gravity_instance_t *bridge_instance_byclassname (gravity_vm *vm, id value, const char* name, uint32_t length) {
    gravity_value_t v = gravity_vm_getvalue(vm, name, length);
    if (!VALUE_ISA_CLASS(v)) return NULL;
    
    gravity_class_t *c2 = VALUE_AS_CLASS(v);
    gravity_instance_t *instance = gravity_instance_new(vm, c2);
    gravity_instance_setxdata(instance, RETAIN_OBJC_VALUE(value));
    return instance;
}

static gravity_value_t bridge_objc2gravity_retain_flag (gravity_vm *vm, id obj, objc_bridge_type type, BOOL value_retained) {
    // sanity check
    if (!obj) return VALUE_FROM_NULL;
    
    @try {
        switch (type) {
            case OBJC_BRIDGE_TYPE_UNKNOWN: {
                return convert_creo2gravity(vm, obj, OBJC_BRIDGE_TYPE_UNKNOWN, value_retained);
            }
                
            case OBJC_BRIDGE_TYPE_SEL:
            case OBJC_BRIDGE_TYPE_VPTR:
            case OBJC_BRIDGE_TYPE_CPTR:
            case OBJC_BRIDGE_TYPE_VOID:
            case OBJC_BRIDGE_TYPE_CLASS: {
                return VALUE_FROM_NULL;
            }
                
            case OBJC_BRIDGE_TYPE_INIT:
            case OBJC_BRIDGE_TYPE_ID:
            case OBJC_BRIDGE_TYPE_USER:
            case OBJC_BRIDGE_TYPE_GRAVITY: {
                return convert_id2gravity(vm, obj);
            }
                
            case OBJC_BRIDGE_TYPE_RANGE: {
                return convert_nsvalue2gravity(vm, obj, type);
            }
                
            case OBJC_BRIDGE_TYPE_POINT:
            case OBJC_BRIDGE_TYPE_RECT:
            case OBJC_BRIDGE_TYPE_SIZE:
            case OBJC_BRIDGE_TYPE_OFFSET:
            case OBJC_BRIDGE_TYPE_EDGEINSETS: {
                if ([obj isKindOfClass:[NSValue class]]) return convert_nsvalue2gravity(vm, obj, type);
                return convert_creo2gravity(vm, obj, type, value_retained);
            }
                
            case OBJC_BRIDGE_TYPE_NSDATE:
            case OBJC_BRIDGE_TYPE_NSDATA:
            case OBJC_BRIDGE_TYPE_IMAGE:
            case OBJC_BRIDGE_TYPE_COLOR:
            case OBJC_BRIDGE_TYPE_GRADIENT:
            case OBJC_BRIDGE_TYPE_MOVIE:
            case OBJC_BRIDGE_TYPE_SOUND:
            case OBJC_BRIDGE_TYPE_FONT: {
                return convert_creo2gravity(vm, obj, type, value_retained);
            }
                
            case OBJC_BRIDGE_TYPE_NSARRAY: {
                if ([obj isKindOfClass:[NSArray class]])
                    return convert_nsarray2gravity(vm, obj);
                else
                    return convert_nsarray2gravity(vm, @[obj]);
            }
                
            case OBJC_BRIDGE_TYPE_NSDICTIONARY: {
                if ([obj isKindOfClass:[NSDictionary class]])
                    return convert_nsdictionary2gravity(vm, obj);
                else
                    return convert_creo2gravity(vm, obj, type, value_retained);
            }
                
            case OBJC_BRIDGE_TYPE_NSNUMBER: {
                if ([obj isKindOfClass:[NSNumber class]])
                    return convert_nsnumber2gravity(vm, obj);
                else
                    return convert_creo2gravity(vm, obj, type, value_retained);
            }
                
            case OBJC_BRIDGE_TYPE_INT8: {
                int8_t value = [obj charValue];
                return VALUE_FROM_INT((gravity_int_t)value);
            }
                
            case OBJC_BRIDGE_TYPE_INT16: {
                int16_t value = [obj shortValue];
                return VALUE_FROM_INT((gravity_int_t)value);
            }
                
            case OBJC_BRIDGE_TYPE_INT32: {
                int32_t value = (int32_t)[obj longValue];
                return VALUE_FROM_INT((gravity_int_t)value);
            }
                
            case OBJC_BRIDGE_TYPE_INT64: {
                int64_t value = [obj longLongValue];
                return VALUE_FROM_INT((gravity_int_t)value);
            }
                
            case OBJC_BRIDGE_TYPE_UINT8: {
                uint8_t value = [obj unsignedCharValue];
                return VALUE_FROM_INT((gravity_int_t)value);
            }
                
            case OBJC_BRIDGE_TYPE_UINT16: {
                uint16_t value = [obj unsignedShortValue];
                return VALUE_FROM_INT((gravity_int_t)value);
            }
                
            case OBJC_BRIDGE_TYPE_UINT32: {
                uint32_t value = (uint32_t)[obj unsignedLongValue];
                return VALUE_FROM_INT((gravity_int_t)value);
            }
                
            case OBJC_BRIDGE_TYPE_UINT64: {
                uint64_t value = [obj unsignedLongLongValue];
                return VALUE_FROM_INT((gravity_int_t)value);
            }
                
            case OBJC_BRIDGE_TYPE_FLOAT: {
                float value = [obj floatValue];
                return VALUE_FROM_FLOAT((gravity_float_t)value);
            }
                
            case OBJC_BRIDGE_TYPE_LDOUBLE:
            case OBJC_BRIDGE_TYPE_DOUBLE: {
                double value = [obj doubleValue];
                return VALUE_FROM_FLOAT((gravity_float_t)value);
            }
                
            case OBJC_BRIDGE_TYPE_BOOL: {
                BOOL value = [obj boolValue];
                return VALUE_FROM_BOOL(value);
            }
                
            case OBJC_BRIDGE_TYPE_NSINTEGER: {
                NSInteger value = [obj integerValue];
                return VALUE_FROM_INT((gravity_int_t)value);
            }
                
            case OBJC_BRIDGE_TYPE_NSUINTEGER: {
                NSUInteger value = [obj unsignedIntegerValue];
                return VALUE_FROM_INT((gravity_int_t)value);
            }
                
            case OBJC_BRIDGE_TYPE_NSSTRING: {
                return VALUE_FROM_STRING(vm, ((NSString*)obj).UTF8String, (uint32_t)[(NSString*)obj lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
            }
                
            default: {
                return convert_creo2gravity(vm, obj, type, value_retained);
            }
        }
    } @catch (NSException *exception) {
        NSLog(@"bridge_objc2gravity %@ (%@ %d)", exception.reason, obj, type);
        return convert_creo2gravity(vm, obj, type, value_retained);
    }
    
    return VALUE_FROM_NULL;
}

gravity_value_t bridge_objc2gravity (gravity_vm *vm, id obj, objc_bridge_type type) {
    // must be protected because it is used by Creo in every event
    gravity_gc_setenabled(vm, false);
    gravity_value_t v = bridge_objc2gravity_retain_flag(vm, obj, type, NO);
    gravity_gc_setenabled(vm, true);
    return v;
}

static objc_bridge_var_t *bridge_property (gravity_vm *vm, gravity_class_t *c, const char *key) {
    #pragma unused(vm)
    
    STATICVALUE_FROM_STRING(k, key, strlen(key));
    gravity_object_t *obj = gravity_class_lookup(c, k);
    if (!obj) return NULL;
    
    if (!OBJECT_ISA_CLOSURE(obj)) return NULL;
    gravity_closure_t *closure = (gravity_closure_t*)obj;
    if (closure->f->tag != EXEC_TYPE_SPECIAL) return NULL;
    if (closure->f->index != GRAVITY_BRIDGE_INDEX) return NULL;
    closure = (closure->f->special[0]) ? closure->f->special[0] : closure->f->special[1];
    if (!closure || (!closure->f)) return NULL;
    if (!closure->f->xdata) return NULL;
    
    return (objc_bridge_var_t *)closure->f->xdata;
}

objc_bridge_type bridge_property_type (gravity_vm *vm, gravity_class_t *c, const char *key) {
    objc_bridge_var_t *property = bridge_property(vm, c, key);
    if (!property) return OBJC_BRIDGE_TYPE_UNKNOWN;
    return property->type;
}

const char *bridge_property_name (gravity_vm *vm, gravity_class_t *c, const char *key) {
    objc_bridge_var_t *property = bridge_property(vm, c, key);
    if (!property) return key;
    if (!property->key) return key;
    return property->key;
}

// MARK: - Delegate -

bool bridge_initinstance (gravity_vm *vm, void *xdata, gravity_value_t ctx, gravity_instance_t *instance, gravity_value_t args[], int16_t nargs) {
    gravity_class_t *class = instance->objclass;
    Class c = (__bridge Class)(class->xdata);
    
    // special case to force to use xdata directly
    if (VALUE_ISA_NULL(ctx) && args == NULL && nargs == 1) c = (__bridge Class)xdata;
    
    id obj = [c alloc];
    gravity_instance_setxdata(instance, RETAIN_OBJC_VALUE(obj));
    
    if (nargs == 1) {
        // no arguments so just execute the init (obj2 can be different than obj, for example the NSDate init)
        id obj2 = [obj init];
        if (!obj2) RETURN_ERROR("Unable to initialize object.");
        
        #ifdef CREO_PROJECT
        set_runtime_instance(vm, obj2, instance);
        #endif
        
        if (obj != obj2) {
            // note1:
            // when the two objects are different (alloc != init) it means that in init there is a code like
            // self = something
            // and this line automatically send a release message to the original object so an explicit release
            // is not needed
            // RELEASE_OBJC_VALUE(obj);
            gravity_instance_setxdata(instance, RETAIN_OBJC_VALUE(obj2));
        }
        RETURN_NOVALUE();
    }
    
    // there are more arguments so execute the init function
    void *saved = gravity_vm_getdata(vm);
    args[0] = VALUE_FROM_OBJECT(instance);
    if (!bridge_execute(vm, xdata, ctx, args, nargs, GRAVITY_DATA_REGISTER)) {
        gravity_instance_setxdata(instance, NULL);
        return false;
    }
    
    // obj2 can be different from obj if the init method returns a different object from the previously allocated one
    id obj2 = (__bridge id)(gravity_vm_getdata(vm));
    gravity_vm_setdata(vm, saved);
    if (!obj2) {
        gravity_instance_setxdata(instance, NULL);
        NSString *name = NULL;
        RETURN_ERROR("Unable to initialize object of type %s.", (name) ? name.UTF8String : class->identifier);
    }
    
    #if GRAVITY_BRIDGE_DEBUG_MEMORY
    NSLog(@"Created instance %p (%@)", obj2, NSStringFromClass([obj2 class]));
    #endif
    
    if (obj != obj2) {
        // see note1 above
        // RELEASE_OBJC_VALUE(obj);
        // obj2 has already been retained in the bridge_execute
        gravity_instance_setxdata(instance, (__bridge void *)(obj2));
    }
    RETURN_NOVALUE();
}

bool bridge_setvalue (gravity_vm *vm, void *xdata, gravity_value_t target, const char *key, gravity_value_t value) {
    DEBUG_BRIDGE(@"bridge_setvalue %s", key);
    
    // obtain property type and optional key from class xdata
    objc_bridge_var_t *property = (objc_bridge_var_t *)xdata;
    if (property->key) key = property->key;
    
    id objcValue = convert_gravity2obj(vm, value, property->type);
    
    // get objc obj from target xdata
    id obj = (__bridge id)gravity_value_xdata(target);
    if (!obj) return false;
    
    @try {
        [obj setValue:objcValue forKey:@(key)];
    }
    @catch (NSException * e) {
        gravity_vm_seterror(vm, "An error occurred while writing key %s (%s).", key, [[e reason] UTF8String]);
        return false;
    }
    
    RETURN_NOVALUE();
}

bool bridge_getvalue (gravity_vm *vm, void *xdata, gravity_value_t target, const char *key, uint32_t rindex) {
    DEBUG_BRIDGE(@"bridge_getvalue %s", key);
    
    // obtain property type and optional key from class xdata
    objc_bridge_var_t *property = (objc_bridge_var_t *)xdata;
    if (property->key) key = property->key;
    
    // get objc obj from target xdata
    id obj = (__bridge id)gravity_value_xdata(target);
    if (!obj) return false;
    
    id result;
    @try {
        result = [obj valueForKey:@(key)];
    }
    @catch (NSException * e) {
        gravity_vm_seterror(vm, "An error occurred while reading key %s (%s).", key, [[e reason] UTF8String]);
        return false;
    }
    
    gravity_value_t value = bridge_objc2gravity(vm, result, property->type);
    RETURN_VALUE(value, rindex);
}

bool bridge_setundef (gravity_vm *vm, void *xdata, gravity_value_t target, const char *key, gravity_value_t value) {
    #pragma unused(vm, xdata, target, key, value)
    RETURN_NOVALUE();
}

bool bridge_getundef (gravity_vm *vm, void *xdata, gravity_value_t target, const char *key, uint32_t rindex) {
    #pragma unused(vm, xdata, target, key, rindex)
    RETURN_NOVALUE();
}

bool bridge_execute (gravity_vm *vm, void *data, gravity_value_t ctx, gravity_value_t args[], int16_t nargs, uint32_t rindex) {
    gravity_value_t        target = args[0];
    objc_bridge_func_t    *xdata = (objc_bridge_func_t *)data;
    id                    callee = (__bridge id)gravity_value_xdata(target);
    NSMutableArray        *arguments = [NSMutableArray arrayWithCapacity:nargs];
    
    // internal debug var
    // struct objc_bridge_func_s *ddata = (struct objc_bridge_func_s *)data;
    
    if (!callee || !xdata) {
        // not an instance nor a class... so a runtime error I guess
        RETURN_ERROR("Unable to process bridge request because target is not an instance nor a class.");
    }
    
    NSInvocation *invocation = (xdata->invocation) ? (__bridge NSInvocation *)(xdata->invocation) : nil;
    if (!invocation) {
        NSMethodSignature *signature = [callee methodSignatureForSelector:xdata->selector];
        if (!signature) {
            const char *name = objc_bridge_get_exposed_name(xdata);
            const char *s = NSStringFromSelector(xdata->selector).UTF8String;
            RETURN_ERROR("Unable to process bridge request because signature for method %s (selector %s) cannot be build.", name, s);
        }
        
        xdata->retlength = [signature methodReturnLength];
        invocation = [NSInvocation invocationWithMethodSignature:signature];
        [invocation setSelector:xdata->selector];                    // hidden _cmd parameter
        xdata->invocation = (void *)CFBridgingRetain(invocation);    // cache invocation
        if (xdata->rettype > OBJC_BRIDGE_TYPE_USER) xdata->rettype = OBJC_BRIDGE_TYPE_USER;
    }
    
    if (!invocation) {
        RETURN_ERROR("Unable to process bridge request because invocation cannot be build.");
    }
    
    // nargs is at least ALWAYS 1 because of the implicit target argument
    // last check added for default values
    if ((nargs>1) && (nargs-1 < xdata->nargs) && (!xdata->argvalue)) {
        const char *name = objc_bridge_get_exposed_name(xdata);
        RETURN_ERROR("Unable to call %s because of missing parameters (passed %d, required %d)", name, nargs-1, xdata->nargs);
    }
    
    #if ENABLE_RUNTIME_CONTEXT
    if ([callee respondsToSelector:@selector(runtimeInstance)]) {
        CREORuntimeInstance *runtimeInstance = [callee runtimeInstance];
        if (VALUE_ISA_INSTANCE(ctx)) runtimeInstance.context = (void *)VALUE_AS_INSTANCE(ctx);
        else if (VALUE_ISA_CLASS(ctx)) runtimeInstance.context = (void *)VALUE_AS_CLASS(ctx);
        else runtimeInstance.context = NULL;
    }
    #endif
    
    // setup parameters (i starts from 2 due to implicit arguments)
    for (uint16_t i=0, j=1, k=2; i<xdata->nargs; ++i, ++j, ++k) {
        gravity_value_t gravity_value = (j<nargs) ? args[j] : VALUE_FROM_NULL;
        BOOL is_default_value = NO;
        
        // check for special default value case
        if (((j>=nargs) || VALUE_ISA_UNDEFINED(gravity_value)) && xdata->argvalue) {
            
            // sanity check
            if (!xdata->argvalue[i]) {
                const char *name = objc_bridge_get_exposed_name(xdata);
                RETURN_ERROR("Unable to call %s because of missing parameters (passed %d, required %d)", name, nargs-1, xdata->nargs);
            }
            
            // unbox default value
            if ((__bridge id)xdata->argvalue[i] == (id)[NSNull null]) gravity_value = VALUE_FROM_NULL;
            else gravity_value = bridge_objc2gravity_retain_flag(vm, (__bridge id)xdata->argvalue[i], xdata->argtype[i], YES);
            
            is_default_value = YES;
        }
        
        // convert argument
        switch (xdata->argtype[i]) {
            
            case OBJC_BRIDGE_TYPE_INIT:
            case OBJC_BRIDGE_TYPE_UNKNOWN:
            case OBJC_BRIDGE_TYPE_CPTR:
            case OBJC_BRIDGE_TYPE_CLASS:
            case OBJC_BRIDGE_TYPE_SEL:
            case OBJC_BRIDGE_TYPE_VOID:
            case OBJC_BRIDGE_TYPE_LDOUBLE:
                assert(0);
                
            case OBJC_BRIDGE_TYPE_CLOSURE: {
                // extra check for argumennt to be a real closure
                gravity_closure_t *closure;
                if (VALUE_ISA_NULL(gravity_value)) closure = NULL;
                else if (VALUE_ISA_CLOSURE(gravity_value)) closure = VALUE_AS_CLOSURE(gravity_value);
                else RETURN_ERROR("Unable to convert parameter %d to Closure (in %s).", k-1, objc_bridge_get_exposed_name(xdata));
                [invocation setArgument:&closure atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_GRAVITY:
            case OBJC_BRIDGE_TYPE_VPTR: {
                // this case is used when an unknown number of arguments can be passed to an event
                void *ptr;
                if (gravity_value_isobject(gravity_value)) ptr = VALUE_AS_OBJECT(gravity_value);
                else ptr = NULL;
                [invocation setArgument:&ptr atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_INT8: {
                gravity_value_t v = convert_value2int(vm, gravity_value);
                if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert parameter %d to Int (in %s).", k-1, objc_bridge_get_exposed_name(xdata));
                
                char value = (char)v.n;
                [invocation setArgument:&value atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_INT16: {
                gravity_value_t v = convert_value2int(vm, gravity_value);
                if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert parameter %d to Int (in %s).", k-1, objc_bridge_get_exposed_name(xdata));
                
                short value = (short)v.n;
                [invocation setArgument:&value atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_INT32: {
                gravity_value_t v = convert_value2int(vm, gravity_value);
                if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert parameter %d to Int (in %s).", k-1, objc_bridge_get_exposed_name(xdata));
                
                long value = (long)v.n;
                [invocation setArgument:&value atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_INT64: {
                gravity_value_t v = convert_value2int(vm, gravity_value);
                if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert parameter %d to Int (in %s).", k-1, objc_bridge_get_exposed_name(xdata));
                
                long long value = (long long)v.n;
                [invocation setArgument:&value atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_UINT8: {
                gravity_value_t v = convert_value2int(vm, gravity_value);
                if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert parameter %d to Int (in %s).", k-1, objc_bridge_get_exposed_name(xdata));
                
                unsigned char value = (unsigned char)v.n;
                [invocation setArgument:&value atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_UINT16: {
                gravity_value_t v = convert_value2int(vm, gravity_value);
                if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert parameter %d to Int (in %s).", k-1, objc_bridge_get_exposed_name(xdata));
                
                unsigned short value = (unsigned short)v.n;
                [invocation setArgument:&value atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_UINT32: {
                gravity_value_t v = convert_value2int(vm, gravity_value);
                if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert parameter %d to Int (in %s).", k-1, objc_bridge_get_exposed_name(xdata));
                
                unsigned long value = (unsigned long)v.n;
                [invocation setArgument:&value atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_UINT64: {
                gravity_value_t v = convert_value2int(vm, gravity_value);
                if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert parameter %d to Int (in %s).", k-1, objc_bridge_get_exposed_name(xdata));
                
                unsigned long long value = (unsigned long long)v.n;
                [invocation setArgument:&value atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_FLOAT: {
                gravity_value_t v = convert_value2float(vm, gravity_value);
                if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert parameter %d to Float (in %s).", k-1, objc_bridge_get_exposed_name(xdata));
                
                float value = (float)v.f;
                [invocation setArgument:&value atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_DOUBLE: {
                gravity_value_t v = convert_value2float(vm, gravity_value);
                if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert parameter %d to Float (in %s).", k-1, objc_bridge_get_exposed_name(xdata));
                
                double value = (double)v.f;
                [invocation setArgument:&value atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_BOOL: {
                gravity_value_t v = convert_value2bool(vm, gravity_value);
                if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert parameter %d to Bool (in %s).", k-1, objc_bridge_get_exposed_name(xdata));
                
                bool value = (bool)v.n;
                [invocation setArgument:&value atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_NSINTEGER: {
                gravity_value_t v = convert_value2int(vm, gravity_value);
                if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert parameter %d to Int (in %s).", k-1, objc_bridge_get_exposed_name(xdata));
                
                NSInteger value = (NSInteger)v.n;
                [invocation setArgument:&value atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_NSUINTEGER: {
                gravity_value_t v = convert_value2int(vm, gravity_value);
                if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert parameter %d to Int (in %s).", k-1, objc_bridge_get_exposed_name(xdata));
                
                NSUInteger value = (NSUInteger)v.n;
                [invocation setArgument:&value atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_ID: {
                id value = nil;
                
                if (VALUE_ISA_INSTANCE(gravity_value)) value = (__bridge id)(VALUE_AS_INSTANCE(gravity_value)->xdata);
                else if (VALUE_ISA_LIST(gravity_value)) value = convert_gravity2nsarray(vm, gravity_value);
                else if (VALUE_ISA_MAP(gravity_value)) value = convert_gravity2nsdictionary(vm, gravity_value);
                else value = convert_gravity2obj(vm, gravity_value, OBJC_BRIDGE_TYPE_ID);
                
                if (value) [arguments addObject:value];
                [invocation setArgument:&value atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_NSNUMBER: {
                NSNumber *value = nil;
                if (VALUE_ISA_INT(gravity_value)) value = @(gravity_value.n);
                else if (VALUE_ISA_FLOAT(gravity_value)) value = @(gravity_value.f);
                else {
                    gravity_value_t v = convert_value2int(vm, gravity_value);
                    if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert parameter %d to Int (in %s).", k-1, objc_bridge_get_exposed_name(xdata));
                    value = @(v.n);
                }
                
                [arguments addObject:value];
                [invocation setArgument:&value atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_NSSTRING: {
                if (VALUE_ISA_NULL(gravity_value) && is_default_value) {
                    id value = nil;
                    [invocation setArgument:&value atIndex:k];
                } else {
                    gravity_value_t v = convert_value2string(vm, gravity_value);
                    if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert parameter %d to String (in %s).", k-1, objc_bridge_get_exposed_name(xdata));
                
                    NSString *value = [NSString stringWithUTF8String:VALUE_AS_CSTRING(v)];
                    [arguments addObject:value];
                    [invocation setArgument:&value atIndex:k];
                }
            } break;
                
            case OBJC_BRIDGE_TYPE_NSARRAY: {
                id value = convert_gravity2nsarray(vm, gravity_value);
                if (value) [arguments addObject:value];
                [invocation setArgument:&value atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_NSDICTIONARY: {
                id value = convert_gravity2nsdictionary(vm, gravity_value);
                if (value) [arguments addObject:value];
                [invocation setArgument:&value atIndex:k];
            } break;
            
            // STRUCT BASED ARGUMENTS
            case OBJC_BRIDGE_TYPE_POINT: {
                // guarantee to return a non null NSValue
                NSValue *value = (NSValue *)convert_gravity2type(vm, gravity_value, xdata->argtype[i]);
                CGPoint point = value.pointValue;
                [invocation setArgument:&point atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_RECT: {
                // guarantee to return a non null NSValue
                NSValue *value = (NSValue *)convert_gravity2type(vm, gravity_value, xdata->argtype[i]);
                CGRect rect = value.rectValue;
                [invocation setArgument:&rect atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_SIZE: {
                // guarantee to return a non null NSValue
                NSValue *value = (NSValue *)convert_gravity2type(vm, gravity_value, xdata->argtype[i]);
                CGSize size = value.sizeValue;
                [invocation setArgument:&size atIndex:k];
            } break;
                
            case OBJC_BRIDGE_TYPE_EDGEINSETS: {
                #if TARGET_OS_IPHONE
                // guarantee to return a non null NSValue
                NSValue *value = (NSValue *)convert_gravity2type(vm, gravity_value, xdata->argtype[i]);
                UIEdgeInsets insets = value.edgeInsetsValue;
                [invocation setArgument:&insets atIndex:k];
                #endif
            } break;
                
            case OBJC_BRIDGE_TYPE_OFFSET: {
                #if TARGET_OS_IPHONE
                // guarantee to return a non null NSValue
                NSValue *value = (NSValue *)convert_gravity2type(vm, gravity_value, xdata->argtype[i]);
                UIOffset offset = value.offsetValue;
                [invocation setArgument:&offset atIndex:k];
                #endif
            } break;
                
            case OBJC_BRIDGE_TYPE_RANGE: {
                NSRange r = convert_gravity2nsrange (vm, gravity_value);
                [invocation setArgument:&r atIndex:k];
            } break;
                
            // OBJ BASED ARGUMENTS
            case OBJC_BRIDGE_TYPE_NSDATE:
            case OBJC_BRIDGE_TYPE_NSDATA:
            case OBJC_BRIDGE_TYPE_IMAGE:
            case OBJC_BRIDGE_TYPE_COLOR:
            case OBJC_BRIDGE_TYPE_GRADIENT:
            case OBJC_BRIDGE_TYPE_SOUND:
            case OBJC_BRIDGE_TYPE_MOVIE:
            case OBJC_BRIDGE_TYPE_FONT: {
                id value = convert_gravity2type(vm, gravity_value, xdata->argtype[i]);
                if (value) [arguments addObject:value];
                [invocation setArgument:&value atIndex:k];
            } break;
            
            case OBJC_BRIDGE_TYPE_USER:
            default: {
                #ifdef CREO_PROJECT
                id<CREORuntimeDelegate> delegate = (__bridge id<CREORuntimeDelegate>)(gravity_vm_getdata(vm));
                if (!delegate) RETURN_ERROR("Delegate not set.");
                
                Class c = [delegate classByTag:xdata->argtype[i]];
                if (!c) RETURN_ERROR("Unable to find class name for class tag %d", xdata->argtype[i]);
                if (!VALUE_ISA_INSTANCE(gravity_value)) {
                    gravity_value_t v = convert_value2string(vm, gravity_value);
                    const char *cname = NSStringFromClass(c).UTF8String;
                    const char *vstring = (VALUE_ISA_STRING(v)) ? VALUE_AS_CSTRING(v) : "N/A";
                    RETURN_ERROR("Unable to convert parameter %s to %s in %s.", vstring, cname, xdata->name);
                }
                
                gravity_instance_t *instance = VALUE_AS_INSTANCE(gravity_value);
                id value = (instance) ? (__bridge id)(instance->xdata) : nil;
                if (![value isKindOfClass:c]) {
                    RETURN_ERROR("Wrong parameter (position %d of %s).", k-1, objc_bridge_get_exposed_name(xdata));
                }
                
                [arguments addObject:value];
                [invocation setArgument:&value atIndex:k];
                #else
                assert(0);
                #endif
            } break;
        }
    }
    
    // invoke function
    @try {
        [invocation invokeWithTarget:callee];
    }
    @catch (NSException * e) {
        gravity_vm_seterror(vm, "An error occurred while calling %s (%s).", objc_bridge_get_exposed_name(xdata), [[e reason] UTF8String]);
        return false;
    }
    
    // process return value
    switch (xdata->rettype) {
        case OBJC_BRIDGE_TYPE_UNKNOWN:
        case OBJC_BRIDGE_TYPE_LDOUBLE:
            assert(0);
            
        case OBJC_BRIDGE_TYPE_VOID: {
            gravity_vm_setslot(vm, VALUE_FROM_NULL, rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_BOOL: {
            char buffer[2] = {0};
            [invocation getReturnValue:&buffer];
            gravity_vm_setslot(vm, (buffer[0] == 0) ? VALUE_FROM_FALSE : VALUE_FROM_TRUE, rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_INT16: {
            short buffer = 0;
            [invocation getReturnValue:&buffer];
            gravity_vm_setslot(vm, VALUE_FROM_INT(buffer), rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_INT32: {
            long buffer = 0;
            [invocation getReturnValue:&buffer];
            gravity_vm_setslot(vm, VALUE_FROM_INT(buffer), rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_INT64: {
            long long buffer = 0;
            [invocation getReturnValue:&buffer];
            gravity_vm_setslot(vm, VALUE_FROM_INT(buffer), rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_UINT8: {
            unsigned char buffer = 0;
            [invocation getReturnValue:&buffer];
            gravity_vm_setslot(vm, VALUE_FROM_INT(buffer), rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_UINT16: {
            unsigned short buffer = 0;
            [invocation getReturnValue:&buffer];
            gravity_vm_setslot(vm, VALUE_FROM_INT(buffer), rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_UINT32: {
            unsigned long buffer = 0;
            [invocation getReturnValue:&buffer];
            gravity_vm_setslot(vm, VALUE_FROM_INT(buffer), rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_UINT64: {
            unsigned long long buffer = 0;
            [invocation getReturnValue:&buffer];
            gravity_vm_setslot(vm, VALUE_FROM_INT(buffer), rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_NSINTEGER: {
            NSInteger buffer = 0;
            [invocation getReturnValue:&buffer];
            gravity_vm_setslot(vm, VALUE_FROM_INT(buffer), rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_NSUINTEGER: {
            NSUInteger buffer = 0;
            [invocation getReturnValue:&buffer];
            gravity_vm_setslot(vm, VALUE_FROM_INT(buffer), rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_FLOAT: {
            float buffer = 0.0;
            [invocation getReturnValue:&buffer];
            gravity_vm_setslot(vm, VALUE_FROM_FLOAT(buffer), rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_DOUBLE: {
            double buffer = 0.0;
            [invocation getReturnValue:&buffer];
            gravity_vm_setslot(vm, VALUE_FROM_FLOAT(buffer), rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_VPTR:
        case OBJC_BRIDGE_TYPE_CPTR: {
            void *buffer = NULL;
            [invocation getReturnValue:&buffer];
            assert(0);
        }
        
        case OBJC_BRIDGE_TYPE_INIT: {
            assert(rindex == GRAVITY_DATA_REGISTER);
            id obj = nil;
            [invocation getReturnValue:&obj];
            gravity_vm_setdata(vm, (void *)CFBridgingRetain(obj));
            break;
        }
            
        case OBJC_BRIDGE_TYPE_NSNUMBER:
        case OBJC_BRIDGE_TYPE_NSSTRING:
        case OBJC_BRIDGE_TYPE_NSARRAY:
        case OBJC_BRIDGE_TYPE_NSDICTIONARY:
        case OBJC_BRIDGE_TYPE_NSDATE:
        case OBJC_BRIDGE_TYPE_NSDATA:
        case OBJC_BRIDGE_TYPE_ID:
        case OBJC_BRIDGE_TYPE_USER:
        case OBJC_BRIDGE_TYPE_GRAVITY:
            
        case OBJC_BRIDGE_TYPE_COLOR:
        case OBJC_BRIDGE_TYPE_SOUND:
        case OBJC_BRIDGE_TYPE_IMAGE:
        case OBJC_BRIDGE_TYPE_GRADIENT:
        case OBJC_BRIDGE_TYPE_FONT: {
            // https://stackoverflow.com/questions/11874056/nsinvocation-getreturnvalue-called-inside-forwardinvocation-makes-the-returned
            __unsafe_unretained id obj = nil;
            [invocation getReturnValue:&obj];
            gravity_vm_setslot(vm, bridge_objc2gravity(vm, obj, xdata->rettype), rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_POINT: {
            CGPoint v;
            [invocation getReturnValue:&v];
            gravity_vm_setslot(vm, bridge_objc2gravity(vm, @(v), xdata->rettype), rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_SIZE: {
            CGSize v;
            [invocation getReturnValue:&v];
            gravity_vm_setslot(vm, bridge_objc2gravity(vm, @(v), xdata->rettype), rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_RECT: {
            CGRect v;
            [invocation getReturnValue:&v];
            gravity_vm_setslot(vm, bridge_objc2gravity(vm, @(v), xdata->rettype), rindex);
            break;
        }
            
        case OBJC_BRIDGE_TYPE_EDGEINSETS: {
            #if TARGET_OS_IPHONE
            UIEdgeInsets v;
            [invocation getReturnValue:&v];
            gravity_vm_setslot(vm, bridge_objc2gravity(vm, [NSValue valueWithUIEdgeInsets:v], xdata->rettype), rindex);
            #endif
            break;
        }
            
        case OBJC_BRIDGE_TYPE_OFFSET: {
            #if TARGET_OS_IPHONE
            UIOffset v;
            [invocation getReturnValue:&v];
            gravity_vm_setslot(vm, bridge_objc2gravity(vm, [NSValue valueWithUIOffset:v], xdata->rettype), rindex);
            #endif
            break;
        }
            
        case OBJC_BRIDGE_TYPE_RANGE: {
            NSRange v;
            [invocation getReturnValue:&v];
            gravity_vm_setslot(vm, bridge_objc2gravity(vm, [NSValue valueWithRange:v], xdata->rettype), rindex);
            break;
        }
            
        default:
            /*
             OBJC_BRIDGE_TYPE_CLASS
             OBJC_BRIDGE_TYPE_SEL
             OBJC_BRIDGE_TYPE_MOVIE
             */
            // default is to ignore return values and not to assert
            NSLog(@"Unhandled bridge_execute return value case");
            gravity_vm_setslot(vm, VALUE_FROM_NULL, rindex);
            break;
    }
    
    return true;
}

const char *bridge_string (gravity_vm *vm, void *xdata, uint32_t *len) {
    #pragma unused(vm)
    // assuming xdata is an objc object
    NSObject *obj = (__bridge NSObject *)(xdata);
    if ([obj respondsToSelector:@selector(description)]) {
        NSString *description = [obj performSelector:@selector(description)];
        *len = (uint32_t)[description lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
        return description.UTF8String;
    }
    return NULL;
}

void bridge_blacken (gravity_vm *vm, void *xdata) {
//    NSObject *obj = (__bridge NSObject *)(xdata);
//    if ([obj respondsToSelector:@selector(runtimeInstance)]) {
//        id r = [obj valueForKey:@"runtimeInstance"];
//        if (!r) return;
//    } else return;
//
///*
//    if ([obj respondsToSelector:@selector(gravityInstance)]) {
//        gravity_instance_t *instance = (__bridge gravity_instance_t *)[obj valueForKey:@"gravityInstance"];
//        if (instance) gravity_instance_blacken(vm, instance);
//    }
//  */
//
//    #pragma clang diagnostic push
//    #pragma clang diagnostic ignored "-Wundeclared-selector"
//    if ([obj respondsToSelector:@selector(gravityBlacken)]) {
//        [obj performSelector:@selector(gravityBlacken)];
//    }
//    #pragma clang diagnostic pop
}

bool bridge_equals (gravity_vm *vm, void *obj_1, void *obj_2) {
    #pragma unused(vm)
    // assuming both obj1 and obj2 are objc objects
    NSObject *obj1 = (__bridge NSObject *)(obj_1);
    NSObject *obj2 = (__bridge NSObject *)(obj_2);
    if ([obj1 respondsToSelector:@selector(isEqual:)]) {
        return [obj1 isEqual:obj2];
    }
    return false;
}

void *bridge_clone (gravity_vm *vm, void *xdata) {
    if (!xdata) return NULL;
    NSObject *clone = nil;
    
    #ifdef CREO_PROJECT
    NSObject *obj = (__bridge NSObject *)(xdata);
    MKObjectID objectID = [obj objectID];
    gravity_delegate_t  *delegate = gravity_vm_delegate(vm);
    CREOApplication *app = (__bridge CREOApplication *)delegate->xdata;
    if (objectID != MKNotFound) {
        clone = [app createObjectWithID:objectID container:nil error:nil useCache:NO];
    } else {
        clone = [app createObjectWithClass:obj.class];
        if ([obj respondsToSelector:@selector(value)]) {
            [clone setValue:[obj valueForKey:@"value"] forKey:@"value"];
        }
    }
// WE CURRENTLY DO NOT SUPPORT PROPERTY SET VIA CODE (ONLY INSPECTOR PROPERTIES ARE SUPPORTED)
//    if (clone) {
//        unsigned int outCount, i;
//        objc_property_t *properties = class_copyPropertyList([obj class], &outCount);
//        for (i = 0; i < outCount; i++) {
//            objc_property_t property = properties[i];
//            const char *propName = property_getName(property);
//            if (propName) {
//                NSString *key = @(propName);
//                id value = [obj valueForKey:key];
//                [clone setValue:value forKey:key];
//                // NSLog(@"%@ %@", key, value);
//            }
//        }
//        free(properties);
//    }
    #endif
    return (clone) ? (void *)CFBridgingRetain(clone) : NULL;
}

// MARK: - Free -

static void bridge_free_instance (gravity_vm *vm, gravity_instance_t *i) {
    #pragma unused(vm)
    DEBUG_XDATA(@"\tBRIDGE FREE INSTANCE %@", i->xdata);
    
    #if GRAVITY_BRIDGE_DEBUG_MEMORY
    NSLog(@"Free instance %p (%@)", i->xdata, NSStringFromClass([(__bridge id)i->xdata class]));
    #endif
    
    if (!i->xdata) return;
    
    #ifdef CREO_PROJECT
    set_runtime_instance(vm, (__bridge id)(i->xdata), nil);
    NSObject *obj = (__bridge NSObject *)(i->xdata);
    if ([obj respondsToSelector:@selector(removeFromSuperview)]) [(UIView*)obj removeFromSuperview];
    #endif
    
    FREE_OBJC_VALUE(i->xdata);
}

static void bridge_free_closure (gravity_vm *vm, gravity_closure_t *closure, bool is_property) {
    DEBUG_XDATA(@"\tBRIDGE FREE CLOSURE %p", closure->f);
    
    #pragma unused(vm)
    if (closure->f->tag == EXEC_TYPE_SPECIAL) {
        assert(closure->f->index == GRAVITY_BRIDGE_INDEX);
        if (closure->f->xdata) objc_bridge_var_free((objc_bridge_var_t *)closure->f->xdata);
        closure->f->xdata = NULL;
        gravity_closure_t *getter = (gravity_closure_t *)closure->f->special[0];
        gravity_closure_t *setter = (closure->f->special[0] != closure->f->special[1]) ? (gravity_closure_t *)closure->f->special[1] : NULL;
        if (getter) bridge_free_closure(vm, getter, true);
        if (setter) bridge_free_closure(vm, setter, true);
    }
    
    if (closure->f->tag == EXEC_TYPE_BRIDGED) {
        if (is_property) {
            objc_bridge_var_free((objc_bridge_var_t *)closure->f->xdata);
        } else {
            objc_bridge_func_free((objc_bridge_func_t *)closure->f->xdata);
        }
        closure->f->xdata = NULL;
    }
    
    if (closure->f->xdata) {
        objc_bridge_var_free((objc_bridge_var_t *)closure->f->xdata);
    }
    gravity_function_t *f = closure->f;
    gravity_closure_free(NULL, closure);
    gravity_function_free(NULL, f);
}

static void bridge_hash_iterate (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value, void *data) {
    #pragma unused(hashtable, key)
    if (gravity_value_isobject(value)) {
        bridge_free((gravity_vm*)data, VALUE_AS_OBJECT(value));
    }
}

static void bridge_free_class (gravity_vm *vm, gravity_class_t *c) {
    if (!c->xdata) return;
    
    DEBUG_XDATA(@"BRIDGE FREE CLASS %s %p", c->identifier, c);
    
    // free meta class first
    gravity_class_t *meta = gravity_class_get_meta(c);
    gravity_hash_iterate(meta->htable, bridge_hash_iterate, (void *)vm);
    
    // then free real class
    gravity_hash_iterate(c->htable, bridge_hash_iterate, (void *)vm);
    FREE_OBJC_VALUE(c->xdata);
}

void bridge_free (gravity_vm *vm, gravity_object_t *obj) {
    if (OBJECT_ISA_INSTANCE(obj)) {
        bridge_free_instance(vm, (gravity_instance_t *)obj);
    } else if (OBJECT_ISA_CLOSURE(obj)) {
        bridge_free_closure(vm, (gravity_closure_t *)obj, false);
    } else if (OBJECT_ISA_CLASS(obj)) {
        bridge_free_class(vm, (gravity_class_t *)obj);
    } else {
        // should never reach this point
        assert(0);
    }
}

