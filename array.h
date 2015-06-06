


/**
 * I'm copying this class here so that users don't need to import the
 * boost library. I claim no copyright over the class in question.
 */


/* The following code declares class array,
 * an STL container (as wrapper) for arrays of constant size.
 *
 * See
 *      http://www.boost.org/libs/array/
 * for documentation.
 *
 * The original author site is at: http://www.josuttis.com/
 *
 * (C) Copyright Nicolai M. Josuttis 2001.
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * 28 Dec 2010 - (mtc) Added cbegin and cend (and crbegin and crend) for C++Ox compatibility.
 * 10 Mar 2010 - (mtc) fill method added, matching resolution of the standard library working group.
 *      See <http://www.open-std.org/jtc1/sc22/wg21/docs/lwg-defects.html#776> or Trac issue #3168
 *      Eventually, we should remove "assign" which is now a synonym for "fill" (Marshall Clow)
 * 10 Mar 2010 - added workaround for SUNCC and !STLPort [trac #3893] (Marshall Clow)
 * 29 Jan 2004 - c_array() added, BOOST_NO_PRIVATE_IN_AGGREGATE removed (Nico Josuttis)
 * 23 Aug 2002 - fix for Non-MSVC compilers combined with MSVC libraries.
 * 05 Aug 2001 - minor update (Nico Josuttis)
 * 20 Jan 2001 - STLport fix (Beman Dawes)
 * 29 Sep 2000 - Initial Revision (Nico Josuttis)
 *
 * Jan 29, 2004
 */
#ifndef BOOST_ARRAY_HPP
#define BOOST_ARRAY_HPP


#include <sstream>
#include <algorithm>
#include <string.h>

template<class T, std::size_t N>
class array {
public:
	T elems[N]; // fixed-size array of elements of type T

public:
	// type definitions
	typedef T value_type;
	typedef T* iterator;
	typedef const T* const_iterator;
	typedef T& reference;
	typedef const T& const_reference;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;

	array() {
	}

	// iterator support
	iterator begin() {
		return elems;
	}
	const_iterator begin() const {
		return elems;
	}
	const_iterator cbegin() const {
		return elems;
	}

	iterator end() {
		return elems + N;
	}
	const_iterator end() const {
		return elems + N;
	}
	const_iterator cend() const {
		return elems + N;
	}

	// operator[]
	reference operator[](size_type i) {
		return elems[i];
	}
	reference at(size_type i) {
		assert(i < N);
		return elems[i];
	}

	bool operator<(const array<T, N> &other) const {
		for (uint k = 0; k < N; ++k)
			if (elems[k] < other.elems[k])
				return true;
			else if (elems[k] > other.elems[k])
				return false;
		return false;
	}

	const_reference operator[](size_type i) const {
		return elems[i];
	}

	const_reference at(size_type i) const {
		assert(i < N);
		return elems[i];
	}
	// front() and back()
	reference front() {
		return elems[0];
	}

	const_reference front() const {
		return elems[0];
	}

	reference back() {
		return elems[N - 1];
	}

	const_reference back() const {
		return elems[N - 1];
	}

	// size is constant
	static size_type size() {
		return N;
	}
	static bool empty() {
		return false;
	}
	static size_type max_size() {
		return N;
	}
	enum {
		static_size = N
	};

	// assignment with type conversion
	template<typename T2>
	array<T, N>& operator=(const array<T2, N>& rhs) {
		std::copy(rhs.begin(), rhs.end(), begin());
		return *this;
	}

	/*array<T,N>& operator=( array<T,N> &other)  {
	 ::memcpy(& other.elems[0],&elems[0],sizeof(T)*N);
	 return *this;
	 }*/

	bool operator==(const array<T, N> &other) const {
		for (uint k = 0; k < N; ++k)
			if (other.elems[k] != elems[k])
				return false;
		return true;
	}
	bool operator!=(const array<T, N> &other) const {
			for (uint k = 0; k < N; ++k)
				if (other.elems[k] != elems[k])
					return true;
			return false;
	}
	operator string() const {
		return str();
	}

	string str() const {
		stringstream ss;
		for (uint k = 0; k < N; ++k)
			ss<<elems[k]<<" ";
		return ss.str();
	}

	// assign one value to all elements
	void assign(const T& value) {
		fill(value);
	} // A synonym for fill
	void fill(const T& value) {
		std::fill_n(begin(), size(), value);
	}

};

template <class T, int c>
ostream& operator<<(ostream & out, const array<T,c> & a) {
	return out<< a.str();
}


#endif /*BOOST_ARRAY_HPP*/
