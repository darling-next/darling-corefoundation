/*
 This file is part of Darling.

 Copyright (C) 2026 Darling contributors

 Darling is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Darling is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Darling.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <CoreFoundation/CFBase.h>

/*
 * The pasteboard bridge in Xcode's SimPasteboardPlus links these private
 * CoreFoundation entry points.  Darling has no pasteboard service, so the
 * operations report that they are unavailable instead of pretending to
 * succeed.  The two struct layouts below are the C ABI the bridge reads:
 * a failed operation has a non-zero status and null result fields.
 *
 * These are deliberately not exported into the public headers: Apple keeps
 * CFPasteboard private, and no Darling framework needs to call it.
 */

typedef struct {
	int status;
	void *value;
} DarlingCFPasteboardResult;

typedef struct {
	int status;
	void *value;
	void *auxiliary;
} DarlingCFPasteboardDataResult;

static const int kDarlingCFPasteboardUnavailable = 1;

unsigned long long CFPasteboardGetGenerationCount(void *pasteboard)
{
	(void) pasteboard;
	return 0;
}

DarlingCFPasteboardResult CFPasteboardBeginGeneration(void *pasteboard)
{
	(void) pasteboard;
	DarlingCFPasteboardResult result = { kDarlingCFPasteboardUnavailable, NULL };
	return result;
}

DarlingCFPasteboardResult CFPasteboardEnumeratePasteboardUsingBlock(void *pasteboard, void *argument, void *block)
{
	(void) pasteboard;
	(void) argument;
	(void) block;
	DarlingCFPasteboardResult result = { kDarlingCFPasteboardUnavailable, NULL };
	return result;
}

int CFPasteboardSetData(void *pasteboard, void *flavor, void *data, void *auxiliary, void *argument, void *block)
{
	(void) pasteboard;
	(void) flavor;
	(void) data;
	(void) auxiliary;
	(void) argument;
	(void) block;
	return kDarlingCFPasteboardUnavailable;
}

int CFPasteboardPromiseDataUsingBlock(void *pasteboard, void *flavor, void *data, void *auxiliary, void *block)
{
	(void) pasteboard;
	(void) flavor;
	(void) data;
	(void) auxiliary;
	(void) block;
	return kDarlingCFPasteboardUnavailable;
}

DarlingCFPasteboardDataResult CFPasteboardCopyData(void *pasteboard, void *flavor, void *type, void *auxiliary)
{
	(void) pasteboard;
	(void) flavor;
	(void) type;
	(void) auxiliary;
	DarlingCFPasteboardDataResult result = { kDarlingCFPasteboardUnavailable, NULL, NULL };
	return result;
}
