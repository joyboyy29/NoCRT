#pragma once

namespace NoCRT {

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

    template<> struct is_integral_base<bool>                : true_type {};

    // Characters:

    template<> struct is_integral_base<char>                : true_type {};
    template<> struct is_integral_base<signed char>         : true_type {};
    template<> struct is_integral_base<unsigned char>       : true_type {};
    template<> struct is_integral_base<wchar_t>             : true_type {};
    template<> struct is_integral_base<char16_t>            : true_type {};
    template<> struct is_integral_base<char32_t>            : true_type {};

    // Integers:

    template<> struct is_integral_base<short>               : true_type {};
    template<> struct is_integral_base<unsigned short>      : true_type {};
    template<> struct is_integral_base<int>                 : true_type {};
    template<> struct is_integral_base<unsigned int>        : true_type {};
    template<> struct is_integral_base<long>                : true_type {};
    template<> struct is_integral_base<unsigned long>       : true_type {};
    template<> struct is_integral_base<long long>           : true_type {};
    template<> struct is_integral_base<unsigned long long>  : true_type {};

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