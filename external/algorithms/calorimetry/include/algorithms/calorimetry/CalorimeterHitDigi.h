// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Chao Peng, Wouter Deconinck, Sylvester Joosten

// A general digitization for CalorimeterHit from simulation
// 1. Smear energy deposit with a/sqrt(E/GeV) + b + c/E or a/sqrt(E/GeV) (relative value)
// 2. Digitize the energy with dynamic ADC range and add pedestal (mean +- sigma)
// 3. Time conversion with smearing resolution (absolute value)
// 4. Signal is summed if the SumFields are provided
//
// Author: Chao Peng
// Date: 06/02/2021

#include <algorithms/algorithm.h>
#include <algorithms/geo.h>
#include <algorithms/property.h>
#include <algorithms/random.h>

#include "DDRec/CellIDPositionConverter.h"
#include "DDSegmentation/BitFieldCoder.h"

#include "fmt/format.h"
#include "fmt/ranges.h"

// Event Model related classes
#include "edm4hep/SimCalorimeterHitCollection.h"
#include "edm4eic/RawCalorimeterHitCollection.h"

namespace algorithms::calorimetry {

  using CalorimeterHitDigiAlgorithm = Algorithm<
    Input<edm4hep::SimCalorimeterHitCollection>,
    Output<edm4eic::RawCalorimeterHitCollection>>;

  /** Generic calorimeter hit digitiziation.
   *
   * \ingroup digi
   * \ingroup calorimetry
   */
  class CalorimeterHitDigi : public CalorimeterHitDigiAlgorithm {
  public:
    using Input      = CalorimeterHitDigiAlgorithm::Input;
    using Output     = CalorimeterHitDigiAlgorithm::Output;

    CalorimeterHitDigi(std::string_view name)
      : CalorimeterHitDigiAlgorithm{name,
                            {"inputHitCollection"},
                            {"outputHitCollection"}} {};

    void init();
    void process(const Input&, const Output&);

  private:
    void single_hits_digi(const Input&, const Output&);
    void signal_sum_digi(const Input&, const Output&);

    // additional smearing resolutions
    Property<std::vector<double>> u_eRes{this, "energyResolutions", {}}; // a/sqrt(E/GeV) + b + c/(E/GeV)
    Property<double>              m_tRes{this, "timeResolution", 0.0 * dd4hep::ns};
    // single hit energy deposition threshold
    Property<double>              m_threshold{this, "threshold", 1. * dd4hep::keV};

    // digitization settings
    Property<unsigned int>       m_capADC{this, "capacityADC", 8096};
    Property<double>             m_dyRangeADC{this, "dynamicRangeADC", 100 * dd4hep::MeV};
    Property<unsigned int>       m_pedMeanADC{this, "pedestalMean", 400};
    Property<double>             m_pedSigmaADC{this, "pedestalSigma", 3.2};
    Property<double>             m_resolutionTDC{this, "resolutionTDC", 0.010 * dd4hep::ns};

    Property<double>             m_corrMeanScale{this, "scaleResponse", 1.0};
    // These are better variable names for the "energyResolutions" array which is a bit
    // magic @FIXME
    //Property<double>             m_corrSigmaCoeffE{this, "responseCorrectionSigmaCoeffE", 0.0};
    //Property<double>             m_corrSigmaCoeffSqrtE{this, "responseCorrectionSigmaCoeffSqrtE", 0.0};

    // signal sums
    // @TODO: implement signal sums with timing
    // field names to generate id mask, the hits will be grouped by masking the field
    Property<std::vector<std::string>> u_fields{this, "signalSumFields", {}};
    // ref field ids are used for the merged hits, 0 is used if nothing provided
    Property<std::vector<int>>         u_refs{this, "fieldRefNumbers", {}};
    Property<std::string>              m_readout{this, "readoutClass", ""};

    double eRes[3] = {0., 0., 0.};
    uint64_t id_mask{0}, ref_mask{0};

    const GeoSvc& m_geoSvc = GeoSvc::instance();
    const RandomSvc& m_randomSvc = RandomSvc::instance();
  };

} // namespace algoriths::calorimetry