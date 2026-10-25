// POSIX backing reused from VibeDarling/darling-corefoundation,
// 85f9253f73f65a2c9b27bd9a486489db5c786cd3, NSFileSecurity.m.
#import <Foundation/NSObject.h>
#include <CoreFoundation/CFFileSecurity.h>
#include <CoreFoundation/CFRuntime.h>
#include <dispatch/dispatch.h>

// These are Objective-C objects, not CFRuntimeBase allocations. Keep the
// registered CF type's default __NSCFType bridge: mapping it to NSFileSecurity
// would make CFRuntime mistake this NSObject storage for a CFRuntimeBase.
static const CFRuntimeClass __CFFileSecurityClass = {
	.version = 0,
	.className = "CFFileSecurity",
};

CFTypeID CFFileSecurityGetTypeID(void)
{
	static dispatch_once_t once;
	static CFTypeID typeID;
	dispatch_once(&once, ^{
		typeID = _CFRuntimeRegisterClass(&__CFFileSecurityClass);
	});
	return typeID;
}

// Only POSIX owner, group and mode are represented. UUID/ACL APIs remain
// unavailable; do not add failure-only exports for unimplemented properties.
@interface NSFileSecurity : NSObject <NSCopying>
{
@public
	uid_t _owner;
	gid_t _group;
	mode_t _mode;
	BOOL _hasOwner, _hasGroup, _hasMode;
}
@end

@implementation NSFileSecurity

- (CFTypeID)_cfTypeID
{
	return CFFileSecurityGetTypeID();
}

- (id)copyWithZone:(NSZone *)zone
{
	NSFileSecurity *copy = [[NSFileSecurity allocWithZone:zone] init];
	if (!copy) return nil;
	copy->_owner = _owner;
	copy->_group = _group;
	copy->_mode = _mode;
	copy->_hasOwner = _hasOwner;
	copy->_hasGroup = _hasGroup;
	copy->_hasMode = _hasMode;
	return copy;
}

@end

#define SEC(ref) ((NSFileSecurity *)(id)(ref))

CFFileSecurityRef CFFileSecurityCreate(CFAllocatorRef allocator)
{
	return (CFFileSecurityRef)[[NSFileSecurity alloc] init];
}

CFFileSecurityRef CFFileSecurityCreateCopy(CFAllocatorRef allocator, CFFileSecurityRef fileSec)
{
	return fileSec ? (CFFileSecurityRef)[SEC(fileSec) copy] : NULL;
}

Boolean CFFileSecurityGetOwner(CFFileSecurityRef fileSec, uid_t *owner)
{
	if (!fileSec || !SEC(fileSec)->_hasOwner) return false;
	if (owner) *owner = SEC(fileSec)->_owner;
	return true;
}

Boolean CFFileSecuritySetOwner(CFFileSecurityRef fileSec, uid_t owner)
{
	if (!fileSec) return false;
	SEC(fileSec)->_owner = owner;
	SEC(fileSec)->_hasOwner = YES;
	return true;
}

Boolean CFFileSecurityGetGroup(CFFileSecurityRef fileSec, gid_t *group)
{
	if (!fileSec || !SEC(fileSec)->_hasGroup) return false;
	if (group) *group = SEC(fileSec)->_group;
	return true;
}

Boolean CFFileSecuritySetGroup(CFFileSecurityRef fileSec, gid_t group)
{
	if (!fileSec) return false;
	SEC(fileSec)->_group = group;
	SEC(fileSec)->_hasGroup = YES;
	return true;
}

Boolean CFFileSecurityGetMode(CFFileSecurityRef fileSec, mode_t *mode)
{
	if (!fileSec || !SEC(fileSec)->_hasMode) return false;
	if (mode) *mode = SEC(fileSec)->_mode;
	return true;
}

Boolean CFFileSecuritySetMode(CFFileSecurityRef fileSec, mode_t mode)
{
	if (!fileSec) return false;
	SEC(fileSec)->_mode = mode & 07777;
	SEC(fileSec)->_hasMode = YES;
	return true;
}

Boolean CFFileSecurityClearProperties(CFFileSecurityRef fileSec, CFOptionFlags clearPropertyMask)
{
	const CFOptionFlags supported = kCFFileSecurityClearOwner |
		kCFFileSecurityClearGroup | kCFFileSecurityClearMode;
	if (!fileSec || (clearPropertyMask & ~supported)) return false;
	if (clearPropertyMask & kCFFileSecurityClearOwner) SEC(fileSec)->_hasOwner = NO;
	if (clearPropertyMask & kCFFileSecurityClearGroup) SEC(fileSec)->_hasGroup = NO;
	if (clearPropertyMask & kCFFileSecurityClearMode) SEC(fileSec)->_hasMode = NO;
	return true;
}


