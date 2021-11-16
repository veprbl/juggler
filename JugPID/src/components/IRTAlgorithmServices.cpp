
#include <map>

#include <math.h>

#include "IRTAlgorithmServices.h"

#include <IRT/ParametricSurface.h>

// -------------------------------------------------------------------------------------

void Jug::PID::IRTAlgorithmServices::configure_QE_lookup_table(const std::vector<std::pair<double, 
							       double>> &QE_vector, unsigned nbins)
{
  // Well, could have probably just reordered the initial vector;
  std::map<double, double> QE_map;

  for(auto entry: QE_vector)
    QE_map[entry.first] = entry.second;

  // Sanity checks;
  if (QE_map.size() < 2 || nbins < 2) return;

  //m_QE_lookup_table.push_back(std::make_pair((*QE_map.begin()).first , 0.3));
  //m_QE_lookup_table.push_back(std::make_pair((*QE_map.rbegin()).first, 0.3)); 

  double from = (*QE_map.begin()).first;
  double to   = (*QE_map.rbegin()).first;
  // Will be "nbins+1" equidistant entries;
  double step = (to - from) / nbins;

  // Just in case somebody considers to call this method twice :-);
  m_QE_lookup_table.clear();

  for(auto entry: QE_map) {
    double e1 = entry.first;
    double qe1 = entry.second;

    if (!m_QE_lookup_table.size())
      m_QE_lookup_table.push_back(std::make_pair(e1, qe1));
    else {
      const auto &prev = m_QE_lookup_table[m_QE_lookup_table.size()-1];

      double e0 = prev.first;
      double qe0 = prev.second;
      double a = (qe1 - qe0) / (e1 - e0);
      double b = qe0 - a*e0;
      // FIXME: check floating point accuracy when moving to a next point; do we actually 
      // care whether the overall number of bins will be "nbins+1" or more?;
      for(double e = e0+step; e<e1; e+=step)
	m_QE_lookup_table.push_back(std::make_pair(e, a*e + b));
    } //if
  } //for entry

  //for(auto entry: m_QE_lookup_table) 
  //printf("%7.2f -> %7.2f\n", entry.first, entry.second);
} // Jug::PID::IRTAlgorithmServices::configure_QE_lookup_table()

// -------------------------------------------------------------------------------------

bool Jug::PID::IRTAlgorithmServices::QE_pass(double ev, double rand) const
{
  // Get the tabulated table reference; perform sanity checks;
  //const std::vector<std::pair<double, double>> &qe = u_quantumEfficiency.value();
  unsigned dim = m_QE_lookup_table.size(); if (dim < 2) return false;

  // Find a proper bin; no tricks, they are all equidistant;
  auto const &from = m_QE_lookup_table[0];
  auto const &to = m_QE_lookup_table[dim-1];
  double emin = from.first;
  double emax = to.first;
  double step = (emax - emin) / (dim - 1);
  int ibin = (int)floor((ev - emin) / step);

  //printf("%f vs %f, %f -> %d\n", ev, from.first, to. first, ibin);
  
  // Out of range check;
  if (ibin < 0 || ibin >= int(dim)) return false;

  // Get tabulated QE value, compare against the provided random variable;
  return (rand <= m_QE_lookup_table[ibin].second);
} // Jug::PID::IRTAlgorithmServices::QE_pass()

// -------------------------------------------------------------------------------------

double Jug::PID::IRTAlgorithmServices::GetDistance(const ParametricSurface *surface, 
							const eic::TrajectoryPoint *point) const
{
  if (!surface || !point) return 0.0;

  auto pt = TVector3(point->position.x, point->position.y, point->position.z);
  
  return surface->GetDistance(TVector3(point->position.x, point->position.y, point->position.z));
} // Jug::PID::IRTAlgorithmServices::GetDistance()

// -------------------------------------------------------------------------------------

bool Jug::PID::IRTAlgorithmServices::GetCrossing(const ParametricSurface *surface, 
						      const eic::TrajectoryPoint *point, 
						      TVector3 *crs) const
{
  if (!surface || !point) return false;
  
  return surface->GetCrossing(GetLocation(point), GetMomentum(point).Unit(), crs);
} // Jug::PID::IRTAlgorithmServices::GetCrossing()

// -------------------------------------------------------------------------------------

TVector3 Jug::PID::IRTAlgorithmServices::GetLocation(const eic::TrajectoryPoint *point) const
{
  return TVector3(point->position.x, point->position.y, point->position.z);
} // Jug::PID::IRTAlgorithmServices::GetLocation()

// -------------------------------------------------------------------------------------

TVector3 Jug::PID::IRTAlgorithmServices::GetMomentum(const eic::TrajectoryPoint *point) const
{
  return TVector3(point->p.x, point->p.y, point->p.z);
} // Jug::PID::IRTAlgorithmServices::GetMomentum()

// -------------------------------------------------------------------------------------

// Search a boolean solid's composition tree for primitive of type `typeName` (e.g., `TGeoSphere`)
// - `prim` will be set to the primitive; can be empty initially
// - `pos` will be set to the primitive's position (careful, it only considers translation of the operands);
//   should be initially set to the position of `sol`
// - `matx` stores operand transformations; you do not need to set it initially
void Jug::PID::IRTAlgorithmServices::findPrimitive(
    const std::string typeName, const dd4hep::Solid sol,
    dd4hep::Solid &prim, dd4hep::Position &pos, const TGeoMatrix *matx
) const {
  if(sol->IsComposite()) {
    auto node = (dd4hep::BooleanSolid) sol;
    findPrimitive(typeName, node.leftShape(),  prim, pos, node.leftMatrix()  );
    findPrimitive(typeName, node.rightShape(), prim, pos, node.rightMatrix() );
  } else if(typeName.compare(sol.type())==0) {
    prim = sol;
    //matx->Print();
    dd4hep::Position translation;
    translation.SetCoordinates(matx->GetTranslation());
    pos += translation;
  }
}

// -------------------------------------------------------------------------------------