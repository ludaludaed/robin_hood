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
                size_(size) {
            data_ = _allocate_and_construct_data(allocator_, size_);
        }

        array(size_type size, const_reference default_value)
                :
                size_(size) {
            data_ = _allocate_and_construct_data(allocator_, size_, default_value);
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
            clear();
        }

        void swap(array &other) {
            if (this == &other) {
                return;
            }
            if constexpr(allocator_traits::propagate_on_container_swap::value ||
                         allocator_traits::is_always_equal::value) {
                std::swap(allocator_, other.allocator_);
            }
            std::swap(data_, other.data_);
            std::swap(size_, other.size_);
        }

        void clear() {
            for (size_type i = 0; i < size_; ++i) {
                allocator_traits::destroy(allocator_, data_ + i);
            }
            allocator_traits::deallocate(allocator_, data_, size_);
            size_ = 0;
        }

        array &operator=(const array &other) {
            if (this == &other) {
                return *this;
            }
            array temp(other);
            temp.swap(*this);
            return *this;
        }

        array &operator=(array &&other) noexcept {
            other.swap(*this);
            other.clear();
            return *this;
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
                clear();
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
                clear();
                size_ = new_size;
                data_ = new_data;
            }
        }

        pointer data() noexcept {
            return data_;
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

        public:
            reference operator*() const {
                return *data_;
            }

            pointer operator->() const {
                return data_;
            }

            reference operator[](difference_type index) const {
                return data_[index];
            }

        public:
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

        public:
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
        using storage = utils::storage<TValue>;

    private:
        static const uint8_t kNoEmptyMarker = 0;
        static const uint8_t kEmptyMarker = 1;
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

    // base for hash set and hash map
    template<class TValue,
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
        using node_allocator = typename std::allocator_traits<allocator_type>::template rebind_alloc<node>;
        using size_type = typename std::allocator_traits<node_allocator>::size_type;
        using array = array<node, node_allocator>;

    private:
        KeySelector key_selector_{};
        KeyHash key_hash_{};
        KeyEqual key_equal_{};
        GrowPolicy grow_policy_{};

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

        size_type _find_index(const key_type &key, size_t hash) {
            size_type index = _hash_to_index(hash);
            size_type distance = 0;

            while (index < data_.size()) {
                if (data_[index].empty() ||
                    distance > _distance_to_ideal_bucket(index)) {
                    return -1;
                }
                if (data_[index].hash() == hash &&
                    key_equal_(data_[index], key)) {
                    return index;
                }
                index++;
                distance++;
            }
            return -1;
        }

        size_type _find_index(const key_type &key) {
            size_t hash = key_hash_(key);
            return _find_index(key, hash);
        }

        size_type _erase(const key_type &key) {
            size_type index = _find_index(key);

            if (index != -1) {
                while (index + 1 < data_.size() &&
                       !data_[index + 1].empty() &&
                       _distance_to_ideal_bucket(index + 1) != 0) {
                    data_[index] = std::move(data_[index + 1]);
                    index++;
                }
                return 1;
            }
            return 0;
        }

        bool _try_to_rehash() {
            if (size_ < _size_to_rehash()) {
                return false;
            }
            //TODO...
            return true;
        }

        void _insert(const value_type &value) {
            const key_type &key = key_selector_(value);
            size_t hash = key_hash_(key);
            size_type index = _hash_to_index(hash);

            size_type insertion_index = _find_index(key, hash);

            if (insertion_index != -1) {
                data_[insertion_index].set_data(hash, value);
            } else {
                if (_try_to_rehash()) {
                    index = _hash_to_index(hash);
                }
                if (data_[index].empty()) {
                    data_[index].set_data(hash, value);
                } else {
                    size_type distance = 0;
                    node insertion_node;
                    insertion_node.set_data(hash, value);
                    while (!data_[index].empty()) {
                        if (_distance_to_ideal_bucket(index) < distance) {
                            data_[index].swap(insertion_node);
                            distance = _distance_to_ideal_bucket(index);
                        }
                        //TODO mb not work... need to test
                        distance++;
                        index++;
                    }
                    data_[index].swap(insertion_node);
                }
            }
        }

    public:
        allocator_type get_allocator() const {
            return data_.get_allocator();
        }

        hash_table();

        hash_table(const hash_table &other);

        hash_table(hash_table &&other);

        hash_table &operator=(const hash_table &other);

        hash_table &operator=(hash_table &&other);

        void clear();

        void reserve();

        void swap(hash_table &other) {
            std::swap(key_selector_, other.key_selector_);
            std::swap(key_hash_, other.key_hash_);
            std::swap(key_equal_, other.key_equal_);
            std::swap(grow_policy_, other.grow_policy_);

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
    static_assert(std::random_access_iterator<detail::array<int>::const_iterator>);
    {
        detail::node<A> node1;
        node1.set_data(1, "11111");
        detail::node<A> node2(node1);
        node2.set_data(1, "22222");
        node1.swap(node2);
        std::cout << node1.value() << " " << node2.value();

        detail::array<int> array;
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

        detail::array<int> a;

        std::swap(array, a);
    }
    return 0;
}