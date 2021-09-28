// This file is part of the Acts project.
//
// Copyright (C) 2016-2018 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <variant>

#include "Acts/Definitions/Algebra.hpp"
#include "Acts/MagneticField/MagneticFieldContext.hpp"
#include "Acts/MagneticField/MagneticFieldProvider.hpp"
#include "Acts/Utilities/Result.hpp"

#include "DD4hep/Detector.h"
#include "DD4hep/DD4hepUnits.h"


namespace Jug::BField {

  ///// The Context to be handed around
  //struct ScalableBFieldContext {
  //  double scalor = 1.;
  //};

  /** Use the dd4hep magnetic field in acts.
   *
   * \ingroup magnets
   * \ingroup magsvc
   */
  class DD4hepBField final : public Acts::MagneticFieldProvider {
  public:
    std::shared_ptr<dd4hep::Detector> m_det;

  public:
    struct Cache {
      Cache(const Acts::MagneticFieldContext& /*mcfg*/) { }
    };

    Acts::MagneticFieldProvider::Cache makeCache(const Acts::MagneticFieldContext& mctx) const override
    {
      return Acts::MagneticFieldProvider::Cache::make<Cache>(mctx);
    }

    /** construct constant magnetic field from field vector.
    *
    * @param [in] DD4hep detector instance
    */
    explicit DD4hepBField(dd4hep::Detector* det) : m_det(std::shared_ptr<dd4hep::Detector>(det)) {}

    /** Retrieve magnetic field value.
     *
     *  @param [in] position global position
     *  @return magnetic field vector
     */
    Acts::Vector3 getField(const Acts::Vector3& position) const override;

    /**  retrieve magnetic field value.
     *
     *  @param [in] position global position
     *  @param [in] cache Cache object (is ignored)
     *  @return magnetic field vector
     *
     *  @note The @p position is ignored and only kept as argument to provide
     *        a consistent interface with other magnetic field services.
     */
    Acts::Vector3 getField(const Acts::Vector3& position, Acts::MagneticFieldProvider::Cache& cache) const override;

    /** @brief retrieve magnetic field value & its gradient
     *
     * @param [in]  position   global position
     * @param [out] derivative gradient of magnetic field vector as (3x3)
     * matrix
     * @return magnetic field vector
     *
     * @note The @p position is ignored and only kept as argument to provide
     *       a consistent interface with other magnetic field services.
     * @note currently the derivative is not calculated
     * @todo return derivative
     */
    Acts::Vector3 getFieldGradient(const Acts::Vector3& position,
                                   Acts::ActsMatrix<3, 3>& /*derivative*/) const override;

    /** @brief retrieve magnetic field value & its gradient
     *
     * @param [in]  position   global position
     * @param [out] derivative gradient of magnetic field vector as (3x3)
     * matrix
     * @param [in] cache Cache object (is ignored)
     * @return magnetic field vector
     *
     * @note The @p position is ignored and only kept as argument to provide
     *       a consistent interface with other magnetic field services.
     * @note currently the derivative is not calculated
     * @todo return derivative
     */
    Acts::Vector3 getFieldGradient(const Acts::Vector3& position, Acts::ActsMatrix<3, 3>& /*derivative*/,
                                   Acts::MagneticFieldProvider::Cache& cache) const override;
  };

  using BFieldVariant = std::variant<std::shared_ptr<const DD4hepBField>>;



} // namespace Jug::BField
