#pragma once

#include <intrin.h>

typedef unsigned char uint8_t;

namespace NoCRT {

    class BaseSpinLock {
    private:
        volatile long _lock;

    public:
        BaseSpinLock() : _lock(0) {}

        void lock() {
            while (_InterlockedCompareExchange(&_lock, 1, 0) != 0) {
                _mm_pause();
            }
        }

        void unlock() {
            _InterlockedExchange(&_lock, 0);
        }
    };

    class RLock {
    private:
        BaseSpinLock _lock;
        volatile const char* _ownerThreadId;
        volatile long _depth;

    public:
        RLock() : _ownerThreadId(0), _depth(0) {}

        void lock() {
            const char* currentThreadId = reinterpret_cast<const char*>(__readgsqword(0x30)); // 0x30 -> TEB

            if (_ownerThreadId == currentThreadId) {
                _depth++;
            }
            else {
                _lock.lock();
                _ownerThreadId = currentThreadId;
                _depth = 1;
            }
        }

        void unlock() {
            const char* currentThreadId = reinterpret_cast<const char*>(__readgsqword(0x30)); // 0x30 -> TEB

            if (_ownerThreadId == currentThreadId) {
                if (--_depth == 0) {
                    _ownerThreadId = 0;
                    _lock.unlock();
                }
            }
        }
    };

    template <typename T, size_t N>
    class NoArray {
    private:
        T _data[N];
        static T _errorValue;

    public:
        class Iterator {
        private:
            T* ptr;

        public:
            Iterator(T* p) : ptr(p) {}

            Iterator& operator++() {
                ptr++;
                return *this;
            }

            Iterator operator++(int) {
                Iterator temp = *this;
                (*this)++;
                return temp;
            }

            T& operator*() const {
                return *ptr;
            }

            T* operator->() {
                return ptr;
            }

            bool operator==(const Iterator& other) const {
                return ptr == other.ptr;
            }

            bool operator!=(const Iterator& other) const {
                return ptr != other.ptr;
            }
        };

        NoArray() : _data{} {}

        Iterator begin() {
            return Iterator(_data);
        }

        Iterator end() {
            return Iterator(_data + N);
        }

        T& at(size_t index) {
            if (index >= N)
                return _errorValue;
            return _data[index];
        }

        const T& at(size_t index) const {
            if (index >= N)
                return _errorValue;
            return _data[index];
        }

        constexpr bool empty() const { return N == 0; }

        constexpr size_t size() const { return N; }

        T& operator[](size_t index) {
            return _data[index];
        }

        const T& operator[](size_t index) const {
            return _data[index];
        }

        T& front() { return _data[0]; }

        const T& front() const { return _data[0]; }

        T& back() { return _data[N - 1]; }

        const T& back() const { return _data[N - 1]; }

        void fill(const T& value) {
            for (size_t i = 0; i < N; i++) {
                _data[i] = value;
            }
        }

        T* data() { return _data; }

        const T* data() const { return _data; }
    };

    template<typename T, size_t N>
    T NoArray<T, N>::_errorValue = T{};

    class NoAlloc {
    private:
        struct BlockHeader {
            size_t          size;
            bool            allocated;
            BlockHeader* next;
        };

        BlockHeader* _firstBlock;
        RLock               _lock;
        size_t              _totalHeapSize;
        uint8_t* _heapBase;  // start
        uint8_t* _heapLimit; // end + 1

        void memcpy(void* dest, const void* src, size_t n) {
            char* d = reinterpret_cast<char*>(dest);
            const char* s = reinterpret_cast<const char*>(src);
            while (n--) {
                *d++ = *s++;
            }
        }

        void memset(void* ptr, int val, size_t n) {
            unsigned char* p = reinterpret_cast<unsigned char*>(ptr);
            while (n-- > 0) {
                *p++ = static_cast<unsigned char>(val);
            }
        }

        void memmove(void* dest, const void* src, size_t n) {
            char* d = reinterpret_cast<char*>(dest);
            const char* s = reinterpret_cast<const char*>(src);

            if (d < s) {
                while (n--) {
                    *d++ = *s++;
                }
            }
            else {
                d += n;
                s += n;
                while (n--) {
                    *--d = *--s;
                }
            }
        }

        int memcmp(const void* s1, const void* s2, size_t n) {
            const unsigned char* p1 = reinterpret_cast<const unsigned char*>(s1);
            const unsigned char* p2 = reinterpret_cast<const unsigned char*>(s2);
            while (n-- > 0) {
                if (*p1 != *p2) {
                    return *p1 - *p2;
                }
                p1++;
                p2++;
            }
            return 0;
        }


        void calcStats(size_t& totalAllocated, size_t& totalFree, size_t& largestFreeBlock, size_t& freeBlocksCount) const {
            totalAllocated = 0;
            totalFree = 0;
            largestFreeBlock = 0;
            freeBlocksCount = 0;

            BlockHeader* current = _firstBlock;
            while (current != nullptr) {
                if (current->allocated) {
                    totalAllocated += current->size;
                }
                else {
                    totalFree += current->size;
                    ++freeBlocksCount;
                    if (current->size > largestFreeBlock) {
                        largestFreeBlock = current->size;
                    }
                }
                current = current->next;
            }
        }

        bool isValidHeapPtr(void* ptr) const {
            return ptr >= _heapBase && ptr < _heapLimit;
        }

    public:
        NoAlloc(size_t heapSize) : _totalHeapSize(heapSize) {
            static uint8_t heap[1024 * 1024];
            _heapBase = heap;
            _heapLimit = heap + sizeof(heap);
            _firstBlock = reinterpret_cast<BlockHeader*>(heap);
            _firstBlock->size = heapSize - sizeof(BlockHeader);
            _firstBlock->allocated = false;
            _firstBlock->next = nullptr;
        }

        // maybe using buddy system for better performance?
        void* malloc(size_t size) {
            _lock.lock();
            BlockHeader* current = _firstBlock;
            while (current != nullptr) {
                if (!current->allocated && current->size >= size + sizeof(BlockHeader)) {
                    size_t remainingSize = current->size - size - sizeof(BlockHeader);

                    current->allocated = true;
                    current->size = size;

                    if (remainingSize > sizeof(BlockHeader)) {
                        BlockHeader* newBlock = reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(current + 1) + size);
                        newBlock->size = remainingSize;
                        newBlock->allocated = false;
                        newBlock->next = current->next;
                        current->next = newBlock;
                    }
                    _lock.unlock();

                    merge();
                    return reinterpret_cast<void*>(current + 1);
                }
                current = current->next;
            }
            _lock.unlock();
            return nullptr;
        }

        void free(void* ptr) {
            if (ptr == nullptr) {
                return;
            }

            _lock.lock();

            // free ptr out of bounds
            if (!isValidHeapPtr(ptr)) {
                _lock.unlock();
                return;
            }

            // free already freed ptr
            BlockHeader* header = reinterpret_cast<BlockHeader*>(ptr) - 1;
            if (!header->allocated) {
                _lock.unlock();
                return;
            }

            header->allocated = false;

            merge();

            _lock.unlock();
        }

        void* realloc(void* ptr, size_t newSize) {
            if (ptr == nullptr) {
                return malloc(newSize);
            }

            BlockHeader* header = reinterpret_cast<BlockHeader*>(ptr) - 1;
            if (header->size >= newSize) {
                return ptr;
            }

            void* newPtr = malloc(newSize);
            if (newPtr != nullptr) {
                memcpy(newPtr, ptr, header->size);
                free(ptr);
            }
            return newPtr;
        }

        void* calloc(size_t numElements, size_t elementSize) {
            _lock.lock();
            size_t totalSize = numElements * elementSize;
            void* ptr = malloc(totalSize);
            if (ptr != nullptr) {
                memset(ptr, 0, totalSize);
            }
            _lock.unlock();
            return ptr;
        }

        void merge() {
            BlockHeader* current = _firstBlock;
            while (current != nullptr && current->next != nullptr) {
                if (!current->allocated && !current->next->allocated) {
                    current->size += current->next->size + sizeof(BlockHeader);
                    current->next = current->next->next;
                }
                else {
                    current = current->next;
                }
            }
        }

        void printStats() const {
            size_t totalAllocated, totalFree, largestFreeBlock, freeBlocksCount;
            calcStats(totalAllocated, totalFree, largestFreeBlock, freeBlocksCount);

            // Test Purposes:

            //std::cout << "Heap Allocator Statistics:\n";
            //std::cout << "Total Heap Size: " << _totalHeapSize << " bytes\n";
            //std::cout << "Total Allocated: " << totalAllocated << " bytes\n";
            //std::cout << "Total Free: " << totalFree << " bytes\n";
            //std::cout << "Number of Free Blocks: " << freeBlocksCount << "\n";
            //std::cout << "Largest Free Block: " << largestFreeBlock << " bytes\n";
        }

    };

    template<typename T> struct remove_reference;

    template<typename T> struct remove_reference<T&> {
        typedef typename remove_reference<T>::type type;
    };

    template<typename T> struct remove_reference {
        typedef T type;
    };

    template<typename T> struct remove_const;

    template<typename T> struct remove_const<const T> {
        typedef typename remove_const<T>::type type;
    };

    template<typename T> struct remove_const {
        typedef T type;
    };

    template<typename T> struct remove_volatile;

    template<typename T> struct remove_volatile<volatile T> {
        typedef typename remove_volatile<T>::type type;
    };

    template<typename T> struct remove_volatile {
        typedef T type;
    };

    template<typename T>
    struct decay {
    private:
        typedef typename remove_reference<T>::type no_ref;
        typedef typename remove_const<no_ref>::type no_const;
        typedef typename remove_volatile<no_const>::type type;

    public:
        typedef type type;
    };

    // std::true_type and std::false_type impl

    struct true_type {
        static constexpr bool value = true;
        typedef bool value_type;
        typedef true_type type;
        constexpr operator bool() const noexcept { return value; }
    };

    struct false_type {
        static constexpr bool value = false;
        typedef bool value_type;
        typedef false_type type;
        constexpr operator bool() const noexcept { return value; }
    };

    // base template

    template<typename T> struct is_integral_base : false_type {};

    // specializations -> 15 total integral types

    // Boolean:

    template<> struct is_integral_base<bool> : true_type {};

    // Characters:

    template<> struct is_integral_base<char> : true_type {};
    template<> struct is_integral_base<signed char> : true_type {};
    template<> struct is_integral_base<unsigned char> : true_type {};
    template<> struct is_integral_base<wchar_t> : true_type {};
    template<> struct is_integral_base<char16_t> : true_type {};
    template<> struct is_integral_base<char32_t> : true_type {};

    // Integers:

    template<> struct is_integral_base<short> : true_type {};
    template<> struct is_integral_base<unsigned short> : true_type {};
    template<> struct is_integral_base<int> : true_type {};
    template<> struct is_integral_base<unsigned int> : true_type {};
    template<> struct is_integral_base<long> : true_type {};
    template<> struct is_integral_base<unsigned long> : true_type {};
    template<> struct is_integral_base<long long> : true_type {};
    template<> struct is_integral_base<unsigned long long> : true_type {};

    // Custom one just in case for some other type

    template<typename T>
    struct is_integral : is_integral_base<typename decay<T>::type> {};

    // is_same impl

    template<typename T, typename U> struct is_same : false_type {};

    template<typename T> struct is_same<T, T> : true_type {};

    // is_same_v impl

    template<typename T, typename U> constexpr bool is_same_v = is_same<T, U>::value; //constexprs are implicitly inline

    // is_array impl

    template<typename T> struct is_array : false_type {};

    template<typename T, size_t N> struct is_array<NoArray<T, N>> : true_type {};

    // is_void impl

    template<typename T> struct is_void : is_same<typename decay<T>::type, void> {};

    // enable_if impl   

    template<bool cond, typename T = void> struct enable_if {};

    template<typename T> struct enable_if<true, T> { typedef T type; };

    template<bool cond, typename T = void> struct enable_if_t { typedef typename enable_if<cond, T>::type type; };

    // std::move and std::forward impl

    template<typename T>
    constexpr typename remove_reference<T>::type&& move(T&& arg) noexcept {
        return static_cast<typename remove_reference<T>::type&&>(arg);
    }

    template<typename T>
    constexpr T&& forward(typename remove_reference<T>::type& arg) noexcept {
        return static_cast<T&&>(arg);
    }
}