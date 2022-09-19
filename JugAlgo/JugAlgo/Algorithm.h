// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Sylvester Joosten

#include "GaudiAlg/GaudiAlgorithm.h"
#include "JugAlgo/IAlgoServiceSvc.h"
#include "JugBase/DataHandle.h"
#include "algorithms/algorithm.h"

#include <algorithms/type_traits.h>
#include <string>
#include <type_traits>

namespace Jug::Algo {

namespace detail {
  template <class T, class Enable = void> struct handle_type { using type = std::unique_ptr<DataHandle<T>>; };
  template <class Vector> struct handle_type<Vector, std::enable_if_t<algorithms::is_vector_v<Vector>>> {
    using type = Gaudi::Property<std::string>;
  };
  template <class Optional> struct handle_type<Optional, std::enable_if_t<algorithms::is_optional_v<Optional>>> {
    using type = Gaudi::Property<std::string>;
  };
  template <class T> using handle_type_t = typename handle_type<T>::type;
  template <class... T> struct DataHandleTuple : std::tuple<handle_type_t<T>...> {};

} // namespace detail

template <class AlgoImpl> class Algorithm : public GaudiAlgorithm {
public:
  using AlgoType   = AlgoImpl;
  using InputType  = typename AlgoType::InputType;
  using OutputType = typename AlgoType::OutputType;

  StatusCode initialize() override {
    debug() << "Initializing " << name() << endmsg;

    // Forward the log level of this algorithm
    const algorithms::LogLevel level{static_cast<algorithms::LogLevel>(msgLevel() > 0 ? msgLevel() - 1 : 0)};
    debug() << "Setting the logger level to " << algorithms::logLevelName(level) << endmsg;
    m_algo->level(level);

    // call configure function that passes properties
    debug() << "Configuring properties" << endmsg;
    auto sc = configure();
    if (sc != StatusCode::SUCCESS) {
      return sc;
    }

    // call the internal algorithm init
    debug() << "Initializing underlying algorithm " << m_algo.name() << endmsg;
    m_algo.init();
    return StatusCode::SUCCESS;
  }

  StatusCode execute() override {
    ;
    return StatusCode::SUCCESS;
  }

  virtual StatusCode configure() = 0;

protected:
  template <typename T, typename U> void setAlgoProp(std::string_view name, U&& value) {
    m_algo.template setProperty<T>(name, value);
  }
  template <typename T> T getAlgoProp(std::string name) const { return m_algo.template getProperty<T>(name); }
  bool hasAlgoProp(std::string_view name) const { return m_algo.hasProperty(name); }

private:
  InputType getInput() const {}

  // template <class Function, class RefTuple, std::size_t... I>
  // RefTuple initialize

  AlgoType m_algo;
};

} // namespace Jug::Algo
