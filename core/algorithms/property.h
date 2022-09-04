#pragma once

#include <any>
#include <map>
#include <string>
#include <vector>

namespace algorithms {

// Configuration/property handling
class Configurable {
public:
  class PropertyBase;
  using PropertyMap = std::map<std::string_view, PropertyBase&>;

  template <typename T, typename U> void setProperty(std::string_view name, U&& value) {
    m_props.at(name).set(T(std::forward<U>(value)));
  }
  template <typename T> T getProperty(std::string_view name) const {
    return std::any_cast<T>(m_props.at(name).get());
  }
  const PropertyMap& getProperties() const { return m_props; }
  bool hasProperty(std::string_view name) const {
    return m_props.count(name) && m_props.at(name).hasValue();
  }

private:
  void registerProperty(PropertyBase& prop) {
    // TODO ensure we have unique property names
    m_props.emplace(prop.name(), prop);
  }

  PropertyMap m_props;

public:
  class PropertyBase {
  public:
    PropertyBase(std::string_view name) : m_name{name} {}
    virtual void set(std::any v) = 0;
    virtual std::any get() const = 0;
    bool hasValue() const { return m_has_value; }
    std::string_view name() const { return m_name; }

  protected:
    std::string_view m_name;
    bool m_has_value = false;
  };

  // A property type that auto-registers itself with the property handler
  // Essentially a simplified and const-like version of Gaudi::Property
  template <class T> class Property : public PropertyBase {
  public:
    using ValueType = T;

    Property(Configurable* owner, std::string_view name) : PropertyBase{name} {
      // TODO what if we have no owner? warn? error?
      if (owner) {
        owner->registerProperty(*this);
      }
    }
    Property(Configurable* owner, std::string_view name, const ValueType& v)
        : Property(owner, name) {
      set(v);
    }

    Property()                      = delete;
    Property(const Property&)       = delete;
    void operator=(const Property&) = delete;

    // Only settable by explicitly calling the value operator
    // as we want the Property to mostly act as if it is constant
    virtual void set(std::any v) {
      m_value     = std::any_cast<T>(v);
      m_has_value = true;
    }
    // virtual getter for use from PropertyBase - use ::value() instead for a quick
    // version that does not go through std::any
    // Get const reference to the value
    virtual std::any get() const { return m_value; }
    const ValueType& value() const { return m_value; }

    // automatically cast to T
    operator T() const {return m_value;}

    // act as if this is a const T
    template <typename U> bool operator==(const U& rhs) const { return m_value == rhs; }
    template <typename U> bool operator!=(const U& rhs) const { return m_value != rhs; }
    template <typename U> bool operator>(const U& rhs) const { return m_value > rhs; }
    template <typename U> bool operator>=(const U& rhs) const { return m_value >= rhs; }
    template <typename U> bool operator<(const U& rhs) const { return m_value < rhs; }
    template <typename U> bool operator<=(const U& rhs) const { return m_value <= rhs; }
    template <typename U> decltype(auto) operator+(const U& rhs) const { return m_value + rhs; }
    template <typename U> decltype(auto) operator-(const U& rhs) const { return m_value - rhs; }
    template <typename U> decltype(auto) operator*(const U& rhs) const { return m_value * rhs; }
    template <typename U> decltype(auto) operator/(const U& rhs) const { return m_value / rhs; }

    // stl collection helpers if needed
    // forced to be templated so we only declare them when used
    template <class U = const ValueType> decltype(auto) size() const { return value().size(); }
    template <class U = const ValueType> decltype(auto) length() const { return value().length(); }
    template <class U = const ValueType> decltype(auto) empty() const { return value().empty(); }
    template <class U = ValueType> decltype(auto) clear() { value().clear(); }
    template <class U = const ValueType> decltype(auto) begin() const { return value().begin(); }
    template <class U = const ValueType> decltype(auto) end() const { return value().end(); }
    template <class U = ValueType> decltype(auto) begin() { return value().begin(); }
    template <class U = ValueType> decltype(auto) end() { return value().end(); }
    template <class Arg> decltype(auto) operator[](const Arg& arg) const { return value()[arg]; }
    template <class Arg> decltype(auto) operator[](const Arg& arg) { return value()[arg]; }
    template <class U = const ValueType>
    decltype(auto) find(const typename U::key_type& key) const {
      return value().find(key);
    }
    template <class U = ValueType> decltype(auto) find(const typename U::key_type& key) {
      return value().find(key);
    }
    template <class Arg> decltype(auto) erase(const Arg& arg) { return value().erase(arg); }

    // In case our property has operator (), delegate the operator()
    template <class... Args>
    decltype(std::declval<ValueType>()(std::declval<Args&&>()...))
    operator()(Args&&... args) const {
      return m_value()(std::forward<Args>(args)...);
    }

  private:
    T m_value;
  };
};

// Property mixin, provides all the configuration functionality for
// our algorithms and services
// Currently an alias to Configurable
using PropertyMixin = Configurable;

} // namespace algorithms

// operator== overload not needed in C++20 as it will call the member version
#if __cpp_impl_three_way_comparison < 201711
template <class T, class U>
bool operator==(const U& rhs, algorithms::Configurable::Property<T>& p) {
  return p == rhs;
}
#endif
// others needed??? TODO