
#pragma once

#include "addresses.h"

/*
	A utility class that unprotects an area of memory while it is in scope.
	Addresses are specified according to the default image base of temple.dll
	and are rebased to the actual image base automatically.
*/
class MemoryUnprotector {
public:
	MemoryUnprotector(uint32_t dllAddress, size_t size) : mAddress(temple_address(dllAddress)), mSize(size), mOldProtection(0) {
		// rebase the address using temple_address
		BOOST_ASSERT(VirtualProtect(mAddress, mSize, PAGE_READWRITE, &mOldProtection));
	}
	~MemoryUnprotector() {
		BOOST_ASSERT(VirtualProtect(mAddress, mSize, mOldProtection, &mOldProtection));
	}
private:
	void *mAddress; // This is the real address
	size_t mSize;
	DWORD mOldProtection;
};

class TempleFix {
public:
	TempleFix();
	virtual ~TempleFix();

	virtual const char *name() = 0;
	virtual void apply() = 0;

protected:
	void write(uint32_t offset, const void *buffer, size_t size);
	void read(uint32_t offset, void *buffer, size_t size);
	void writeHex(uint32_t offset, const string &hexPattern);
	void redirectCall(uint32_t offset, void* redirectTo);
	void *replaceFunction(uint32_t offset, void *replaceWith);
	void writeCall(uint32_t offset, void* redirectTo);
};

class TempleFixes {
	friend class TempleFix;
public:
	static void apply();
private:
	static vector<TempleFix*> &fixes();
};
