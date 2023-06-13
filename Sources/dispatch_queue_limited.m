//
//  dispatch_queue_limited.m
//  IconChamp
//
//  Created by Jeremy on 1/23/21.
//

#if __has_include(<objc/objc.h>)
#define HAVE_OBJC 1
#else
#define HAVE_OBJC 0
#define DQL_USE_OBJC 0
#endif

#if !defined(DQL_USE_OBJC) && HAVE_OBJC
#define DQL_USE_OBJC 1
#endif

#if DQL_USE_OBJC
#define OS_OBJECT_SWIFT3 1
#include "dispatch_queue_limited.h"
#include <objc/objc-runtime.h>
#include <Foundation/NSString.h>

extern void dispatch_queue_limited_debug_desc(dispatch_queue_limited_t dq, char* buf, size_t bufsiz);

@implementation OS_dispatch_queue_limited

+ (void)load {}

- (instancetype)init {
    __asm__("");
    __builtin_trap();
    return [super init];
}

- (NSString*)description {
    
    Class nsString = objc_lookUpClass("NSString");
    if (!nsString) return nil;
    
    char buf[2048];
    dispatch_queue_limited_debug_desc(self, buf, sizeof(buf));
    
    NSString *format = [nsString stringWithUTF8String: "%s[%p] = {\n%s\n}"];
    if (!format) return nil;
    return [nsString stringWithFormat:format, object_getClassName(self), self, buf];
}

- (NSString*)debugDescription {
    return [self description];
}

@end

#endif
