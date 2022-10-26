#include <iostream>
#include <unordered_map>
#include <utility>
#include <list>
#include <cassert>
#include <cstdint>

namespace ludaed {

    namespace detail {

        template<typename TValue>
        class storage {
            alignas(alignof(TValue)) uint8_t data[sizeof(TValue)];

        public:
            using value_type = TValue;

            template<typename ...Args>
            void construct(Args &&...args) {
                ::new(&data) TValue(std::forward<Args>(args)...);
            }

            void destruct() {
                reinterpret_cast<TValue *>(&data)->~TValue();
            }

            TValue &operator*() {
                return *reinterpret_cast<TValue *>(&data);
            }

            const TValue &operator*() const {
                return *reinterpret_cast<const TValue *>(&data);
            }
        };


        template<typename TValue, typename Allocator = std::allocator<TValue>>
        class array {
            static_assert(std::is_trivially_constructible_v<TValue>);

            template<typename TItem>
            class array_iterator;

        public:
            using allocator_type = Allocator;

            using value_type = TValue;
            using difference_type = std::ptrdiff_t;
            using reference = TValue &;
            using const_reference = const TValue &;
            using pointer = TValue *;
            using const_pointer = const TValue *;

            using iterator = array_iterator<TValue>;
            using const_iterator = array_iterator<const TValue>;

        private:
            using size_type = typename std::allocator_traits<allocator_type>::size_type;
            using allocator_traits = std::allocator_traits<allocator_type>;

            allocator_type allocator_;
            pointer data_;
            size_type size_;

        private:
            template<typename ...Args>
            static pointer _allocate_and_construct_data(allocator_type &allocator, size_type new_size, Args &&...args) {
                pointer new_data = allocator_traits::allocate(allocator, new_size);
                for (size_type i = 0; i < new_size; ++i) {
                    try {
                        allocator_traits::construct(allocator, new_data + i, std::forward<Args>(args)...);
                    } catch (...) {
                        for (size_type j = 0; j < i; ++j) {
                            allocator_traits::destroy(allocator, new_data + j);
                        }
                        allocator_traits::deallocate(allocator, new_data, new_size);
                        throw;
                    }
                }
                return new_data;
            }

            static void _deallocate_and_destroy_data(allocator_type &allocator, pointer data, size_type size) {
                for (size_type i = 0; i < size; ++i) {
                    allocator_traits::destroy(allocator, data + i);
                }
                allocator_traits::deallocate(allocator, data, size);
            }

        public:
            allocator_type get_allocator() const {
                return allocator_;
            }

            array()
                    :
                    data_(nullptr),
                    size_(0) {};

            explicit array(size_type size)
                    :
                    data_(nullptr),
                    size_(size) {
                data_ = _allocate_and_construct_data(allocator_, size_);
            }

            explicit array(const allocator_type &allocator)
                    :
                    data_(nullptr),
                    size_(0),
                    allocator_(allocator) {
            }

            array(size_type size, const allocator_type &allocator)
                    :
                    data_(nullptr),
                    size_(size),
                    allocator_(allocator) {
                data_ = _allocate_and_construct_data(allocator_, size_);
            }

            array(size_type size, const_reference default_value)
                    :
                    data_(nullptr),
                    size_(size) {
                data_ = _allocate_and_construct_data(allocator_, size_, default_value);
            }

            array(const array &other)
                    :
                    data_(nullptr),
                    size_(other.size_),
                    allocator_(allocator_traits::select_on_container_copy_construction(other.allocator_)) {
                data_ = allocator_traits::allocate(allocator_, size_);
                for (size_type i = 0; i < size_; ++i) {
                    try {
                        allocator_traits::construct(allocator_, data_ + i, other.data_[i]);
                    } catch (...) {
                        for (size_type j = 0; j < i; ++j) {
                            allocator_traits::destroy(allocator_, data_ + j);
                        }
                        allocator_traits::deallocate(allocator_, data_, size_);
                        throw;
                    }
                }
            }

            array(array &&other) noexcept
                    :
                    size_(other.size_),
                    data_(other.data_),
                    allocator_(std::move(other.allocator_)) {
                other.data_ = nullptr;
                other.size_ = 0;
            }

            ~array() {
                _deallocate_and_destroy_data(allocator_, data_, size_);
            }

            array &operator=(const array &other) {
                if (this == &other) {
                    return *this;
                }
                _deallocate_and_destroy_data(allocator_, data_, size_);
                if constexpr(allocator_traits::propagate_on_container_copy_assignment::value) {
                    if (allocator_ != other.allocator_) {
                        allocator_ = other.allocator_;
                    }
                }
                size_type new_size = other.size_;
                pointer new_data = allocator_traits::allocate(allocator_, new_size);
                for (size_type i = 0; i < new_size; ++i) {
                    try {
                        allocator_traits::construct(allocator_, new_data + i, data_[i]);
                    } catch (...) {
                        for (size_type j = 0; j < i; ++j) {
                            allocator_traits::destroy(allocator_, new_data + j);
                        }
                        allocator_traits::deallocate(allocator_, new_data, new_size);
                        throw;
                    }
                }
                size_ = new_size;
                data_ = new_data;
                return *this;
            }

            array &operator=(array &&other) noexcept {
                if (this == &other) {
                    return *this;
                }
                if constexpr (allocator_traits::propagate_on_container_move_assignment::value) {
                    _deallocate_and_destroy_data(allocator_, data_, size_);
                    if (allocator_ != other.allocator_) {
                        allocator_ = std::move(other.allocator_);
                    }
                    data_ = other.data_;
                    other.data_ = nullptr;
                    other.size_ = 0;
                } else {
                    if (allocator_ == other.allocator_) {
                        _deallocate_and_destroy_data(allocator_, data_, size_);
                        data_ = other.data_;
                        other.data_ = nullptr;
                        other.size_ = 0;
                    } else {
                        _deallocate_and_destroy_data(allocator_, data_, size_);
                        size_type new_size = other.size_;
                        pointer new_data = allocator_traits::allocate(allocator_, new_size);
                        for (size_type i = 0; i < new_size; ++i) {
                            try {
                                allocator_traits::construct(allocator_, new_data + i, std::move(other.data_[i]));
                            } catch (...) {
                                for (size_type j = 0; j < i; ++j) {
                                    allocator_traits::destroy(allocator_, new_data + j);
                                }
                                allocator_traits::deallocate(allocator_, new_data, new_size);
                                throw;
                            }
                        }
                        data_ = new_data;
                    }
                }
                size_ = other.size_;
                return *this;
            }

            void swap(array &other) {
                if (this == &other) {
                    return;
                }
                if constexpr(allocator_traits::propagate_on_container_swap::value) {
                    std::swap(allocator_, other.allocator_);
                    std::swap(data_, other.data_);
                    std::swap(size_, other.size_);
                } else {
                    if (allocator_ == other.allocator_) {
                        std::swap(data_, other.data_);
                        std::swap(size_, other.size_);
                    } else {
                        size_type first_size = other.size_;
                        pointer first_data = allocator_traits::allocate(allocator_, first_size);
                        for (size_type i = 0; i < first_size; ++i) {
                            try {
                                allocator_traits::construct(allocator_, first_data + i,
                                                            std::move_if_noexcept(other.data_[i]));
                            } catch (...) {
                                for (size_type j = 0; j < i; ++j) {
                                    allocator_traits::destroy(allocator_, first_data + j);
                                }
                                allocator_traits::deallocate(allocator_, first_data, first_size);
                                throw;
                            }
                        }

                        size_type second_size = other.size_;
                        pointer second_data = allocator_traits::allocate(other.allocator_, second_size);
                        for (size_type i = 0; i < second_size; ++i) {
                            try {
                                allocator_traits::construct(other.allocator_, second_data + i,
                                                            std::move_if_noexcept(other.data_[i]));
                            } catch (...) {
                                _deallocate_and_destroy_data(allocator_, first_data, first_size);
                                for (size_type j = 0; j < i; ++j) {
                                    allocator_traits::destroy(other.allocator_, second_data + j);
                                }
                                allocator_traits::deallocate(other.allocator_, second_data, second_size);
                                throw;
                            }
                        }
                        _deallocate_and_destroy_data(allocator_, data_, size_);
                        _deallocate_and_destroy_data(other.allocator_, other.data_, other.size_);

                        std::swap(data_, first_data);
                        std::swap(size_, first_size);
                        std::swap(other.data_, second_data);
                        std::swap(other.size_, second_size);
                    }
                }
            }

            void clear() {
                _deallocate_and_destroy_data(allocator_, data_, size_);
                size_ = 0;
            }

            void resize(size_type new_size) {
                resize(new_size, {});
            }

            void resize(size_type new_size, const_reference default_value) {
                if (size_ > new_size) {
                    pointer new_data = allocator_traits::allocate(allocator_, new_size);
                    for (size_type i = 0; i < new_size; ++i) {
                        try {
                            allocator_traits::construct(allocator_, new_data + i, std::move_if_noexcept(data_[i]));
                        } catch (...) {
                            for (size_type j = 0; j < i; ++j) {
                                allocator_traits::destroy(allocator_, new_data + j);
                            }
                            allocator_traits::deallocate(allocator_, new_data, new_size);
                            throw;
                        }
                    }
                    _deallocate_and_destroy_data(allocator_, data_, size_);
                    size_ = new_size;
                    data_ = new_data;
                } else if (size_ < new_size) {
                    pointer new_data = allocator_traits::allocate(allocator_, new_size);
                    for (size_type i = 0; i < size_; ++i) {
                        try {
                            allocator_traits::construct(allocator_, new_data + i, std::move_if_noexcept(data_[i]));
                        } catch (...) {
                            for (size_type j = 0; j < i; ++j) {
                                allocator_traits::destroy(allocator_, new_data + j);
                            }
                            allocator_traits::deallocate(allocator_, new_data, new_size);
                            throw;
                        }
                    }
                    for (size_type i = size_; i < new_size; ++i) {
                        allocator_traits::construct(allocator_, new_data + i, default_value);
                    }
                    _deallocate_and_destroy_data(allocator_, data_, size_);
                    size_ = new_size;
                    data_ = new_data;
                }
            }

            pointer data() noexcept {
                return data_;
            }

            template<typename ...Args>
            void emplace(size_type index, Args &&...args) {
                assert(index < size_);
                allocator_traits::construct(allocator_, data_ + index, std::forward<Args>(args)...);
            }

            template<typename ...Args>
            void emplace(iterator it, Args &&...args) {
                assert(it.data_ != nullptr);
                allocator_traits::construct(allocator_, it.data_, std::forward<Args>(args)...);
            }

            TValue &operator[](size_type index) {
                assert(index < size_);
                return data_[index];
            }

            const TValue &operator[](size_type index) const {
                assert(index < size_);
                return data_[index];
            }

            TValue &at(size_type index) {
                assert(index < size_);
                return data_[index];
            }

            const TValue &at(size_type index) const {
                assert(index < size_);
                return data_[index];
            }

            bool empty() const noexcept {
                return size_ == 0;
            }

            size_type size() const noexcept {
                return size_;
            }

            iterator begin() noexcept {
                return iterator(data_);
            }

            iterator end() noexcept {
                return iterator(data_ + size_);
            }

            const_iterator begin() const noexcept {
                return cbegin();
            }

            const_iterator end() const noexcept {
                return cend();
            }

            const_iterator cbegin() const noexcept {
                return const_iterator(data_);
            }

            const_iterator cend() const noexcept {
                return const_iterator(data_ + size_);
            }

        private:
            template<typename TItem>
            class array_iterator {
                friend class array;

            public:
                using iterator_category = std::random_access_iterator_tag;
                using value_type = TItem;
                using difference_type = std::ptrdiff_t;
                using reference = value_type &;
                using pointer = value_type *;

            private:
                pointer data_;

                explicit array_iterator(pointer data)
                        :
                        data_(data) {}

            public:
                array_iterator()
                        :
                        data_(nullptr) {}

                array_iterator(const array_iterator &other)
                        :
                        data_(other.data_) {}


                array_iterator &operator=(const array_iterator &other) {
                    data_ = other.data_;
                }

                array_iterator &operator=(pointer other) {
                    data_ = other;
                }

                reference operator*() const {
                    return *data_;
                }

                pointer operator->() const {
                    return data_;
                }

                reference operator[](difference_type index) const {
                    return data_[index];
                }

                bool operator==(const array_iterator &other) const {
                    return data_ == other.data_;
                }

                bool operator!=(const array_iterator &other) const {
                    return data_ != other.data_;
                }

                bool operator<(const array_iterator &other) const {
                    return data_ < other.data_;
                }

                bool operator<=(const array_iterator &other) const {
                    return data_ <= other.data_;
                }

                bool operator>(const array_iterator &other) const {
                    return data_ > other.data_;
                }

                bool operator>=(const array_iterator &other) const {
                    return data_ >= other.data_;
                }

                array_iterator &operator++() {
                    ++data_;
                    return *this;
                }

                array_iterator operator++(int) {
                    array_iterator result = *this;
                    ++data_;
                    return result;
                }

                array_iterator &operator--() {
                    --data_;
                    return *this;
                }

                array_iterator operator--(int) {
                    array_iterator result = *this;
                    --data_;
                    return result;
                }

                array_iterator &operator+=(difference_type difference) {
                    data_ += difference;
                    return *this;
                }

                array_iterator &operator-=(difference_type difference) {
                    data_ -= difference;
                    return *this;
                }

                array_iterator operator+(difference_type difference) const {
                    return array_iterator(data_ + difference);
                }

                array_iterator operator-(difference_type difference) const {
                    return array_iterator(data_ - difference);
                }

                difference_type operator+(const array_iterator &other) const {
                    return data_ + other.data_;
                }

                difference_type operator-(const array_iterator &other) const {
                    return data_ - other.data_;
                }

                friend array_iterator operator+(difference_type lhs, const array_iterator &rhs) {
                    return array_iterator(lhs + rhs.data_);
                }

                friend array_iterator operator-(difference_type lhs, const array_iterator &rhs) {
                    return array_iterator(lhs - rhs.data_);
                }
            };
        };

        static constexpr const size_t PRIMES[] = {
                1u,
                5u,
                17u,
                29u,
                37u,
                53u,
                67u,
                79u,
                97u,
                131u,
                193u,
                257u,
                389u,
                521u,
                769u,
                1031u,
                1543u,
                2053u,
                3079u,
                6151u,
                12289u,
                24593u,
                49157u
#if SIZE_MAX >= ULONG_MAX
                ,98317ul,
                196613ul,
                393241ul,
                786433ul,
                1572869ul,
                3145739ul,
                6291469ul,
                12582917ul,
                25165843ul,
                50331653ul,
                100663319ul,
                201326611ul,
                402653189ul,
                805306457ul,
                1610612741ul,
                3221225473ul,
                4294967291ul
#endif
#if SIZE_MAX >= ULLONG_MAX
                ,6442450939ull,
                12884901893ull,
                25769803751ull,
                51539607551ull,
                103079215111ull,
                206158430209ull,
                412316860441ull,
                824633720831ull,
                1649267441651ull,
                3298534883309ull,
                6597069766657ull,
#endif
        };

        class grow_power_of_two_policy {
        public:
            size_t operator()(size_t current) const {
                return current * 2;
            }
        };

        class grow_prime_policy {
        public:
            size_t operator()(size_t current) const {
                for (const auto &item: PRIMES) {
                    if (current < item) {
                        return item;
                    }
                }
                return current;
            }
        };

        template<typename TValue>
        class node {
        public:
            using hash_type = size_t;
            using value_type = TValue;
            using storage = storage<TValue>;

        private:
            static const uint8_t kNoEmptyMarker = 1;
            static const uint8_t kEmptyMarker = 0;
            static const hash_type kDefaultHash = 0;

            uint8_t empty_;
            hash_type hash_;
            storage value_;

        public:
            node()
                    :
                    empty_(kEmptyMarker),
                    hash_(kDefaultHash) {}

            node(const node &other) noexcept(std::is_nothrow_copy_constructible_v<value_type>)
                    :
                    empty_(other.empty_),
                    hash_(other.hash_) {
                if (!other.empty()) {
                    value_.construct(*other.value_);
                }
            }

            node(node &&other) noexcept(std::is_nothrow_move_constructible_v<value_type>)
                    :
                    empty_(other.empty_),
                    hash_(other.hash_) {
                if (!other.empty()) {
                    value_.construct(std::move(*other.value_));
                }
                other.clear();
            }

            ~node() {
                clear();
            }

            node &operator=(const node &other) {
                clear();
                if (!other.empty()) {
                    value_.construct(*other.value_);
                }
                hash_ = other.hash_;
                empty_ = other.empty_;
                return *this;
            }

            node &operator=(node &&other) {
                clear();
                if (!other.empty()) {
                    value_.construct(std::move(*other.value_));
                }
                hash_ = other.hash_;
                empty_ = other.empty_;
                other.clear();
                return *this;
            }

            template<typename ...Args>
            void set_data(hash_type hash, Args &&...args) {
                if (!empty()) {
                    clear();
                }
                value_.construct(std::forward<Args>(args)...);
                empty_ = kNoEmptyMarker;
                hash_ = hash;
            }

            void clear() {
                if (!empty()) {
                    value_.destruct();
                }
                empty_ = kEmptyMarker;
                hash_ = kDefaultHash;
            }

            void swap(node &other) {
                std::swap(*value_, *other.value_);
                std::swap(hash_, other.hash_);
                std::swap(empty_, other.empty_);
            }

            bool empty() const {
                return empty_ == kEmptyMarker;
            }

            hash_type hash() const {
                return hash_;
            }

            const value_type &value() const {
                return *value_;
            }

            value_type &value() {
                return *value_;
            }
        };

        template<class TValue,
                class KeySelector,
                class KeyHash,
                class KeyEqual,
                class GrowPolicy,
                class Allocator>
        class hash_table {
            template<typename TItem>
            class hash_table_iterator;

        public:
            using value_type = TValue;
            using difference_type = std::ptrdiff_t;
            using reference = TValue &;
            using const_reference = const TValue &;
            using pointer = TValue *;
            using const_pointer = const TValue *;

            using key_type = typename KeySelector::type;
            using key_equal = KeyEqual;
            using hasher = KeyHash;

            using key_selector = KeySelector;
            using grow_policy = GrowPolicy;

            using iterator = hash_table_iterator<TValue>;
            using const_iterator = hash_table_iterator<const TValue>;

            using allocator_type = Allocator;

        private:
            using node = node<TValue>;
            using node_allocator = typename std::allocator_traits<allocator_type>::template rebind_alloc<node>;
            using size_type = typename std::allocator_traits<node_allocator>::size_type;
            using array = array<node, node_allocator>;
            using node_pointer = typename array::pointer;

        private:
            key_selector key_selector_function_{};
            hasher key_hash_function_{};
            key_equal key_equal_function_{};
            grow_policy grow_policy_function_{};

            float load_factor_{0.5f};
            size_type size_{0};
            array data_;

        private:
            size_type _hash_to_index(size_t hash) const {
                return hash % data_.size();
            }

            size_type _distance_to_ideal_bucket(size_type index) {
                return index - _hash_to_index(data_[index].hash());
            }

            size_type _size_to_rehash() const {
                return load_factor_ * data_.size();
            }

            size_type _next_capacity(size_type needed_capacity) {
                size_type current_capacity = data_.size();
                while (needed_capacity >= current_capacity) {
                    current_capacity = grow_policy_function_(current_capacity);
                }
                return current_capacity;
            }

            void _shift_up(size_type index) {
                size_type distance = 0;
                node insertion_node(std::move(data_[index]));
                while (!insertion_node.empty()) {
                    if (_distance_to_ideal_bucket(index) < distance) {
                        data_[index].swap(insertion_node);
                        distance = _distance_to_ideal_bucket(index);
                    }
                    distance++;
                    index++;
                }
                data_[index].swap(insertion_node);
            };

            void _shift_down(size_type index) {
                while (index + 1 < data_.size() &&
                       !data_[index + 1].empty() &&
                       _distance_to_ideal_bucket(index + 1) != 0) {
                    data_[index] = std::move(data_[index + 1]);
                    index++;
                }
            }

            void _rehash(size_type new_capacity) {
                if (new_capacity > data_.size()) {
                    hash_table rehashing_table(new_capacity,
                                               load_factor_,
                                               data_.get_allocator(),
                                               key_hash_function_,
                                               key_equal_function_);

                    for (const auto &item: data_) {
                        if (!item.empty()) {
                            rehashing_table._insert(std::move(item));
                        }
                    }
                    rehashing_table.swap(*this);
                }
            }

            bool _try_to_rehash() {
                if (size_ < _size_to_rehash()) {
                    return false;
                }
                _rehash(grow_policy_function_(data_.size()));
                return true;
            }

        protected:
            size_type _find_index(const key_type &key, size_t hash) {
                size_type index = _hash_to_index(hash);
                size_type distance = 0;

                while (index < data_.size()) {
                    if (data_[index].empty() ||
                        distance > _distance_to_ideal_bucket(index)) {
                        return data_.size();
                    }
                    if (data_[index].hash() == hash &&
                        key_equal_function_(data_[index], key)) {
                        return index;
                    }
                    index++;
                    distance++;
                }
                return data_.size();
            }

            size_type _find_index(const key_type &key) {
                size_t hash = key_hash_function_(key);
                return _find_index(key, hash);
            }

            size_type _erase(const key_type &key) {
                size_type index = _find_index(key);

                if (index != data_.size()) {
                    _shift_down(index);
                    --size_;
                    return 1;
                }
                return 0;
            }

            void _insert(node &&insertion_node) {
                const key_type &key = key_selector_function_(insertion_node.value());
                size_type index = _hash_to_index(insertion_node.hash());

                if (!data_[index].empty()) {
                    _shift_up(index);
                }
                data_[index] = std::move(insertion_node);
                size_++;
            }

            std::pair<iterator, bool> _insert(const value_type &value) {
                const key_type &key = key_selector_function_(value);
                return _insert(key, value);
            }

            std::pair<iterator, bool> _insert(value_type &&value) {
                const key_type &key = key_selector_function_(value);
                return _insert(key, std::move(value));
            }

            template<typename ...Args>
            std::pair<iterator, bool> _insert(const key_type &key, Args &&... args) {
                size_t hash = key_hash_function_(key);
                bool has_key = true;
                size_type index = _hash_to_index(hash);
                size_type insertion_index = _find_index(key, hash);

                if (insertion_index == data_.size()) {
                    if (_try_to_rehash()) {
                        index = _hash_to_index(hash);
                    }
                    if (!data_[index].empty()) {
                        _shift_up(index);
                    }
                    has_key = false;
                }

                data_[index].set_data(hash, std::forward<Args>(args)...);
                size_++;

                node_pointer first = data_.data();
                node_pointer last = data_.data() + data_.size();

                return std::make_pair(iterator(&data_[index], first, last), has_key);
            }

        public:
            allocator_type get_allocator() const {
                return data_.get_allocator();
            }

            hash_table() = default;

            explicit hash_table(size_type capacity,
                                float load_factor = 0.5f,
                                const allocator_type &allocator = allocator_type{},
                                const hasher &key_hash_function = hasher{},
                                const key_equal &key_equal_function = key_equal{})
                    :
                    data_(capacity, allocator),
                    size_(0),
                    load_factor_(load_factor),
                    key_hash_function_(key_hash_function),
                    key_equal_function_(key_equal_function),
                    key_selector_function_(),
                    grow_policy_function_() {
            }

            template<typename InputIt>
            hash_table(InputIt begin, InputIt end,
                       float load_factor = 0.5f,
                       const allocator_type &allocator = allocator_type{},
                       const hasher &key_hash_function = hasher{},
                       const key_equal &key_equal_function = key_equal{})
                    :
                    data_(allocator),
                    size_(0),
                    load_factor_(load_factor),
                    key_hash_function_(key_hash_function),
                    key_equal_function_(key_equal_function),
                    key_selector_function_(),
                    grow_policy_function_() {
                insert(begin, end);
            }

            hash_table(std::initializer_list<value_type> list,
                       float load_factor = 0.5f,
                       const allocator_type &allocator = allocator_type{},
                       const hasher &key_hash_function = hasher{},
                       const key_equal &key_equal_function = key_equal{})
                    :
                    data_(allocator),
                    size_(0),
                    load_factor_(load_factor),
                    key_hash_function_(key_hash_function),
                    key_equal_function_(key_equal_function),
                    key_selector_function_(),
                    grow_policy_function_() {
                insert(list.begin(), list.end());
            }

            hash_table(const hash_table &other)
                    :
                    data_(other.data_),
                    size_(other.size_),
                    load_factor_(other.load_factor_),
                    key_hash_function_(other.key_hash_function_),
                    key_equal_function_(other.key_equal_function_),
                    key_selector_function_(other.key_selector_function_),
                    grow_policy_function_(other.grow_policy_function_) {}

            hash_table(hash_table &&other) noexcept(
            std::is_nothrow_move_constructible<hasher>::value &&
            std::is_nothrow_move_constructible<key_equal>::value &&
            std::is_nothrow_move_constructible<grow_policy>::value &&
            std::is_nothrow_move_constructible<array>::value)
                    :
                    data_(std::move(other.data_)),
                    size_(other.size_),
                    load_factor_(other.load_factor_),
                    key_hash_function_(std::move(other.key_hash_function_)),
                    key_equal_function_(std::move(other.key_equal_function_)),
                    key_selector_function_(std::move(other.key_selector_function_)),
                    grow_policy_function_(std::move(other.grow_policy_function_)) {
                other.clear();
            }

            hash_table &operator=(const hash_table &other) {
                if (this == &other) {
                    return *this;
                }
                data_ = other.data_;
                size_ = other.size_;
                load_factor_ = other.load_factor_;
                key_hash_function_ = other.key_hash_function_;
                key_equal_function_ = other.key_equal_function_;
                key_selector_function_ = other.key_selector_function_;
                grow_policy_function_ = other.grow_policy_function_;
                return *this;
            }

            hash_table &operator=(hash_table &&other) noexcept {
                if (this == &other) {
                    return *this;
                }
                data_ = std::move(other.data_);
                size_ = other.size_;
                load_factor_ = other.load_factor_;
                key_hash_function_ = std::move(other.key_hash_function_);
                key_equal_function_ = std::move(other.key_equal_function_);
                key_selector_function_ = std::move(other.key_selector_function_);
                grow_policy_function_ = std::move(other.grow_policy_function_);
                other.clear();
                return *this;
            }

            void reserve(size_type new_capacity) {
                size_type next_capacity = _next_capacity(new_capacity);
                if (new_capacity > data_.size()) {
                    _rehash(next_capacity);
                }
            }

            std::pair<iterator, bool> insert(const value_type &value) {
                return _insert(value);
            }

            std::pair<iterator, bool> insert(value_type &&value) {
                return _insert(value);
            }

            iterator insert(const_iterator hint, const value_type &value) {
                (void) hint;
                return _insert(value).first();
            }

            iterator insert(const_iterator hint, value_type &&value) {
                (void) hint;
                return _insert(value).first();
            }

            template<typename InputIt>
            void insert(InputIt begin, InputIt end) {
                for (; begin != end; ++begin) {
                    _insert(value_type(*begin));
                }
            }

            void insert(std::initializer_list<value_type> list) {
                for (auto it = list.begin(); it != list.end(); ++it) {
                    _insert(std::move(*it));
                }
            }

            template<typename ...Args>
            std::pair<iterator, bool> emplace(Args ...args) {
                return _insert(value_type(std::forward<Args>(args)...));
            }

            template<typename ...Args>
            iterator emplace_hint(const_iterator hint, Args ...args) {
                (void) hint;
                return _insert(value_type(std::forward<Args>(args)...)).first();
            }

            iterator erase(iterator position) {
                if (position == end()) {
                    return end();
                }
                _erase(key_selector_function_(*position));
                if (position.data_->empty()) {
                    ++position;
                }
                return position;
            }

            iterator erase(const_iterator position) {
                return erase(mutable_iterator(position));
            }

            iterator erase(const_iterator begin, const_iterator end) {
                // TODO: Can be faster
                for (; begin != end; ++begin) {
                    erase(begin);
                }
            }

            size_type erase(const key_type &key) {
                return _erase(key);
            }

            size_type count(const key_type &key) const {
                size_type index = _find_index(key);
                if (index != data_.size()) {
                    return 1;
                }
                return 0;
            }

            iterator find(const key_type &key) {
                return mutable_iterator(static_cast<const hash_table *>(this)->find(key));
            }

            const_iterator find(const key_type &key) const {
                size_type index = _find_index(key);
                if (index == data_.size()) {
                    return end();
                }
                node_pointer first = data_.data();
                node_pointer last = data_.data() + data_.size();
                return const_iterator(&data_[index], first, last);
            }

            //TODO: Two more methods to find

            bool contains(const key_type &key) const {
                return count(key) == 1;
            }

            std::pair<iterator, iterator> equal_range(const key_type &key) {
                iterator founded = find(key);
                return std::make_pair(founded, std::next(founded));
            }

            std::pair<const_iterator, const_iterator> equal_range(const key_type &key) const {
                const_iterator founded = find(key);
                return std::make_pair(founded, std::next(founded));
            }

            //TODO: Two more methods to equal_range

            size_type bucket_count() const {
                return size_;
            }

            size_type max_bucket_count() const {
                return data_.size();
            }

            float load_factor() const {
                return static_cast<float>(size_) / static_cast<float>(data_.size());
            }

            float max_load_factor() const {
                return load_factor_;
            }

            void max_load_factor(float load_factor) {
                load_factor_ = std::min(1.f, load_factor);
            }

            void rehash(size_type new_capacity) {
                reserve(new_capacity);
            }

            hasher hash_function() const {
                return key_hash_function_;
            }

            key_equal key_eq() const {
                return key_equal_function_;
            }

            bool operator==(const hash_table &other) const {
                if (other.size() != size()) {
                    return false;
                }
                for (auto it = other.begin(); it != other.end(); ++it) {
                    if (!contains(other.key_selector_function_(*it))) {
                        return false;
                    }
                }
                return true;
            }

            bool operator!=(const hash_table &other) const {
                return !this->operator==(other);
            }

            void clear() {
                data_.clear();
                size_ = 0;
            }

            void swap(hash_table &other) {
                std::swap(key_selector_function_, other.key_selector_function_);
                std::swap(key_hash_function_, other.key_hash_function_);
                std::swap(key_equal_function_, other.key_equal_function_);
                std::swap(grow_policy_function_, other.grow_policy_function_);

                std::swap(load_factor_, other.load_factor_);
                std::swap(size_, other.size_);
                std::swap(data_, other.data_);
            }

            bool empty() const {
                return size_ == 0;
            }

            size_type size() const {
                return size_;
            }

            iterator mutable_iterator(const_iterator position) const {
                node_pointer first = data_.data();
                node_pointer last = data_.data() + data_.size();
                return iterator(const_cast<node_pointer>(position.data_), first, last);
            }

            iterator begin() noexcept {
                node_pointer first = data_.data();
                node_pointer last = data_.data() + data_.size();
                return iterator(first, first, last);
            }

            iterator end() noexcept {
                node_pointer first = data_.data();
                node_pointer last = data_.data() + data_.size();
                return iterator(last, first, last);
            }

            iterator begin() const noexcept {
                return cbegin();
            }

            iterator end() const noexcept {
                return cend();
            }

            const_iterator cbegin() const noexcept {
                node_pointer first = data_.data();
                node_pointer last = data_.data() + data_.size();
                return const_iterator(first, first, last);
            }

            const_iterator cend() const noexcept {
                node_pointer first = data_.data();
                node_pointer last = data_.data() + data_.size();
                return const_iterator(last, first, last);
            }

            iterator rbegin() noexcept {
                node_pointer first = data_.data();
                node_pointer last = data_.data() + data_.size();
                return iterator(last - 1, first, last);
            }

            iterator rend() noexcept {
                node_pointer first = data_.data();
                node_pointer last = data_.data() + data_.size();
                return iterator(first - 1, first, last);
            }

            const_iterator rbegin() const noexcept {
                node_pointer first = data_.data();
                node_pointer last = data_.data() + data_.size();
                return const_iterator(last - 1, first, last);
            }

            const_iterator rend() const noexcept {
                node_pointer first = data_.data();
                node_pointer last = data_.data() + data_.size();
                return const_iterator(first - 1, first, last);
            }

        private:
            template<typename TItem>
            class hash_table_iterator {
                friend class hash_table;

            public:
                using iterator_category = std::bidirectional_iterator_tag;
                using value_type = TItem;
                using difference_type = std::ptrdiff_t;
                using reference = value_type &;
                using pointer = value_type *;

            private:
                node_pointer first_;
                node_pointer last_;
                node_pointer data_;

                explicit hash_table_iterator(node_pointer data, node_pointer first, node_pointer last)
                        :
                        first_(first),
                        last_(last),
                        data_(data) {}

            public:
                hash_table_iterator()
                        :
                        first_(nullptr),
                        last_(nullptr),
                        data_(nullptr) {}

                hash_table_iterator(const hash_table_iterator &other)
                        :
                        first_(other.first_),
                        last_(other.last_),
                        data_(other.data_) {}


                hash_table_iterator &operator=(const hash_table_iterator &other) {
                    first_ = other.first_;
                    last_ = other.last_;
                    data_ = other.data_;
                }

            public:
                reference operator*() const {
                    return data_->value();
                }

                pointer operator->() const {
                    return &data_->value();
                }

                bool operator==(const hash_table_iterator &other) const {
                    return first_ == other.first_ && last_ == other.last_ && data_ == other.data_;
                }

                bool operator!=(const hash_table_iterator &other) const {
                    return first_ != other.first_ || last_ != other.last_ || data_ != other.data_;
                }

                hash_table_iterator &operator++() {
                    go_next();
                    return *this;
                }

                hash_table_iterator operator++(int) {
                    hash_table_iterator result = *this;
                    go_next();
                    return result;
                }

                hash_table_iterator &operator--() {
                    go_prior();
                    return *this;
                }

                hash_table_iterator operator--(int) {
                    hash_table_iterator result = *this;
                    go_prior();
                    return result;
                }

            private:
                void go_next() {
                    while (true) {
                        data_++;
                        if (data_ == last_ || !data_->empty()) {
                            return;
                        }
                    }
                }

                void go_prior() {
                    while (true) {
                        data_--;
                        if (data_ == first_ - 1 || !data_->empty()) {
                            return;
                        }
                    }
                }
            };
        };
    }
}

struct A {
    std::string a;

    explicit A(std::string a) : a(std::move(a)) {}


//    A &operator=(const A &other) = default;

};

std::ostream &operator<<(std::ostream &stream, A &data) {
    stream << data.a;
    return stream;
}

int main() {
    static_assert(std::random_access_iterator<ludaed::detail::array<int>::const_iterator>);
    {
        std::cout << SIZE_MAX << " " << ULONG_MAX << std::endl;
        ludaed::detail::node<A> node1;
        node1.set_data(1, "11111");
        ludaed::detail::node<A> node2(node1);
        node2.set_data(1, "22222");
        node1.swap(node2);
        std::cout << node1.value() << " " << node2.value();

        ludaed::detail::array<int> array;
        array.resize(3);
        for (auto &item: array) {
            item = 10;
        }
        array.resize(5, 9);
        for (const auto &item: array) {
            std::cout << item << " ";
        }
        array.resize(100, 9);
        for (const auto &item: array) {
            std::cout << item << " ";
        }

        ludaed::detail::array<int> a;

        std::swap(array, a);
        array.swap(a);
    }
    return 0;
}