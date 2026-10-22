//
//  NSData.m
//  CoreFoundation
//
//  Copyright (c) 2014 Apportable. All rights reserved.
//

#import <CoreFoundation/CFData.h>
#import <Foundation/NSData.h>
#import "CFPriv.h"

@implementation NSData
@end

@implementation NSMutableData
@end

CF_PRIVATE
@interface __NSCFData : NSMutableData {
    unsigned char _cfinfo[4];
    CFIndex _length;
    CFIndex _capacity;
    CFAllocatorRef *_bytesDeallocator;
    char *_bytes;
}
@end

@implementation __NSCFData

+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key
{
    return NO;
}

- (void)setLength:(NSUInteger)length
{
    CFDataSetLength((CFMutableDataRef)self, length);
}

- (void *)mutableBytes
{
    return CFDataGetMutableBytePtr((CFMutableDataRef)self);
}

- (const void *)bytes
{
    return CFDataGetBytePtr((CFDataRef)self);
}

- (NSUInteger)length
{
    return CFDataGetLength((CFDataRef)self);
}

- (id)copyWithZone:(NSZone *)zone
{
    return (id)CFDataCreateCopy(kCFAllocatorDefault, (CFDataRef)self);
}

- (NSUInteger)retainCount
{
    return CFGetRetainCount((CFTypeRef)self);
}

- (BOOL)_isDeallocating
{
    return _CFIsDeallocating((CFTypeRef)self);
}

- (BOOL)_tryRetain
{
    return _CFTryRetain((CFTypeRef)self) != NULL;
}

- (oneway void)release
{
    CFRelease((CFTypeRef)self);
}

- (id)retain
{
    return (id)CFRetain((CFTypeRef)self);
}

- (NSUInteger)hash
{
    return CFHash((CFTypeRef)self);
}

- (BOOL)isEqual:(id)object
{
    if (object == nil)
    {
        return NO;
    }
    return CFEqual((CFTypeRef)self, (CFTypeRef)object);
}

@end

/* NSConstantData is the class of the data the compiler emits for constant
 * (literal) byte payloads; in Apple's CoreFoundation it is one of the exported
 * constant-collection classes (CoreFoundation.tbd: objc-classes NSConstantData,
 * objc-ivars NSConstantData._bytes, NSConstantData._length).  Clients reference
 * the CLASS OBJECT by name -- xcodebuild imports _OBJC_CLASS_$_NSConstantData,
 * checked against the client's own bytes with
 * `llvm-nm -m <client> | grep NSConstant` -- and dyld stops with
 * "Symbol not found: _OBJC_CLASS_$_NSConstantData / Expected in CoreFoundation"
 * without it.  The ivars match Apple's declared names, so an instance laid out
 * by an Apple-built client is read with the right offsets.  Deliberately NOT
 * CF_PRIVATE: it is an exported symbol in Apple's CoreFoundation. */
@interface NSConstantData : NSData {
    const uint8_t *_bytes;
    NSUInteger _length;
}
@end

@implementation NSConstantData

- (NSUInteger)length
{
    return _length;
}

- (const void *)bytes
{
    return _bytes;
}

@end
