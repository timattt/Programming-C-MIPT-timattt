#ifndef T_STACK
#define T_STACK

#include "tContainer.h"

namespace tStorage {

/*
 *	Author timattt
 */

//!Stack created by timattt
//!It is armed with memory protection. Hash is always checked.
//!
template<typename T, int size, int (*hash)(char*, int) = tDefaultHash> class tStack: private tContainer<
		T, size, (*hash)> {
private:
	int total_objects = 0;
	using tContainer<T, size, (*hash)>::tWriteTo;
	using tContainer<T, size, (*hash)>::tGetFrom;
public:

	//!Inserts object on the top of this stack.
	void tPush(const T &el) {
		assert(total_objects != size);
		tWriteTo(total_objects, el);
		total_objects++;
	}

	//!Delete object from top.
	T tPop() {
		assert(total_objects != 0);
		total_objects--;
		T res = tGetFrom(total_objects);
		return res;
	}

	//!Returns quantity of objects in this stack.
	int tGetSize() {
		return total_objects;
	}

	//!Invokes consumer function for every element in this stack.
	void tForEach(void (*consumer)(const T&)) {
		for (int i = total_objects - 1; i != -1; i--) {
			consumer(this->tGetFrom(i));
		}
	}

	//! To make this stack beautiful. This thing just invokes tPop() function.
	void operator>>(T &el) {
		el = tPop();
	}

	//! To make this stack beautiful. This thing just invokes tPush(el) function.
	void operator<<(const T &el) {
		tPush(el);
	}

	//!This is normal tSeeBitsFunction with one little difference.
	//!After last object will be placed [*] instead of [|].
	char* tSeeBits() {
		char *res = tContainer<T, size, (*hash)>::tSeeBits();
		int mmsi = 1; // Main memory start index.
		for (; res[mmsi - 1] != res[mmsi] || res[mmsi] != '|'; mmsi++)
			;

		for (int i = 0; i < total_objects; i++, mmsi += sizeof(T) * 8 + 1)
			//Plus one because | separates objects.
			;

		if (total_objects != size) { // Because I don't want to see such thing like this [*|]
			res[mmsi] = '*';
		}

		return res;
	}
};
}

#endif
