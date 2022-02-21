/*
 *  Topological Cell Clustering Algorithm for Imaging Calorimetry
 *  1. group all the adjacent pixels
 *
 *  Author: Chao Peng (ANL), 06/02/2021
 *  References: https://arxiv.org/pdf/1603.02934.pdf
 *
 */
#include "fmt/format.h"
#include <algorithm>

#include "Gaudi/Property.h"
#include "GaudiAlg/GaudiAlgorithm.h"
#include "GaudiAlg/GaudiTool.h"
#include "GaudiAlg/Transformer.h"
#include "GaudiKernel/PhysicalConstants.h"
#include "GaudiKernel/RndmGenerators.h"
#include "GaudiKernel/ToolHandle.h"

#include "DD4hep/BitFieldCoder.h"
#include "DDRec/CellIDPositionConverter.h"
#include "DDRec/Surface.h"
#include "DDRec/SurfaceManager.h"

#include "JugBase/DataHandle.h"
#include "JugBase/IGeoSvc.h"
#include "JugReco/ClusterType.h"

// Event Model related classes
#include "eicd/ProtoClusterCollection.h"
#include "eicd/CalorimeterHitCollection.h"

using namespace Gaudi::Units;

namespace Jug::Reco {

  /** Topological Cell Clustering Algorithm.
   *
   * Topological Cell Clustering Algorithm for Imaging Calorimetry
   *  1. group all the adjacent pixels
   *
   *  Author: Chao Peng (ANL), 06/02/2021
   *  References: https://arxiv.org/pdf/1603.02934.pdf
   *
   * \ingroup reco
   */
  class ImagingTopoCluster : public GaudiAlgorithm {
  public:
    // maximum difference in layer numbers that can be considered as neighbours
    Gaudi::Property<int> m_neighbourLayersRange{this, "neighbourLayersRange", 1};
    // maximum distance of local (x, y) to be considered as neighbors at the same layer
    Gaudi::Property<std::vector<double>> u_localDistXY{this, "localDistXY", {1.0 * mm, 1.0 * mm}};
    // maximum distance of global (eta, phi) to be considered as neighbors at different layers
    Gaudi::Property<std::vector<double>> u_layerDistEtaPhi{this, "layerDistEtaPhi", {0.01, 0.01}};
    // maximum global distance to be considered as neighbors in different sectors
    Gaudi::Property<double> m_sectorDist{this, "sectorDist", 1.0 * cm};

    // minimum hit energy to participate clustering
    Gaudi::Property<double> m_minClusterHitEdep{this, "minClusterHitEdep", 0.};
    // minimum cluster center energy (to be considered as a seed for cluster)
    Gaudi::Property<double> m_minClusterCenterEdep{this, "minClusterCenterEdep", 0.};
    // minimum cluster energy (to save this cluster)
    Gaudi::Property<double> m_minClusterEdep{this, "minClusterEdep", 0.5 * MeV};
    // minimum number of hits (to save this cluster)
    Gaudi::Property<int> m_minClusterNhits{this, "minClusterNhits", 10};
    // input hits collection
    DataHandle<eic::CalorimeterHitCollection> m_inputHitCollection{"inputHitCollection",
                                                                   Gaudi::DataHandle::Reader, this};
    // output clustered hits
    DataHandle<eic::ProtoClusterCollection> m_outputProtoClusterCollection{"outputProtoClusterCollection",
                                                                           Gaudi::DataHandle::Writer, this};

    // unitless counterparts of the input parameters
    double localDistXY[2], layerDistEtaPhi[2], sectorDist;
    double minClusterHitEdep, minClusterCenterEdep, minClusterEdep, minClusterNhits;

    ImagingTopoCluster(const std::string& name, ISvcLocator* svcLoc) 
      : GaudiAlgorithm(name, svcLoc)
    {
      declareProperty("inputHitCollection", m_inputHitCollection, "");
      declareProperty("outputProtoClusterCollection", m_outputProtoClusterCollection, "");
    }

    StatusCode initialize() override
    {
      if (GaudiAlgorithm::initialize().isFailure()) {
        return StatusCode::FAILURE;
      }

      // unitless conversion
      // sanity checks
      if (u_localDistXY.size() != 2) {
        error() << "Expected 2 values (x_dist, y_dist) for localDistXY" << endmsg;
        return StatusCode::FAILURE;
      }
      if (u_layerDistEtaPhi.size() != 2) {
        error() << "Expected 2 values (eta_dist, phi_dist) for layerDistEtaPhi" << endmsg;
        return StatusCode::FAILURE;
      }

      // using juggler internal units (GeV, mm, ns, rad)
      localDistXY[0] = u_localDistXY.value()[0]/mm;
      localDistXY[1] = u_localDistXY.value()[1]/mm;
      layerDistEtaPhi[0] = u_layerDistEtaPhi.value()[0];
      layerDistEtaPhi[1] = u_layerDistEtaPhi.value()[1]/rad;
      sectorDist = m_sectorDist.value()/mm;
      minClusterHitEdep = m_minClusterHitEdep.value()/GeV;
      minClusterCenterEdep = m_minClusterCenterEdep.value()/GeV;
      minClusterEdep = m_minClusterEdep.value()/GeV;

      // summarize the clustering parameters
      info() << fmt::format("Local clustering (same sector and same layer): "
                            "Local [x, y] distance between hits <= [{:.4f} mm, {:.4f} mm].",
                            localDistXY[0], localDistXY[1]) << endmsg;
      info() << fmt::format("Neighbour layers clustering (same sector and layer id within +- {:d}: "
                            "Global [eta, phi] distance between hits <= [{:.4f}, {:.4f} rad].",
                            m_neighbourLayersRange.value(), layerDistEtaPhi[0], layerDistEtaPhi[1]) << endmsg;
      info() << fmt::format("Neighbour sectors clustering (different sector): "
                            "Global distance between hits <= {:.4f} mm.",
                            sectorDist) << endmsg;

      return StatusCode::SUCCESS;
    }

    StatusCode execute() override
    {
      // input collections
      const auto& hits = *m_inputHitCollection.get();
      // Create output collections
      auto& proto = *m_outputProtoClusterCollection.createAndPut();

      // group neighboring hits
      std::vector<bool>                           visits(hits.size(), false);
      std::vector<std::vector<std::pair<uint32_t, eic::ConstCalorimeterHit>>> groups;
      for (size_t i = 0; i < hits.size(); ++i) {
        if (msgLevel(MSG::DEBUG)) {
          debug() << fmt::format("hit {:d}: local position = ({}, {}, {}), global position = ({}, {}, {})", i + 1,
                                 hits[i].local().x, hits[i].local().y, hits[i].position().z, hits[i].position().x,
                                 hits[i].position().y, hits[i].position().z)
                  << endmsg;
        }
        // already in a group, or not energetic enough to form a cluster
        if (visits[i] || hits[i].energy() < minClusterCenterEdep) {
          continue;
        }
        groups.emplace_back();
        // create a new group, and group all the neighboring hits
        dfs_group(groups.back(), i, hits, visits);
      }
      if (msgLevel(MSG::DEBUG)) {
        debug() << "found " << groups.size() << " potential clusters (groups of hits)" << endmsg;
        for (size_t i = 0; i < groups.size(); ++i) {
          debug() << fmt::format("group {}: {} hits", i, groups[i].size()) << endmsg;
        }
      }

      // form clusters
      for (const auto& group : groups) {
        if (static_cast<int>(group.size()) < m_minClusterNhits.value()) {
          continue;
        }
        double energy = 0.;
        for (const auto& [idx, hit] : group) {
          energy += hit.energy();
        }
        if (energy < minClusterEdep) {
          continue;
        }
        auto pcl = proto.create();
        for (const auto& [idx, hit] : group) {
          pcl.addhits(hit);
        }
      }

      return StatusCode::SUCCESS;
    }

  private:
    template <typename T>
    static inline T pow2(const T& x)
    {
      return x * x;
    }

    // helper function to group hits
    bool is_neighbor(const eic::ConstCalorimeterHit& h1, const eic::ConstCalorimeterHit& h2) const
    {
      // different sectors, simple distance check
      if (h1.sector() != h2.sector()) {
        return std::sqrt(pow2(h1.position().x - h2.position().x) + pow2(h1.position().y - h2.position().y) + pow2(h1.position().z - h2.position().z)) <=
               sectorDist;
      }

      // layer check
      int ldiff = std::abs(h1.layer() - h2.layer());
      // same layer, check local positions
      if (!ldiff) {
        return (std::abs(h1.local().x - h2.local().x) <= localDistXY[0]) &&
               (std::abs(h1.local().y - h2.local().y) <= localDistXY[1]);
      } else if (ldiff <= m_neighbourLayersRange) {
        return (std::abs(h1.position().eta() - h2.position().eta()) <= layerDistEtaPhi[0]) &&
               (std::abs(h1.position().phi() - h2.position().phi()) <= layerDistEtaPhi[1]);
      }

      // not in adjacent layers
      return false;
    }

    // grouping function with Depth-First Search
    void dfs_group(std::vector<std::pair<uint32_t, eic::ConstCalorimeterHit>>& group, int idx,
                   const eic::CalorimeterHitCollection& hits, std::vector<bool>& visits) const
    {
      // not a qualified hit to participate in clustering, stop here
      if (hits[idx].energy() < minClusterHitEdep) {
        visits[idx] = true;
        return;
      }

      group.push_back({idx, hits[idx]});
      visits[idx] = true;
      for (size_t i = 0; i < hits.size(); ++i) {
        // visited, or not a neighbor
        if (visits[i] || !is_neighbor(hits[idx], hits[i])) {
          continue;
        }
        dfs_group(group, i, hits, visits);
      }
    }
  }; // namespace Jug::Reco

  DECLARE_COMPONENT(ImagingTopoCluster)

} // namespace Jug::Reco

