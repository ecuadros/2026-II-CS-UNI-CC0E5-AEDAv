#include <iostream>
#include <stdexcept>
#include <algorithm> // para std::swap

class Vector {
private:
    unsigned long long* data;
    int m_size;   // cantidad actual
    int cap;  // capacidad

    void reserve(int new_cap) {
        if (new_cap <= cap) return;
        unsigned long long* new_data = new unsigned long long[new_cap];
        for (int i = 0; i < m_size; ++i) {
            new_data[i] = data[i];
        }
        delete[] data;
        data = new_data;
        cap = new_cap;
    }

public:
    Vector() : data(nullptr), m_size(0), cap(0) {}

    ~Vector() {
        delete[] data;
    }

    Vector(const Vector& other) : data(nullptr), m_size(0), cap(0) {
        reserve(other.m_size);
        for (int i = 0; i < other.m_size; ++i) {
            data[i] = other.data[i];
        }
        m_size = other.m_size;
    }

    Vector& operator=(Vector other) {
        swap(other);
        return *this;
    }

    Vector(Vector&& other) noexcept : data(nullptr), m_size(0), cap(0) {
        swap(other);
    }

    Vector& operator=(Vector&& other) noexcept {
        swap(other);
        return *this;
    }

    void swap(Vector& other) noexcept {
        std::swap(data, other.data);
        std::swap(m_size, other.m_size);
        std::swap(cap, other.cap);
    }

    void push_back(unsigned long long value) {
        if (m_size == cap) {
            int new_cap = (cap == 0) ? 1 : cap * 2;
            reserve(new_cap);
        }
        data[m_size] = value;
        ++m_size;
    }

    void pop_back() {
        if (m_size > 0) {
            --m_size;
            // data[m_size].~unsigned long long();
        }
    }

    unsigned long long& operator[](int index) {
        return data[index];
    }

    const unsigned long long& operator[](int index) const {
        return data[index];
    }

    unsigned long long& at(int index) {
        if (index >= m_size) throw std::out_of_range("Indice fuera de rango");
        return data[index];
    }

    const unsigned long long& at(int index) const {
        if (index >= m_size) throw std::out_of_range("Indice fuera de rango");
        return data[index];
    }

    int size() const { return m_size; }
    int capacity() const { return cap; }
    bool empty() const { return m_size == 0; }

    void clear() {
        for (int i = 0; i < m_size; ++i) {
            // data[i].~unsigned long long();
        }
        m_size = 0;
    }
};