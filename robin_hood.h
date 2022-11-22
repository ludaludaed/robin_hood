#ifndef HASHMAP_ROBIN_HOOD_H
#define HASHMAP_ROBIN_HOOD_H

#include <utility>
#include <cassert>
#include <cstdint>

namespace ld {

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

        template<typename TValue, typename Allocator = std::allocator<TValue>>
        class array {
            static_assert(std::is_default_constructible<TValue>::value);

            template<typename TItem>
            class array_iterator;

            using allocator_traits = std::allocator_traits<Allocator>;

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
            using size_type = typename std::allocator_traits<allocator_type>::size_type;

        private:
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

            array(const array &other, const allocator_type &allocator)
                    :
                    data_(nullptr),
                    size_(other.size_),
                    allocator_(allocator) {
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

            array(array &&other, const allocator_type &allocator)
            noexcept(std::is_nothrow_move_constructible<TValue>::value)
                    :
                    size_(0),
                    data_(nullptr),
                    allocator_(allocator) {
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
                other.clear();
                size_ = new_size;
                data_ = new_data;
            }

            ~array() {
                _deallocate_and_destroy_data(allocator_, data_, size_);
            }

            array &operator=(const array &other) {
                if (this == &other) {
                    return *this;
                }
                _deallocate_and_destroy_data(allocator_, data_, size_);
                if constexpr (allocator_traits::propagate_on_container_copy_assignment::value) {
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
                    size_ = other.size_;
                    other.data_ = nullptr;
                    other.size_ = 0;
                } else {
                    if (allocator_ == other.allocator_) {
                        _deallocate_and_destroy_data(allocator_, data_, size_);
                        data_ = other.data_;
                        size_ = other.size_;
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
                        size_ = other.size_;
                        other.clear();
                        return *this;
                    }
                }
                return *this;
            }

            void swap(array &other) {
                if (this == &other) {
                    return;
                }
                if constexpr (allocator_traits::propagate_on_container_swap::value) {
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
                data_ = nullptr;
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

            const_pointer data() const noexcept {
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

            reference operator[](size_type index) {
                assert(index < size_);
                return data_[index];
            }

            const_reference operator[](size_type index) const {
                assert(index < size_);
                return data_[index];
            }

            reference at(size_type index) {
                assert(index < size_);
                return data_[index];
            }

            const_reference at(size_type index) const {
                assert(index < size_);
                return data_[index];
            }

            allocator_type get_allocator() const {
                return allocator_;
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

            template<typename ...Args>
            explicit node(hash_type hash, Args &&...args)
                    :
                    hash_(hash),
                    empty_(kNoEmptyMarker) {
                value_.construct(std::forward<Args>(args)...);
            }

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

            node &operator=(node &&other) noexcept(std::is_nothrow_move_constructible<value_type>::value) {
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
                node temp = *this;
                *this = other;
                other = temp;
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
                class Allocator,
                class GrowthPolicy>
        class hash_table {
            template<typename TItem>
            class hash_table_iterator;

            using node = node<TValue>;
            using node_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<node>;
            using array = array<node, node_allocator>;
            using node_pointer = typename array::pointer;

            static constexpr const float kDefaultLoadFactor = 0.5f;

        public:
            using value_type = TValue;
            using difference_type = std::ptrdiff_t;
            using reference = TValue &;
            using const_reference = const TValue &;
            using pointer = TValue *;
            using const_pointer = const TValue *;

            using key_type = typename KeySelector::key_type;
            using key_equal = KeyEqual;
            using hasher = KeyHash;

            using key_selector = KeySelector;
            using growth_policy = GrowthPolicy;

            using iterator = hash_table_iterator<TValue>;
            using const_iterator = hash_table_iterator<const TValue>;

            using allocator_type = Allocator;

            using size_type = typename array::size_type;

        private:
            key_selector key_selector_function_{};
            hasher key_hash_function_{};
            key_equal key_equal_function_{};
            growth_policy growth_policy_function_{};

            float load_factor_{kDefaultLoadFactor};
            size_type size_{0};
            array data_;

        private:
            size_type _next_index(size_type index) const {
                return (index + 1) % data_.size();
            }

            size_type _hash_to_index(size_t hash) const {
                return hash % std::max(data_.size(), size_type(1));
            }

            size_type _distance_to_ideal_bucket(size_type index) const {
                size_type hashed_index = _hash_to_index(data_[index].hash());

                if (hashed_index > index) {
                    return data_.size() - hashed_index + index;
                } else {
                    return index - hashed_index;
                }
            }

            size_type _next_capacity(size_type needed_capacity) const {
                size_type current_capacity = data_.size();

                while (needed_capacity >= current_capacity) {
                    current_capacity = growth_policy_function_(current_capacity);
                }
                return current_capacity;
            }

            size_type _size_to_rehash() const {
                return load_factor_ * data_.size();
            }

            void _rehash(size_type new_capacity) {
                if (new_capacity > data_.size()) {
                    hash_table rehashing_table(new_capacity,
                                               key_hash_function_,
                                               key_equal_function_,
                                               data_.get_allocator());

                    for (auto &item: data_) {
                        if (!item.empty()) {
                            rehashing_table._insertion_helper(std::move(item));
                            rehashing_table.size_++;
                        }
                    }
                    rehashing_table.swap(*this);
                }
            }

            bool _try_to_rehash() {
                if (size_ < _size_to_rehash()) {
                    return false;
                } else {
                    _rehash(growth_policy_function_(std::max(data_.size(), size_type(1))));
                    return true;
                }
            }

            std::pair<size_type, bool> _find_spot(const key_type &key, size_t hash) const {
                if (data_.empty()) {
                    return std::make_pair(data_.size(), false);
                }

                size_type index = _hash_to_index(hash);
                size_type distance = 0;

                while (true) {
                    if (data_[index].empty() ||
                        distance > _distance_to_ideal_bucket(index)) {
                        return std::make_pair(index, false);
                    }
                    if (data_[index].hash() == hash &&
                        key_equal_function_(key_selector_function_(data_[index].value()), key)) {
                        return std::make_pair(index, true);
                    }
                    index = _next_index(index);
                    distance++;
                }
            }

            std::pair<size_type, bool> _find_spot(const key_type &key) const {
                size_t hash = key_hash_function_(key);
                return _find_spot(key, hash);
            }

            void _backward_shift(size_type index) {
                size_type prior_index = index;
                size_type current_index = _next_index(index);

                data_[prior_index].clear();
                while (!data_[current_index].empty() &&
                       _distance_to_ideal_bucket(current_index) > 0) {
                    data_[prior_index] = std::move(data_[current_index]);
                    prior_index = current_index;
                    current_index = _next_index(current_index);
                }
            }

            size_type _erase(const key_type &key) {
                auto spot_info = _find_spot(key);

                if (spot_info.second) {
                    _backward_shift(spot_info.first);
                    --size_;
                    return 1;
                }
                return 0;
            }

            void _insertion_helper(node &&insertion_node, size_type index) {
                size_type ideal_pos = _hash_to_index(insertion_node.hash());
                size_type distance = index - ideal_pos;

                while (!data_[index].empty()) {
                    if (_distance_to_ideal_bucket(index) < distance) {
                        distance = _distance_to_ideal_bucket(index);
                        data_[index].swap(insertion_node);
                    }
                    distance++;
                    index = _next_index(index);
                }
                data_[index].swap(insertion_node);
            }

            void _insertion_helper(node &&insertion_node) {
                size_type index = _hash_to_index(insertion_node.hash());
                _insertion_helper(std::move(insertion_node), index);
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

                auto insertion_spot_info = _find_spot(key, hash);

                if (insertion_spot_info.second) {
                    data_[insertion_spot_info.first].set_data(hash, std::forward<Args>(args)...);

                    auto first = data_.data();
                    auto last = data_.data() + data_.size();

                    return std::make_pair(iterator(first + insertion_spot_info.first, first, last), true);
                }

                if (_try_to_rehash()) {
                    insertion_spot_info = _find_spot(key, hash);
                }

                node insertion_node(hash, std::forward<Args>(args)...);
                _insertion_helper(std::move(insertion_node), insertion_spot_info.first);
                size_++;

                auto first = data_.data();
                auto last = data_.data() + data_.size();

                return std::make_pair(iterator(first + insertion_spot_info.first, first, last), false);
            }

        public:
            hash_table() = default;

            explicit hash_table(size_type capacity,
                                const hasher &key_hash_function = hasher{},
                                const key_equal &key_equal_function = key_equal{},
                                const allocator_type &allocator = allocator_type{})
                    :
                    data_(capacity, allocator),
                    key_hash_function_(key_hash_function),
                    key_equal_function_(key_equal_function),
                    key_selector_function_(),
                    growth_policy_function_() {
            }

            template<typename InputIt>
            hash_table(InputIt begin, InputIt end,
                       size_type capacity = 0,
                       const hasher &key_hash_function = hasher{},
                       const key_equal &key_equal_function = key_equal{},
                       const allocator_type &allocator = allocator_type{})
                    :
                    data_(capacity, allocator),
                    key_hash_function_(key_hash_function),
                    key_equal_function_(key_equal_function),
                    key_selector_function_(),
                    growth_policy_function_() {
                insert(begin, end);
            }

            hash_table(std::initializer_list<value_type> list,
                       size_type capacity = 0,
                       const hasher &key_hash_function = hasher{},
                       const key_equal &key_equal_function = key_equal{},
                       const allocator_type &allocator = allocator_type{})
                    :
                    data_(capacity, allocator),
                    key_hash_function_(key_hash_function),
                    key_equal_function_(key_equal_function),
                    key_selector_function_(),
                    growth_policy_function_() {
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
                    growth_policy_function_(other.growth_policy_function_) {}

            hash_table(const hash_table &other, const allocator_type &allocator)
                    :
                    data_(other.data_, allocator),
                    size_(other.size_),
                    load_factor_(other.load_factor_),
                    key_hash_function_(other.key_hash_function_),
                    key_equal_function_(other.key_equal_function_),
                    key_selector_function_(other.key_selector_function_),
                    growth_policy_function_(other.growth_policy_function_) {}

            hash_table(hash_table &&other) noexcept(
            std::is_nothrow_move_constructible<hasher>::value &&
            std::is_nothrow_move_constructible<key_equal>::value &&
            std::is_nothrow_move_constructible<growth_policy>::value &&
            std::is_nothrow_move_constructible<array>::value)
                    :
                    data_(std::move(other.data_)),
                    size_(other.size_),
                    load_factor_(other.load_factor_),
                    key_hash_function_(std::move(other.key_hash_function_)),
                    key_equal_function_(std::move(other.key_equal_function_)),
                    key_selector_function_(std::move(other.key_selector_function_)),
                    growth_policy_function_(std::move(other.growth_policy_function_)) {
                other.clear();
            }

            hash_table(hash_table &&other, const allocator_type &allocator)
                    :
                    data_(std::move(other.data_), allocator),
                    size_(other.size_),
                    load_factor_(other.load_factor_),
                    key_hash_function_(std::move(other.key_hash_function_)),
                    key_equal_function_(std::move(other.key_equal_function_)),
                    key_selector_function_(std::move(other.key_selector_function_)),
                    growth_policy_function_(std::move(other.growth_policy_function_)) {
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
                growth_policy_function_ = other.growth_policy_function_;
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
                growth_policy_function_ = std::move(other.growth_policy_function_);
                other.clear();
                return *this;
            }

            hash_table &operator=(std::initializer_list<value_type> list) noexcept {
                clear();
                insert(list.begin(), list.end());
                return *this;
            }

            allocator_type get_allocator() const {
                return data_.get_allocator();
            }

            std::pair<iterator, bool> insert(const value_type &value) {
                return _insert(value);
            }

            std::pair<iterator, bool> insert(value_type &&value) {
                return _insert(std::move(value));
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
                auto spot_info = _find_spot(key);
                if (spot_info.second) {
                    return 1;
                } else {
                    return 0;
                }
            }

            // TODO: One more methods of 'count'

            iterator find(const key_type &key) {
                return mutable_iterator(static_cast<const hash_table *>(this)->find(key));
            }

            const_iterator find(const key_type &key) const {
                auto spot_info = _find_spot(key);

                if (!spot_info.second) {
                    return end();
                }
                auto first = data_.data();
                auto last = data_.data() + data_.size();

                return const_iterator(first + spot_info.first, first, last);
            }

            //TODO: Two more methods of find

            bool contains(const key_type &key) const {
                return count(key) == 1;
            }

            // TODO: One more methods of 'contains'

            std::pair<iterator, iterator> equal_range(const key_type &key) {
                iterator founded = find(key);
                return std::make_pair(founded, std::next(founded));
            }

            std::pair<const_iterator, const_iterator> equal_range(const key_type &key) const {
                const_iterator founded = find(key);
                return std::make_pair(founded, std::next(founded));
            }

            //TODO: Two more methods of equal_range

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

            void reserve(size_type new_capacity) {
                size_type next_capacity = _next_capacity(new_capacity);

                if (new_capacity > data_.size()) {
                    _rehash(next_capacity);
                }
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
                std::swap(growth_policy_function_, other.growth_policy_function_);

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

            iterator mutable_iterator(const_iterator position) {
                auto first = data_.data();
                auto last = data_.data() + data_.size();
                return iterator(const_cast<node_pointer>(position.data_), first, last);
            }

            iterator begin() noexcept {
                auto first = data_.data();
                auto last = data_.data() + data_.size();
                auto current = first;
                while (current->empty() && current != last) {
                    ++current;
                }
                return iterator(current, first, last);
            }

            iterator end() noexcept {
                auto first = data_.data();
                auto last = data_.data() + data_.size();
                return iterator(last, first, last);
            }

            iterator begin() const noexcept {
                return cbegin();
            }

            const_iterator end() const noexcept {
                return cend();
            }

            const_iterator cbegin() const noexcept {
                auto first = data_.data();
                auto last = data_.data() + data_.size();
                auto current = first;
                while (current->empty() && current != last) {
                    ++current;
                }
                return const_iterator(current, first, last);
            }

            const_iterator cend() const noexcept {
                auto first = data_.data();
                auto last = data_.data() + data_.size();
                return const_iterator(last, first, last);
            }

            iterator rbegin() noexcept {
                auto first = data_.data();
                auto last = data_.data() + data_.size();
                auto current = last;
                while (current->empty() && current != first - 1) {
                    --current;
                }
                return iterator(current, first, last);
            }

            iterator rend() noexcept {
                auto first = data_.data();
                auto last = data_.data() + data_.size();
                return iterator(first - 1, first, last);
            }

            const_iterator rbegin() const noexcept {
                auto first = data_.data();
                auto last = data_.data() + data_.size();
                auto current = last;
                while (current->empty() && current != first - 1) {
                    --current;
                }
                return const_iterator(current, first, last);
            }

            const_iterator rend() const noexcept {
                auto first = data_.data();
                auto last = data_.data() + data_.size();
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
                using node_pointer = typename std::conditional<std::is_const<TItem>::value, const node *, node *>::type;

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

    class power_of_two_growth_policy {
    public:
        size_t operator()(size_t current) const {
            return current * 2;
        }
    };

    class prime_growth_policy {
    public:
        size_t operator()(size_t current) const {
            for (const auto &item: detail::PRIMES) {
                if (current < item) {
                    return item;
                }
            }
            return current;
        }
    };

    template<class TKey,
            class KeyHash = std::hash<TKey>,
            class KeyEqual = std::equal_to<TKey>,
            class Allocator = std::allocator<TKey>,
            class GrowthPolicy = power_of_two_growth_policy>
    class unordered_set {

        class key_selector {
        public:
            using key_type = TKey;

        public:
            key_type operator()(key_type &&p) const noexcept {
                return std::move(p.first);
            }

            const key_type &operator()(const key_type &p) const noexcept {
                return p.first;
            }
        };

        using hash_table = detail::hash_table<TKey,
                key_selector, KeyHash, KeyEqual, Allocator, GrowthPolicy>;

    public:
        using key_type = typename hash_table::key_type;
        using value_type = typename hash_table::value_type;

        using size_type = typename hash_table::size_type;
        using difference_type = typename hash_table::difference_type;

        using hasher = typename hash_table::hasher;
        using key_equal = typename hash_table::key_equal;
        using allocator_type = typename hash_table::allocator_type;

        using reference = typename hash_table::reference;
        using const_reference = typename hash_table::const_reference;

        using pointer = typename hash_table::pointer;
        using const_pointer = typename hash_table::const_pointer;

        using iterator = typename hash_table::iterator;
        using const_iterator = typename hash_table::const_iterator;

    private:
        hash_table hash_table_;

    public:
        unordered_set()
                :
                hash_table_() {}

        explicit unordered_set(size_type capacity,
                               const hasher &key_hash_function = hasher{},
                               const key_equal &key_equal_function = key_equal{},
                               const allocator_type &allocator = allocator_type{})
                : hash_table_(capacity, key_hash_function, key_equal_function, allocator) {}

        unordered_set(size_type capacity, const allocator_type &allocator)
                : hash_table_(capacity, hasher{}, key_equal{}, allocator) {}

        unordered_set(size_type capacity, const hasher &key_hash_function, const allocator_type &allocator)
                : hash_table_(capacity, key_hash_function, key_equal{}, allocator) {}

        explicit unordered_set(const allocator_type &allocator)
                : hash_table_(0, hasher{}, key_equal{}, allocator) {}

        template<typename InputIt>
        unordered_set(InputIt begin, InputIt end,
                      size_type capacity = 0,
                      const hasher &key_hash_function = hasher{},
                      const key_equal &key_equal_function = key_equal{},
                      const allocator_type &allocator = allocator_type{})
                : hash_table_(begin, end, capacity, key_hash_function, key_equal_function, allocator) {}

        template<typename InputIt>
        unordered_set(InputIt begin, InputIt end,
                      size_type capacity = 0,
                      const allocator_type &allocator = allocator_type{})
                : hash_table_(begin, end, capacity, hasher{}, key_equal{}, allocator) {}

        template<typename InputIt>
        unordered_set(InputIt begin, InputIt end,
                      size_type capacity = 0,
                      const hasher &key_hash_function = hasher{},
                      const allocator_type &allocator = allocator_type{})
                : hash_table_(begin, end, capacity, key_hash_function, key_equal{}, allocator) {}

        unordered_set(std::initializer_list<value_type> list,
                      size_type capacity = 0,
                      const hasher &key_hash_function = hasher{},
                      const key_equal &key_equal_function = key_equal{},
                      const allocator_type &allocator = allocator_type{})
                : hash_table_(list.begin(), list.end(), capacity, key_hash_function, key_equal_function, allocator) {}

        unordered_set(std::initializer_list<value_type> list,
                      size_type capacity = 0,
                      const allocator_type &allocator = allocator_type{})
                : hash_table_(list.begin(), list.end(), capacity, hasher{}, key_equal{}, allocator) {}

        unordered_set(std::initializer_list<value_type> list,
                      size_type capacity = 0,
                      const hasher &key_hash_function = hasher{},
                      const allocator_type &allocator = allocator_type{})
                : hash_table_(list.begin(), list.end(), capacity, key_hash_function, key_equal{}, allocator) {}

        unordered_set(const unordered_set &other) noexcept(std::is_nothrow_copy_constructible<hash_table>::value)
                : hash_table_(other.hash_table_) {}

        unordered_set(const unordered_set &other, const allocator_type &allocator)
                : hash_table_(other.hash_table_, allocator) {}

        unordered_set(unordered_set &&other) noexcept(std::is_nothrow_move_constructible<hash_table>::value)
                : hash_table_(std::move(other.hash_table_)) {}

        unordered_set(unordered_set &&other, const allocator_type &allocator)
                : hash_table_(std::move(other.hash_table_), allocator) {}

        unordered_set &operator=(const unordered_set &other) {
            hash_table_ = other.hash_table_;
            return *this;
        }

        unordered_set &operator=(unordered_set &&other) noexcept(std::is_nothrow_move_assignable<hash_table>::value) {
            hash_table_ = std::move(other.hash_table_);
            return *this;
        }

        unordered_set &operator=(std::initializer_list<value_type> list) {
            hash_table_ = list;
            return *this;
        }

        allocator_type get_allocator() const {
            return hash_table_.get_allocator();
        }

        iterator begin() noexcept {
            return hash_table_.begin();
        }

        iterator end() noexcept {
            return hash_table_.end();
        }

        const_iterator begin() const noexcept {
            return hash_table_.begin();
        }

        const_iterator end() const noexcept {
            return hash_table_.end();
        }

        const_iterator cbegin() const noexcept {
            return hash_table_.cbegin();
        }

        const_iterator cend() const noexcept {
            return hash_table_.cend();
        }

        iterator rbegin() noexcept {
            return hash_table_.rbegin();
        }

        iterator rend() noexcept {
            return hash_table_.rend();
        }

        const_iterator rbegin() const noexcept {
            return hash_table_.rbegin();
        }

        const_iterator rend() const noexcept {
            return hash_table_.rend();
        }

        bool empty() const noexcept {
            return hash_table_.empty();
        }

        size_type size() const noexcept {
            return hash_table_.size();
        }

        std::pair<iterator, bool> insert(const value_type &value) {
            return hash_table_.insert(value);
        }

        template<class P, typename std::enable_if<std::is_constructible<value_type, P &&>::value>::type * = nullptr>
        std::pair<iterator, bool> insert(P &&value) {
            return hash_table_.emplace(std::forward<P>(value));
        }

        std::pair<iterator, bool> insert(value_type &&value) {
            return hash_table_.insert(std::move(value));
        }

        iterator insert(const_iterator hint, const value_type &value) {
            return hash_table_.insert(hint, value);
        }

        template<class P, typename std::enable_if<std::is_constructible<value_type, P &&>::value>::type * = nullptr>
        iterator insert(const_iterator hint, P &&value) {
            return hash_table_.emplace_hint(hint, std::forward<P>(value));
        }

        iterator insert(const_iterator hint, value_type &&value) {
            return hash_table_.insert(hint, std::move(value));
        }

        template<class InputIt>
        void insert(InputIt begin, InputIt end) {
            hash_table_.insert(begin, end);
        }

        void insert(std::initializer_list<value_type> list) {
            hash_table_.insert(list.begin(), list.end());
        }

        template<class... Args>
        std::pair<iterator, bool> emplace(Args &&... args) {
            return hash_table_.emplace(std::forward<Args>(args)...);
        }

        template<class... Args>
        iterator emplace_hint(const_iterator hint, Args &&... args) {
            return hash_table_.emplace_hint(hint, std::forward<Args>(args)...);
        }

        iterator erase(iterator position) {
            return hash_table_.erase(position);
        }

        iterator erase(const_iterator position) {
            return hash_table_.erase(position);
        }

        iterator erase(const_iterator begin, const_iterator end) {
            return hash_table_.erase(begin, end);
        }

        size_type erase(const key_type &key) {
            return hash_table_.erase(key);
        }

        void swap(unordered_set &other) {
            other.hash_table_.swap(hash_table_);
        }

        size_type count(const key_type &key) const {
            return hash_table_.count(key);
        }

        // TODO: One more methods of 'count'

        iterator find(const key_type &key) {
            return hash_table_.find(key);
        }

        const_iterator find(const key_type &key) const {
            return hash_table_.find(key);
        }

        //TODO: Two more methods of find

        bool contains(const key_type &key) {
            return hash_table_.contains(key);
        }

        // TODO: One more methods of 'contains'

        std::pair<iterator, iterator> equal_range(const key_type &key) {
            return hash_table_.equal_range(key);
        }

        std::pair<const_iterator, const_iterator> equal_range(const key_type &key) const {
            const_iterator founded = find(key);
            return hash_table_.equal_range(key);
        }

        //TODO: Two more methods of equal_range

        size_type bucket_count() const {
            return hash_table_.bucket_count();
        }

        size_type max_bucket_count() const {
            return hash_table_.max_bucket_count();
        }

        float load_factor() const {
            return hash_table_.load_factor();
        }

        float max_load_factor() const {
            return hash_table_.max_load_factor();
        }

        void max_load_factor(float load_factor) {
            hash_table_.max_load_factor(load_factor);
        }

        void rehash(size_type new_capacity) {
            hash_table_.rehash(new_capacity);
        }

        void reserve(size_type new_capacity) {
            hash_table_.reserve(new_capacity);
        }

        hasher hash_function() const {
            return hash_table_.hash_function();
        }

        key_equal key_eq() const {
            return hash_table_.key_eq();
        }

        bool operator==(const unordered_set &other) const {
            return hash_table_ == other.hash_table_;
        }

        bool operator!=(const hash_table &other) const {
            return hash_table_ != other.hash_table_;
        }

        void clear() {
            hash_table_.clear();
        }
    };

    template<class TKey,
            class TValue,
            class KeyHash = std::hash<TKey>,
            class KeyEqual = std::equal_to<TKey>,
            class Allocator = std::allocator<std::pair<const TKey, TValue>>,
            class GrowthPolicy = power_of_two_growth_policy>
    class unordered_map {

        class key_selector {
        public:
            using key_type = TKey;
            using value_type = TValue;

        public:
            key_type operator()(std::pair<key_type, value_type> &&p) const noexcept {
                return std::move(p.first);
            }

            const key_type &operator()(const std::pair<key_type, value_type> &p) const noexcept {
                return p.first;
            }
        };


        using hash_table = detail::hash_table<std::pair<const TKey, TValue>,
                key_selector, KeyHash, KeyEqual, Allocator, GrowthPolicy>;

    public:
        using key_type = typename hash_table::key_type;
        using value_type = typename hash_table::value_type;
        using mapped_type = TValue;

        using size_type = typename hash_table::size_type;
        using difference_type = typename hash_table::difference_type;

        using hasher = typename hash_table::hasher;
        using key_equal = typename hash_table::key_equal;
        using allocator_type = typename hash_table::allocator_type;

        using reference = typename hash_table::reference;
        using const_reference = typename hash_table::const_reference;

        using pointer = typename hash_table::pointer;
        using const_pointer = typename hash_table::const_pointer;

        using iterator = typename hash_table::iterator;
        using const_iterator = typename hash_table::const_iterator;

    private:
        hash_table hash_table_;

    public:
        unordered_map()
                :
                hash_table_() {}

        explicit unordered_map(size_type capacity,
                               const hasher &key_hash_function = hasher{},
                               const key_equal &key_equal_function = key_equal{},
                               const allocator_type &allocator = allocator_type{})
                : hash_table_(capacity, key_hash_function, key_equal_function, allocator) {}

        unordered_map(size_type capacity, const allocator_type &allocator)
                : hash_table_(capacity, hasher{}, key_equal{}, allocator) {}

        unordered_map(size_type capacity, const hasher &key_hash_function, const allocator_type &allocator)
                : hash_table_(capacity, key_hash_function, key_equal{}, allocator) {}

        explicit unordered_map(const allocator_type &allocator)
                : hash_table_(0, hasher{}, key_equal{}, allocator) {}

        template<typename InputIt>
        unordered_map(InputIt begin, InputIt end,
                      size_type capacity = 0,
                      const hasher &key_hash_function = hasher{},
                      const key_equal &key_equal_function = key_equal{},
                      const allocator_type &allocator = allocator_type{})
                : hash_table_(begin, end, capacity, key_hash_function, key_equal_function, allocator) {}

        template<typename InputIt>
        unordered_map(InputIt begin, InputIt end,
                      size_type capacity = 0,
                      const allocator_type &allocator = allocator_type{})
                : hash_table_(begin, end, capacity, hasher{}, key_equal{}, allocator) {}

        template<typename InputIt>
        unordered_map(InputIt begin, InputIt end,
                      size_type capacity = 0,
                      const hasher &key_hash_function = hasher{},
                      const allocator_type &allocator = allocator_type{})
                : hash_table_(begin, end, capacity, key_hash_function, key_equal{}, allocator) {}

        unordered_map(std::initializer_list<value_type> list,
                      size_type capacity = 0,
                      const hasher &key_hash_function = hasher{},
                      const key_equal &key_equal_function = key_equal{},
                      const allocator_type &allocator = allocator_type{})
                : hash_table_(list.begin(), list.end(), capacity, key_hash_function, key_equal_function, allocator) {}

        unordered_map(std::initializer_list<value_type> list,
                      size_type capacity = 0,
                      const allocator_type &allocator = allocator_type{})
                : hash_table_(list.begin(), list.end(), capacity, hasher{}, key_equal{}, allocator) {}

        unordered_map(std::initializer_list<value_type> list,
                      size_type capacity = 0,
                      const hasher &key_hash_function = hasher{},
                      const allocator_type &allocator = allocator_type{})
                : hash_table_(list.begin(), list.end(), capacity, key_hash_function, key_equal{}, allocator) {}

        unordered_map(const unordered_map &other) noexcept(std::is_nothrow_copy_constructible<hash_table>::value)
                : hash_table_(other.hash_table_) {}

        unordered_map(const unordered_map &other, const allocator_type &allocator)
                : hash_table_(other.hash_table_, allocator) {}

        unordered_map(unordered_map &&other) noexcept(std::is_nothrow_move_constructible<hash_table>::value)
                : hash_table_(std::move(other.hash_table_)) {}

        unordered_map(unordered_map &&other, const allocator_type &allocator)
                : hash_table_(std::move(other.hash_table_), allocator) {}

        unordered_map &operator=(const unordered_map &other) {
            hash_table_ = other.hash_table_;
            return *this;
        }

        unordered_map &operator=(unordered_map &&other) noexcept(std::is_nothrow_move_assignable<hash_table>::value) {
            hash_table_ = std::move(other.hash_table_);
            return *this;
        }

        unordered_map &operator=(std::initializer_list<value_type> list) {
            hash_table_ = list;
            return *this;
        }

        allocator_type get_allocator() const {
            return hash_table_.get_allocator();
        }

        iterator begin() noexcept {
            return hash_table_.begin();
        }

        iterator end() noexcept {
            return hash_table_.end();
        }

        const_iterator begin() const noexcept {
            return hash_table_.begin();
        }

        const_iterator end() const noexcept {
            return hash_table_.end();
        }

        const_iterator cbegin() const noexcept {
            return hash_table_.cbegin();
        }

        const_iterator cend() const noexcept {
            return hash_table_.cend();
        }

        iterator rbegin() noexcept {
            return hash_table_.rbegin();
        }

        iterator rend() noexcept {
            return hash_table_.rend();
        }

        const_iterator rbegin() const noexcept {
            return hash_table_.rbegin();
        }

        const_iterator rend() const noexcept {
            return hash_table_.rend();
        }

        bool empty() const noexcept {
            return hash_table_.empty();
        }

        size_type size() const noexcept {
            return hash_table_.size();
        }

        std::pair<iterator, bool> insert(const value_type &value) {
            return hash_table_.insert(value);
        }

        template<class P, typename std::enable_if<std::is_constructible<value_type, P &&>::value>::type * = nullptr>
        std::pair<iterator, bool> insert(P &&value) {
            return hash_table_.emplace(std::forward<P>(value));
        }

        std::pair<iterator, bool> insert(value_type &&value) {
            return hash_table_.insert(std::move(value));
        }

        iterator insert(const_iterator hint, const value_type &value) {
            return hash_table_.insert(hint, value);
        }

        template<class P, typename std::enable_if<std::is_constructible<value_type, P &&>::value>::type * = nullptr>
        iterator insert(const_iterator hint, P &&value) {
            return hash_table_.emplace_hint(hint, std::forward<P>(value));
        }

        iterator insert(const_iterator hint, value_type &&value) {
            return hash_table_.insert(hint, std::move(value));
        }

        template<class InputIt>
        void insert(InputIt begin, InputIt end) {
            hash_table_.insert(begin, end);
        }

        void insert(std::initializer_list<value_type> list) {
            hash_table_.insert(list.begin(), list.end());
        }

        template<class... Args>
        std::pair<iterator, bool> emplace(Args &&... args) {
            return hash_table_.emplace(std::forward<Args>(args)...);
        }

        template<class... Args>
        iterator emplace_hint(const_iterator hint, Args &&... args) {
            return hash_table_.emplace_hint(hint, std::forward<Args>(args)...);
        }

        template<class K, class... Args>
        std::pair<iterator, bool> try_emplace(K &&key, Args &&... args) {
            return hash_table_.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(std::forward<K>(key)),
                                       std::forward_as_tuple(std::forward<Args>(args)...));
        }

        template<class K, class... Args>
        iterator try_emplace_hint(const_iterator hint, K &&key, Args &&... args) {
            (void) hint;
            return try_emplace(std::forward<K>(key), std::forward<Args>(args)...).first;
        }

        iterator erase(iterator position) {
            return hash_table_.erase(position);
        }

        iterator erase(const_iterator position) {
            return hash_table_.erase(position);
        }

        iterator erase(const_iterator begin, const_iterator end) {
            return hash_table_.erase(begin, end);
        }

        size_type erase(const key_type &key) {
            return hash_table_.erase(key);
        }

        void swap(unordered_map &other) {
            other.hash_table_.swap(hash_table_);
        }

        mapped_type &operator[](const key_type &key) {
            auto iter = hash_table_.find(key);
            if (iter != hash_table_.end()) {
                return iter->second;
            }
            return try_emplace(key).first->second;
        }

        mapped_type &operator[](key_type &&key) {
            auto iter = hash_table_.find(key);
            if (iter != hash_table_.end()) {
                return iter->second;
            }
            return try_emplace(std::move(key)).first->second;
        }

        mapped_type &at(const key_type &key) {
            return hash_table_.find(key)->second;
        }

        const mapped_type &at(const key_type &key) const {
            return hash_table_.find(key)->second;
        }

        size_type count(const key_type &key) const {
            return hash_table_.count(key);
        }

        // TODO: One more methods of 'count'

        iterator find(const key_type &key) {
            return hash_table_.find(key);
        }

        const_iterator find(const key_type &key) const {
            return hash_table_.find(key);
        }

        //TODO: Two more methods of find

        bool contains(const key_type &key) {
            return hash_table_.contains(key);
        }

        // TODO: One more methods of 'contains'

        std::pair<iterator, iterator> equal_range(const key_type &key) {
            return hash_table_.equal_range(key);
        }

        std::pair<const_iterator, const_iterator> equal_range(const key_type &key) const {
            const_iterator founded = find(key);
            return hash_table_.equal_range(key);
        }

        //TODO: Two more methods of equal_range

        size_type bucket_count() const {
            return hash_table_.bucket_count();
        }

        size_type max_bucket_count() const {
            return hash_table_.max_bucket_count();
        }

        float load_factor() const {
            return hash_table_.load_factor();
        }

        float max_load_factor() const {
            return hash_table_.max_load_factor();
        }

        void max_load_factor(float load_factor) {
            hash_table_.max_load_factor(load_factor);
        }

        void rehash(size_type new_capacity) {
            hash_table_.rehash(new_capacity);
        }

        void reserve(size_type new_capacity) {
            hash_table_.reserve(new_capacity);
        }

        hasher hash_function() const {
            return hash_table_.hash_function();
        }

        key_equal key_eq() const {
            return hash_table_.key_eq();
        }

        bool operator==(const unordered_map &other) const {
            return hash_table_ == other.hash_table_;
        }

        bool operator!=(const hash_table &other) const {
            return hash_table_ != other.hash_table_;
        }

        void clear() {
            hash_table_.clear();
        }
    };

    template<class TKey,
            class TValue,
            class KeyHash = std::hash<TKey>,
            class KeyEqual = std::equal_to<TKey>,
            class Allocator = std::allocator<std::pair<const TKey, TValue>>>
    using unordered_prime_map = unordered_map<TKey, TValue, KeyHash, KeyEqual, Allocator, prime_growth_policy>;

    template<class TKey,
            class KeyHash = std::hash<TKey>,
            class KeyEqual = std::equal_to<TKey>,
            class Allocator = std::allocator<TKey>>
    using unordered_prime_set = unordered_set<TKey, KeyHash, KeyEqual, Allocator, prime_growth_policy>;
}
#endif //HASHMAP_ROBIN_HOOD_H