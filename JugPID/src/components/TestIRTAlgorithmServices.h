
#include <vector>

#include <TVector3.h>

#ifndef _TEST_IRT_ALGORITHM_SERVICES_
#define _TEST_IRT_ALGORITHM_SERVICES_

#include "eicd/TrajectoryPoint.h"
class ParametricSurface;

namespace Jug::Reco {

  // Wanto to split off the part, which knows nothing about Gaudi & Co; somehow this is 
  // less trivial than expected (need to pass the initial parameters separately);
  class TestIRTAlgorithmServices {
  public:
    TestIRTAlgorithmServices( void ) {};
    ~TestIRTAlgorithmServices( void ) {};

  protected:
    std::vector<std::pair<double, double>> m_QE_lookup_table;

    void configure_QE_lookup_table(const std::vector<std::pair<double, double>> &QE_vector, unsigned nbins);

    // Keep the same method name as in Chao's PhotoMultiplierDigi.cpp; however implement 
    // a fast equidistant array lookup instead;
    bool QE_pass(double ev, double rand) const;

    // FIXME: this crap needs to be optimized;
    double GetDistance(const ParametricSurface *surface, const eic::TrajectoryPoint *point) const;
    bool GetCrossing(const ParametricSurface *surface, const eic::TrajectoryPoint *point, 
		     TVector3 *crs) const;
    TVector3 GetLocation(const eic::TrajectoryPoint *point) const;
    TVector3 GetMomentum(const eic::TrajectoryPoint *point) const;
  };

} // namespace Jug::Reco

#endif
