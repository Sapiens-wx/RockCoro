#pragma once
namespace rockcoro {

template <typename T, size_t CAPACITY = 64> class Vector {
public:
    Vector()
        : data_(allocate(CAPACITY))
        , size_(0)
        , capacity_(CAPACITY)
    {
    }

    explicit Vector(size_t n)
        : data_(allocate(n))
        , size_(n)
        , capacity_(n)
    {
        for (size_t i = 0; i < n; ++i)
            new (data_ + i) T();
    }

    ~Vector()
    {
        destroy_all();
        operator delete(data_);
    }

    Vector(const Vector &other)
        : data_(allocate(other.capacity_))
        , size_(other.size_)
        , capacity_(other.capacity_)
    {
        for (size_t i = 0; i < size_; ++i)
            new (data_ + i) T(other.data_[i]);
    }

    Vector &operator=(const Vector &other)
    {
        if (this == &other)
            return *this;

        destroy_all();
        operator delete(data_);

        data_ = allocate(other.capacity_);
        size_ = other.size_;
        capacity_ = other.capacity_;

        for (size_t i = 0; i < size_; ++i)
            new (data_ + i) T(other.data_[i]);

        return *this;
    }

    Vector(Vector &&other) noexcept
        : data_(other.data_)
        , size_(other.size_)
        , capacity_(other.capacity_)
    {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    Vector &operator=(Vector &&other) noexcept
    {
        if (this == &other)
            return *this;

        destroy_all();
        operator delete(data_);

        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;

        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;

        return *this;
    }

    T &operator[](size_t i)
    {
        assert(i < size_);
        return data_[i];
    }

    const T &operator[](size_t i) const
    {
        assert(i < size_);
        return data_[i];
    }

    // ---------- 容量 ----------
    size_t size() const
    {
        return size_;
    }
    size_t capacity() const
    {
        return capacity_;
    }
    bool empty() const
    {
        return size_ == 0;
    }

    void push_back(const T &value)
    {
        ensure_capacity(size_ + 1);
        new (data_ + size_) T(value);
        ++size_;
    }

    void push_back(T &&value)
    {
        ensure_capacity(size_ + 1);
        new (data_ + size_) T(std::move(value));
        ++size_;
    }

    template <typename... Args> T &emplace_back(Args &&...args)
    {
        ensure_capacity(size_ + 1);
        new (data_ + size_) T(std::forward<Args>(args)...);
        return data_[size_++];
    }

    void pop_back()
    {
        assert(size_ > 0);
        data_[--size_].~T();
    }

    void clear()
    {
        destroy_all();
        size_ = 0;
    }

private:
    T *data_;
    size_t size_;
    size_t capacity_;

    static T *allocate(size_t n)
    {
        if (n == 0)
            return nullptr;
        return static_cast<T *>(operator new(sizeof(T) * n));
    }

    void destroy_all()
    {
        for (size_t i = 0; i < size_; ++i)
            data_[i].~T();
    }

    void ensure_capacity(size_t min_cap)
    {
        if (min_cap <= capacity_)
            return;

        size_t new_cap = capacity_ == 0 ? 1 : capacity_ * 2;
        if (new_cap < min_cap)
            new_cap = min_cap;

        T *new_data = allocate(new_cap);

        // 移动构造
        for (size_t i = 0; i < size_; ++i)
            new (new_data + i) T(std::move(data_[i]));

        destroy_all();
        operator delete(data_);

        data_ = new_data;
        capacity_ = new_cap;
    }
};
} // namespace rockcoro