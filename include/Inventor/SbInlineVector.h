#ifndef COIN_SBINLINEVECTOR_H
#define COIN_SBINLINEVECTOR_H

#include <cstddef>
#include <vector>

/*!
  \class SbInlineVector
  \brief Contiguous storage with space for N values inside the container.

  Larger sequences transparently move to heap-backed storage. This deliberately
  small interface serves Coin value records that are default-constructible and
  assignable; it is not intended to reproduce the complete std::vector API.
*/
template <typename T, std::size_t N>
class SbInlineVector {
public:
  static_assert(N > 0, "SbInlineVector requires inline storage");
  using value_type = T;
  using size_type = std::size_t;
  using iterator = value_type *;
  using const_iterator = const value_type *;

  bool empty() const { return this->size() == 0; }
  size_type size() const
  {
    return this->usingOverflow ? this->overflow.size() : this->inlineCount;
  }
  void clear()
  {
    this->inlineCount = 0;
    this->overflow.clear();
    this->usingOverflow = false;
  }
  void reserve(size_type count)
  {
    if (count > N) this->overflow.reserve(count);
  }
  void push_back(const value_type & value)
  {
    if (!this->usingOverflow && this->inlineCount < N) {
      this->inlineValues[this->inlineCount++] = value;
      return;
    }
    this->moveInlineValuesToOverflow();
    this->overflow.push_back(value);
  }
  void push_back(value_type && value)
  {
    if (!this->usingOverflow && this->inlineCount < N) {
      this->inlineValues[this->inlineCount++] = static_cast<value_type &&>(value);
      return;
    }
    this->moveInlineValuesToOverflow();
    this->overflow.push_back(static_cast<value_type &&>(value));
  }
  void resize(size_type count)
  {
    if (count == 0) this->clear();
    else if (!this->usingOverflow && count <= N) this->inlineCount = count;
    else {
      this->moveInlineValuesToOverflow();
      this->overflow.resize(count);
    }
  }
  value_type & operator[](size_type index)
  {
    return this->usingOverflow ? this->overflow[index]
                               : this->inlineValues[index];
  }
  const value_type & operator[](size_type index) const
  {
    return this->usingOverflow ? this->overflow[index]
                               : this->inlineValues[index];
  }
  iterator data()
  {
    return this->usingOverflow ? this->overflow.data()
                               : this->inlineValues;
  }
  const_iterator data() const
  {
    return this->usingOverflow ? this->overflow.data()
                               : this->inlineValues;
  }
  iterator begin() { return this->data(); }
  const_iterator begin() const { return this->data(); }
  iterator end() { return this->data() + this->size(); }
  const_iterator end() const { return this->data() + this->size(); }

private:
  void moveInlineValuesToOverflow()
  {
    if (this->usingOverflow) return;
    this->overflow.reserve(this->inlineCount + 1);
    for (size_type i = 0; i < this->inlineCount; ++i) {
      this->overflow.push_back(
        static_cast<value_type &&>(this->inlineValues[i]));
    }
    this->usingOverflow = true;
  }

  value_type inlineValues[N] = {};
  size_type inlineCount = 0;
  std::vector<value_type> overflow;
  bool usingOverflow = false;
};

#endif // COIN_SBINLINEVECTOR_H
