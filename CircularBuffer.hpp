#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <cstring> // size_t
#include <memory> // get_temporary_buffer() / return_temporary_buffer
#include <stdexcept> // std::runtime_error

class NoBoundaryCheck;
class NoGrow;

////////////////////////////////////////////////////////////////
// CircularBuffer: circular buffer implementation with FIFO semantics
////////////////////////////////////////////////////////////////
template<class T,
		 class GrowPolicy = NoGrow,
		 class CheckBoundariesPolicy = NoBoundaryCheck>
class CircularBuffer
{
public:
	// size: maximum quantity of "T" objects that will be held
	CircularBuffer(size_t size = 0) throw(std::bad_alloc);
	~CircularBuffer();

	// maximum holdable size (passed during construction)
	inline size_t size() const;
	// current object count
	inline size_t count() const;
	// true if no objects are being held, false otherwise
	inline bool isEmpty() const;
	// true if maximum quantity of objects are being held, false otherwise
	inline bool isFull() const;

	// stores elem as the first element (at the top of the buffer)
	void push(const T& elem) throw(std::exception);
	// returns a copy of the last element held and releases it from the buffer
	T pop() throw(std::runtime_error);
	// releases all elements from the buffer
	inline void clear();
	// resize buffer, true if allocation
	void resize(size_t newSize) throw(std::bad_alloc);

	// indexed access to elements
	// NOTE: this operator is for debugging purposes only. You cannot now if
	// a certain index contains a valid object (the element will always be
	// allocated but could be uninitialized).
	inline T& operator[](size_t index) const;

private:
	inline void advance(size_t& currentPos);

	size_t _size;
	size_t _count;
	size_t _nextPushPos;
	size_t _nextPopPos;
	T* _elements; // pointer to buffer
};

////////////////////////////////////////////////////////////////
// CircularBuffer implementation
////////////////////////////////////////////////////////////////
template<class T, class GrowPolicy, class CheckBoundariesPolicy>
CircularBuffer<T, GrowPolicy, CheckBoundariesPolicy>::CircularBuffer(size_t size) throw(std::bad_alloc)
	: _size(size) , _count(0), _nextPushPos(0), _nextPopPos(0), _elements(0)
{
	// allocate memory to hold object buffer (but don't initialize)
	std::pair<T*, ptrdiff_t> result(std::get_temporary_buffer<T>(_size));
	if(result.second == 0)
		throw std::bad_alloc();
	_elements = result.first;
}

template<class T, class GrowPolicy, class CheckBoundariesPolicy>
CircularBuffer<T, GrowPolicy, CheckBoundariesPolicy>::~CircularBuffer()
{
	clear();
	// free allocated memory (but don't uninitialize)
	std::return_temporary_buffer(_elements);
}

template<class T, class GrowPolicy, class CheckBoundariesPolicy>
size_t CircularBuffer<T, GrowPolicy, CheckBoundariesPolicy>::size() const
{
	return _size;
}

template<class T, class GrowPolicy, class CheckBoundariesPolicy>
size_t CircularBuffer<T, GrowPolicy, CheckBoundariesPolicy>::count() const
{
	return _count;
}

template<class T, class GrowPolicy, class CheckBoundariesPolicy>
bool CircularBuffer<T, GrowPolicy, CheckBoundariesPolicy>::isEmpty() const
{
	return _count == 0;
}

template<class T, class GrowPolicy, class CheckBoundariesPolicy>
bool CircularBuffer<T, GrowPolicy, CheckBoundariesPolicy>::isFull() const
{
	return _count == _size;
}

template<class T, class GrowPolicy, class CheckBoundariesPolicy>
void CircularBuffer<T, GrowPolicy, CheckBoundariesPolicy>::advance(size_t& currentPos)
{
	if(++currentPos == _size)
		currentPos = 0;
}

template<class T, class GrowPolicy, class CheckBoundariesPolicy>
void CircularBuffer<T, GrowPolicy, CheckBoundariesPolicy>::resize(size_t newSize) throw(std::bad_alloc)
{
	if(_size == newSize)
		return;

	// allocate new buffer
	std::pair<T*, ptrdiff_t> result(std::get_temporary_buffer<T>(newSize));
	if(result.second == 0)
		throw std::bad_alloc();

	T* newBuffer = result.first;
	size_t offset = newSize - _size;
	// copy all elements in the range [0 , nextPushPos)
	for(size_t i = 0; i < _nextPushPos; ++i)
		new (&newBuffer[i]) T(_elements[i]);
	// copy all elements in the range [nextPushPos, newSize) with proper offset
	for(size_t i = _nextPushPos; i < _size; ++i)
		new (&newBuffer[i + offset]) T(_elements[i]);

	_nextPopPos += offset;
	_size = newSize;
	// release old buffer
	std::return_temporary_buffer(_elements);
	_elements = newBuffer;
}

template<class T, class GrowPolicy, class CheckBoundariesPolicy>
void CircularBuffer<T, GrowPolicy, CheckBoundariesPolicy>::push(const T& elem) throw(std::exception)
{
	CheckBoundariesPolicy::checkFull(_count, _size);
	if(isFull())
		resize(GrowPolicy::newSize(_size));

	++_count;
	// construct type with previously allocated memory
	new (&_elements[_nextPushPos]) T(elem);

	advance(_nextPushPos);
}

template<class T, class GrowPolicy, class CheckBoundariesPolicy>
T CircularBuffer<T, GrowPolicy, CheckBoundariesPolicy>::pop() throw(std::runtime_error)
{
	CheckBoundariesPolicy::checkEmpty(_count);
	--_count;
	size_t popPos = _nextPopPos;
	advance(_nextPopPos);

	T elemCopy(_elements[popPos]);
	// destruct object
	_elements[popPos].~T();
	return elemCopy;
}

template<class T, class GrowPolicy, class CheckBoundariesPolicy>
T& CircularBuffer<T, GrowPolicy, CheckBoundariesPolicy>::operator[](size_t index) const
{
	return _elements[index];
}

template<class T, class GrowPolicy, class CheckBoundariesPolicy>
void CircularBuffer<T, GrowPolicy, CheckBoundariesPolicy>::clear()
{
	// destruct every object (but don't delete memory)
	for(;count() > 0;)
		pop();
}

////////////////////////////////////////////////////////////////
// Policy implementations
////////////////////////////////////////////////////////////////
// Boundaries-checking policies
class NoBoundaryCheck {
public:
	static void checkEmpty(size_t count) {}
	static void checkFull(size_t count, size_t size) {}
};

class CheckBoundaryThrow {
public:
	static void checkEmpty(size_t count) { if(count == 0) throw std::runtime_error("buffer empty"); }
	static void checkFull(size_t count, size_t size) { if(count == size) throw std::runtime_error("buffer full"); }
};

// Growing policies
class NoGrow {
public:
	static size_t newSize(size_t currentSize) { return currentSize; }
};

class GrowSingle {
public:
	static size_t newSize(size_t currentSize) { return currentSize + 1; }
};

class GrowDouble {
public:
	static size_t newSize(size_t currentSize) { return currentSize * 2; }
};

#endif // CIRCULAR_BUFFER_H
