#include "JugTrack/GeometryContainers.hpp"

// Gaudi
#include "GaudiAlg/GaudiAlgorithm.h"
#include "GaudiKernel/ToolHandle.h"
#include "GaudiAlg/Transformer.h"
#include "GaudiAlg/GaudiTool.h"
#include "GaudiKernel/RndmGenerators.h"
#include "Gaudi/Property.h"

#include "JugBase/DataHandle.h"
#include "JugBase/IGeoSvc.h"

#include "DDRec/CellIDPositionConverter.h"
#include "DDRec/SurfaceManager.h"
#include "DDRec/Surface.h"
#include "DD4hep/Volumes.h"
#include "DD4hep/DD4hepUnits.h"


#include "Acts/Surfaces/Surface.hpp"
#include "Acts/Definitions/Units.hpp"
#include "Acts/Definitions/Common.hpp"
#include "Acts/Geometry/TrackingGeometry.hpp"
#include "Acts/Plugins/DD4hep/DD4hepDetectorElement.hpp"

#include "JugTrack/Index.hpp"
#include "JugTrack/IndexSourceLink.hpp"
#include "JugTrack/Measurement.hpp"

#include "eicd/TrackerHitCollection.h"

namespace Jug::Reco {

  /** Source source Linker.
   *
   * \ingroup track
   * \ingroup tracking
   */
  class TrackerSourceLinker : public GaudiAlgorithm {
  public:
    DataHandle<eic::TrackerHitCollection>    m_inputHitCollection{"inputHitCollection", Gaudi::DataHandle::Reader, this};
    DataHandle<IndexSourceLinkContainer>     m_outputSourceLinks{"outputSourceLinks", Gaudi::DataHandle::Writer, this};
    DataHandle<MeasurementContainer>          m_outputMeasurements{"outputMeasurements", Gaudi::DataHandle::Writer, this};
    /// Pointer to the geometry service
    SmartIF<IGeoSvc> m_geoSvc;

  public:
    TrackerSourceLinker(const std::string& name, ISvcLocator* svcLoc)
        : GaudiAlgorithm(name, svcLoc) {
      declareProperty("inputHitCollection", m_inputHitCollection, "");
      declareProperty("outputSourceLinks", m_outputSourceLinks, "");
      declareProperty("outputMeasurements", m_outputMeasurements, "");
    }

    StatusCode initialize() override {
      if (GaudiAlgorithm::initialize().isFailure())
        return StatusCode::FAILURE;
      m_geoSvc = service("GeoSvc");
      if (!m_geoSvc) {
        error() << "Unable to locate Geometry Service. "
                << "Make sure you have GeoSvc and SimSvc in the right order in the configuration."
                << endmsg;
        return StatusCode::FAILURE;
      }

      return StatusCode::SUCCESS;
    }

    StatusCode execute() override {
      // input collection
      const eic::TrackerHitCollection* hits = m_inputHitCollection.get();
      // Create output collections
      auto sourceLinks = m_outputSourceLinks.createAndPut();
      auto measurements = m_outputMeasurements.createAndPut();
      // IndexMultimap<ActsFatras::Barcode> hitParticlesMap;
      // IndexMultimap<Index> hitSimHitsMap;
      sourceLinks->reserve(hits->size());
      measurements->reserve(hits->size());

      debug() << (*hits).size() << " hits " << endmsg;
      int ihit = 0;
      for(const auto& ahit : *hits) {

        Acts::SymMatrix2 cov = Acts::SymMatrix2::Zero();
        cov(0,0) = ahit.covMatrix().xx*Acts::UnitConstants::mm;//*ahit.xx()*Acts::UnitConstants::mm;
        cov(1,1) = ahit.covMatrix().yy*Acts::UnitConstants::mm;//*ahit.yy()*Acts::UnitConstants::mm;
        debug() << "cov matrix:\n" << cov << endmsg;

        auto vol_ctx   = m_geoSvc->cellIDPositionConverter()->findContext(ahit.cellID());
        auto vol_id    = vol_ctx->identifier;
        auto volman    = m_geoSvc->detector()->volumeManager();
        auto alignment = volman.lookupDetElement(vol_id).nominal();
        auto local_position =
            alignment.worldToLocal({ahit.position().x / 10, ahit.position().y / 10, ahit.position().z / 10});
        // note the factor of 10 above goes from mm to cm

        const auto is      = m_geoSvc->surfaceMap().find(vol_id);
        if (is == m_geoSvc->surfaceMap().end()) {
          error() << " vol_id (" <<  vol_id << ")  not found in m_surfaces." <<endmsg;
          continue;
        }
        const Acts::Surface* surface = is->second;
        auto surf_center = surface->center(Acts::GeometryContext());

        // transform global position into local coordinates
        // geometry context contains nothing here
        Acts::Vector2 pos = surface->globalToLocal(Acts::GeometryContext(), {ahit.position().x, ahit.position().y, ahit.position().z}, {0, 0, 0}).value();//, pos);

        debug() << "dd4hep loc pos   : " <<  local_position.x() << " "<< local_position.y() << " " << local_position.z()  << endmsg;
        debug() << "   surface center:" << surface->center(Acts::GeometryContext()).transpose() << endmsg;
        debug() << "acts local center:" << pos.transpose() << endmsg;

        Acts::Vector2 loc = Acts::Vector2::Zero();
        loc[Acts::eBoundLoc0]     = pos[0] ;//+ m_cfg.sigmaLoc0 * stdNormal(rng);
        loc[Acts::eBoundLoc1]     = pos[1] ;//+ m_cfg.sigmaLoc1 * stdNormal(rng);
        debug() << "     acts loc pos: (" << loc[Acts::eBoundLoc0] << ", " << loc[Acts::eBoundLoc1] << ")" << endmsg;

        //local position
        //auto loc = {ahit.x(), ahit.y(), ahit.z()} - vol_ctx->volumePlacement().position()
        //debug() << " hit          : \n" <<  ahit << endmsg;
        //debug() << " cell ID : " << ahit.cellID() << endmsg;
        //debug() << " position : (" <<  ahit.position(0) << ", " <<  ahit.position(1) << ", "<<  ahit.position(2) << ") " << endmsg;
        //debug() << " vol_id       : " <<  vol_id << endmsg;
        //debug() << " placment pos : " << vol_ctx->volumePlacement().position() << endmsg;

        // the measurement container is unordered and the index under which the
        // measurement will be stored is known before adding it.
        Index hitIdx = measurements->size();
        IndexSourceLink sourceLink(surface->geometryId(), ihit);
        auto meas = Acts::makeMeasurement(sourceLink, loc, cov, Acts::eBoundLoc0, Acts::eBoundLoc1);

        // add to output containers. since the input is already geometry-order,
        // new elements in geometry containers can just be appended at the end.
        sourceLinks->emplace_hint(sourceLinks->end(), std::move(sourceLink));
        measurements->emplace_back(std::move(meas));

        ihit++;
      }
      return StatusCode::SUCCESS;
    }

   };
  DECLARE_COMPONENT(TrackerSourceLinker)

} // namespace Jug::reco

