// This file is part of the Acts project.
//
// Copyright (C) 2019 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

//#include "HitSmearing.hpp"

#include "JugReco/GeometryContainers.hpp"

// Gaudi
#include "GaudiAlg/GaudiAlgorithm.h"
#include "GaudiKernel/ToolHandle.h"
#include "GaudiAlg/Transformer.h"
#include "GaudiAlg/GaudiTool.h"
#include "GaudiKernel/RndmGenerators.h"
#include "GaudiKernel/Property.h"

#include "JugBase/DataHandle.h"
#include "JugBase/IGeoSvc.h"

#include "DDRec/CellIDPositionConverter.h"
#include "DDRec/SurfaceManager.h"
#include "DDRec/Surface.h"

#include "Acts/Utilities/Units.hpp"
#include "Acts/Utilities/Definitions.hpp"
#include "Acts/Geometry/TrackingGeometry.hpp"
#include "Acts/Plugins/DD4hep/DD4hepDetectorElement.hpp"

#include "JugReco/SourceLinks.h"

#include "eicd/TrackerHitCollection.h"

namespace Jug::Reco {

  class TrackerSourceLinker : public GaudiAlgorithm {
  public:
    Gaudi::Property<double>               m_timeResolution{this, "timeResolution", 10};
    Rndm::Numbers                         m_gaussDist;
    DataHandle<eic::TrackerHitCollection>    m_inputHitCollection{"inputHitCollection", Gaudi::DataHandle::Reader, this};
    DataHandle<SourceLinkContainer> m_outputSourceLinks{"outputSourceLinks", Gaudi::DataHandle::Writer, this};
    /// Pointer to the geometry service
    SmartIF<IGeoSvc> m_geoSvc;

    /// Lookup container for hit surfaces that generate smeared hits
    std::unordered_map<uint64_t, const Acts::Surface*> m_surfaces;

  public:
    TrackerSourceLinker(const std::string& name, ISvcLocator* svcLoc)
        : GaudiAlgorithm(name, svcLoc) {
      declareProperty("inputHitCollection", m_inputHitCollection, "");
      declareProperty("outputSourceLinks", m_outputSourceLinks, "");
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
      IRndmGenSvc* randSvc = svc<IRndmGenSvc>("RndmGenSvc", true);
      StatusCode   sc = m_gaussDist.initialize(randSvc, Rndm::Gauss(0.0, m_timeResolution.value()));
      if (!sc.isSuccess()) {
        return StatusCode::FAILURE;
      }
      debug() << "visiting all the surfaces  " << endmsg;
      m_geoSvc->trackingGeometry()->visitSurfaces([this](const Acts::Surface* surface) {
        // for now we just require a valid surface
        if (not surface) {
          return;
        }
        auto det_element = dynamic_cast<const Acts::DD4hepDetectorElement*>(surface->associatedDetectorElement());
        if(!det_element) {
          debug() << "invalid det_element!!! " << endmsg;
          return;
        }
        this->m_surfaces.insert_or_assign(det_element->identifier(), surface);
      });

      return StatusCode::SUCCESS;
    }

    StatusCode execute() override {
      // input collection
      const eic::TrackerHitCollection* hits = m_inputHitCollection.get();
      // Create output collections
      auto source_links = m_outputSourceLinks.createAndPut();
      // setup local covariance
      // TODO add support for per volume/layer/module settings

      for(const auto& ahit : *hits) {

        Acts::BoundMatrix cov           = Acts::BoundMatrix::Zero();
        cov(Acts::eBoundLoc0, Acts::eBoundLoc0) = ahit.covMatrix(0)*Acts::UnitConstants::mm;//*ahit.covMatrix(0);
        cov(Acts::eBoundLoc1, Acts::eBoundLoc1) = ahit.covMatrix(1)*Acts::UnitConstants::mm;//*ahit.covMatrix(1);

        debug() << "cell ID : " << ahit.cellID() << endmsg;
        auto vol_ctx = m_geoSvc->cellIDPositionConverter()->findContext(ahit.cellID());
        auto vol_id = vol_ctx->identifier;
        debug() << " vol_id : " <<  vol_id << endmsg;
        debug() << " hit : " <<  ahit << endmsg;

        const auto is = m_surfaces.find(vol_id);
        if (is == m_surfaces.end()) {
          continue;
        }
        const Acts::Surface* surface = is->second;

        // transform global position into local coordinates
        Acts::Vector2D pos(0, 0);
        // geometry context contains nothing here
        surface->globalToLocal(Acts::GeometryContext(), {ahit.position(0), ahit.position(1), ahit.position(2)},
                               {0, 0, 0});//, pos);

        //// smear truth to create local measurement
        Acts::BoundVector loc = Acts::BoundVector::Zero();
        //loc[Acts::eLOC_0]     = pos[0] + m_cfg.sigmaLoc0 * stdNormal(rng);
        //loc[Acts::eLOC_1]     = pos[1] + m_cfg.sigmaLoc1 * stdNormal(rng);

        // create source link at the end of the container
        //auto it = source_links->emplace_hint(source_links->end(), *surface, hit, 2, loc, cov);
        auto it = source_links->emplace_hint(source_links->end(), *surface,  2, loc, cov);
        // ensure hits and links share the same order to prevent ugly surprises
        if (std::next(it) != source_links->end()) {
          error() << "The hit ordering broke. Run for your life." << endmsg;
          return StatusCode::FAILURE;
        }

        //std::array<double,3> posarr; pos.GetCoordinates(posarr);
        //std::array<double,3> dimarr; dim.GetCoordinates(posarr);
        //eic::TrackerHit hit;
        //eic::TrackerHit hit((long long)ahit.cellID0(), (long long)ahit.cellID(), (long long)ahit.time(),
        //                    (float)ahit.charge() / 10000.0, (float)0.0, {{pos.x(), pos.y(),pos.z()}},{{dim[0],dim[1],0.0}});
        //rec_hits->push_back(hit);
      }
      return StatusCode::SUCCESS;
    }

   };
  DECLARE_COMPONENT(TrackerSourceLinker)

} // namespace Jug::reco

//HitSmearing::HitSmearing(const Config& cfg, Acts::Logging::Level lvl)
//    : BareAlgorithm("HitSmearing", lvl), m_cfg(cfg) {
//  if (m_cfg.inputSimulatedHits.empty()) {
//    throw std::invalid_argument("Missing input simulated hits collection");
//  }
//  if (m_cfg.outputSourceLinks.empty()) {
//    throw std::invalid_argument("Missing output source links collection");
//  }
//  if ((m_cfg.sigmaLoc0 < 0) or (m_cfg.sigmaLoc1 < 0)) {
//    throw std::invalid_argument("Invalid resolution setting");
//  }
//  if (not m_cfg.trackingGeometry) {
//    throw std::invalid_argument("Missing tracking geometry");
//  }
//  if (!m_cfg.randomNumbers) {
//    throw std::invalid_argument("Missing random numbers tool");
//  }
//  // fill the surface map to allow lookup by geometry id only
//  m_cfg.trackingGeometry->visitSurfaces([this](const Acts::Surface* surface) {
//    // for now we just require a valid surface
//    if (not surface) {
//      return;
//    }
//    this->m_surfaces.insert_or_assign(surface->associatedDetectorElement()->identifier(), surface);
//  });
//     }
//}

//ProcessCode HitSmearing::execute(const AlgorithmContext& ctx) const {
//  // setup input and output containers
//  const auto& hits =
//      ctx.eventStore.get<SimHitContainer>(m_cfg.inputSimulatedHits);
//  SimSourceLinkContainer sourceLinks;
//  sourceLinks.reserve(hits.size());
//
//  // setup random number generator
//  auto rng = m_cfg.randomNumbers->spawnGenerator(ctx);
//  std::normal_distribution<double> stdNormal(0.0, 1.0);
//
//  // setup local covariance
//  // TODO add support for per volume/layer/module settings
//  Acts::BoundMatrix cov = Acts::BoundMatrix::Zero();
//  cov(Acts::eLOC_0, Acts::eLOC_0) = m_cfg.sigmaLoc0 * m_cfg.sigmaLoc0;
//  cov(Acts::eLOC_1, Acts::eLOC_1) = m_cfg.sigmaLoc1 * m_cfg.sigmaLoc1;
//
//  for (auto&& [moduleGeoId, moduleHits] : groupByModule(hits)) {
//    // check if we should create hits for this surface
//    const auto is = m_surfaces.find(moduleGeoId);
//    if (is == m_surfaces.end()) {
//      continue;
//    }
//
//    // smear all truth hits for this module
//    const Acts::Surface* surface = is->second;
//    for (const auto& hit : moduleHits) {
//      // transform global position into local coordinates
//      Acts::Vector2D pos(0, 0);
//      surface->globalToLocal(ctx.geoContext, hit.position(),
//                             hit.unitDirection(), pos);
//
//      // smear truth to create local measurement
//      Acts::BoundVector loc = Acts::BoundVector::Zero();
//      loc[Acts::eLOC_0] = pos[0] + m_cfg.sigmaLoc0 * stdNormal(rng);
//      loc[Acts::eLOC_1] = pos[1] + m_cfg.sigmaLoc1 * stdNormal(rng);
//
//      // create source link at the end of the container
//      auto it = sourceLinks.emplace_hint(sourceLinks.end(), *surface, hit, 2,
//                                         loc, cov);
//      // ensure hits and links share the same order to prevent ugly surprises
//      if (std::next(it) != sourceLinks.end()) {
//        ACTS_FATAL("The hit ordering broke. Run for your life.");
//        return ProcessCode::ABORT;
//      }
//    }
//  }
//
//  ctx.eventStore.add(m_cfg.outputSourceLinks, std::move(sourceLinks));
//  return ProcessCode::SUCCESS;
//}