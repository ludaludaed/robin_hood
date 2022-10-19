#include <iostream>
#include <unordered_map>
#include <utility>
#include <list>
#include <cassert>

namespace detail {

    namespace utils {
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
    }

    template<typename TValue>
    class node {
    public:
        using distance_type = int16_t;
        using hash_type = size_t;
        using value_type = TValue;
        using storage = utils::storage<TValue>;

    private:
        static const distance_type kEmptyDistanceMarker = -1;
        static const hash_type kDefaultHash = 0;

        distance_type distance_;
        hash_type hash_;
        storage value_;

    public:
        node()
                :
                distance_(kEmptyDistanceMarker),
                hash_(kDefaultHash) {}

        node(const node &other) noexcept(std::is_nothrow_copy_constructible_v<value_type>)
                :
                distance_(other.distance_),
                hash_(other.hash_) {
            if (!other.empty()) {
                value_.construct(*other.value_);
            }
        }

        node(node &&other) noexcept(std::is_nothrow_move_constructible_v<value_type>)
                :
                distance_(other.distance_),
                hash_(other.hash_) {
            if (!other.empty()) {
                value_.construct(std::move(*other.value_));
            }
        }

        ~node() {
            clear();
        }

        template<typename ...Args>
        void set_data(distance_type distance, hash_type hash, Args &&...args) {
            if (!empty()) {
                clear();
            }
            value_.construct(std::forward<Args>(args)...);
            distance_ = distance;
            hash_ = hash;
        }

        void clear() {
            if (!empty()) {
                value_.destruct();
            }
            distance_ = kEmptyDistanceMarker;
            hash_ = kDefaultHash;
        }

        void swap(node &other) {
            std::swap(*value_, *other.value_);
            std::swap(hash_, other.hash_);
            std::swap(distance_, other.distance_);
        }

        bool empty() const {
            return distance_ == kEmptyDistanceMarker;
        }

        distance_type distance() const {
            return distance_;
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
        using allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<TValue>;
        using size_type = typename std::allocator_traits<allocator>::size_type;
        using allocator_traits = std::allocator_traits<allocator>;

        allocator allocator_;
        pointer data_;
        size_type size_;

    public:
        array()
                :
                data_(nullptr),
                size_(0) {};

        explicit array(size_type size)
                :
                size_(size) {
            data_ = allocator_traits::allocate(allocator_, size_);
            for (size_type i = 0; i < size_; ++i) {
                try {
                    allocator_traits::construct(allocator_, data_ + i);
                } catch (...) {
                    for (int j = 0; j < i; ++j) {
                        allocator_traits::destroy(allocator_, data_ + j);
                    }
                    allocator_traits::deallocate(allocator_, data_, size_);
                    throw;
                }
            }
        }

        array(size_type size, const_reference default_value)
                :
                size_(size) {
            data_ = allocator_traits::allocate(allocator_, size_);
            for (size_type i = 0; i < size_; ++i) {
                try {
                    allocator_traits::construct(allocator_, data_ + i, default_value);
                } catch (...) {
                    for (int j = 0; j < i; ++j) {
                        allocator_traits::destroy(allocator_, data_ + j);
                    }
                    allocator_traits::deallocate(allocator_, data_, size_);
                    throw;
                }
            }
        }

        array(const array &other)
                :
                size_(other.size_),
                allocator_(allocator_traits::select_on_container_copy_construction(other.allocator_)) {
            data_ = allocator_traits::allocate(allocator_, size_);
            for (size_type i = 0; i < size_; ++i) {
                try {
                    allocator_traits::construct(allocator_, data_ + i, other.data_[i]);
                } catch (...) {
                    for (int j = 0; j < i; ++j) {
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
            if (data_ != nullptr) {
                for (size_type i = 0; i < size_; ++i) {
                    allocator_traits::destroy(allocator_, data_ + i);
                }
                allocator_traits::deallocate(allocator_, data_, size_);
            }
        }

        void resize(size_type new_size);

        void resize(size_type new_size, const_reference default_value);

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

        bool empty() const {
            return size_ == 0;
        }

        size_type size() const {
            return size_;
        }

    private:
        template<typename TItem>
        class array_iterator {
            friend class array;

        public:
            using iterator_category = std::contiguous_iterator_tag;
            using value_type = TItem;
            using element_type = TItem;
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

    // base for hash set and hash map
    template<
            class TValue,
            class KeySelector,
            class KeyHash,
            class KeyEqual,
            class GrowPolicy,
            class Allocator>
    class hash_table {
        template<typename TItem>
        class hash_iterator;

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

        using iterator = hash_iterator<TValue>;
        using const_iterator = hash_iterator<const TValue>;

        using allocator_type = Allocator;

    private:
        using node = node<TValue>;
        using distance_type = typename node::distance_type;
        using node_allocator = typename std::allocator_traits<allocator_type>::template rebind_alloc<node>;
        using size_type = typename std::allocator_traits<allocator_type>::size_type;

    private:
        float load_factor_{0.5f};
        size_type size_{0};
        array<node, node_allocator> data_;

    public:
        hash_table();

        hash_table(const hash_table &other);

        hash_table(hash_table &&other);

        template<typename Iter>
        hash_table(Iter begin, Iter end);

        ~hash_table();

        bool empty() const {
            return size_ == 0;
        }

        size_type size() const {
            return size_;
        }
    };

    class grow_power_of_two_policy {
    public:
        size_t operator()(size_t current) const {
            return current * 2;
        }
    };
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
    static_assert(std::contiguous_iterator<detail::array<int>::iterator>);
    detail::node<A> node1;
    node1.set_data(1, 1, "11111");
    detail::node<A> node2(node1);

    node2.set_data(1, 1, "22222");

    node1.swap(node2);

    std::cout << node1.value() << " " << node2.value();
    return 0;
}