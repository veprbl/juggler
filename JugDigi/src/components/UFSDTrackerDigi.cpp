#include <algorithm>

#include "GaudiAlg/Transformer.h"
#include "GaudiAlg/GaudiTool.h"
#include "GaudiKernel/RndmGenerators.h"
#include "GaudiKernel/Property.h"

// FCCSW
#include "JugBase/DataHandle.h"

// Event Model related classes
//#include "GaudiExamples/MyTrack.h"
#include "eicd/RawTrackerHitCollection.h"
#include "dd4pod/TrackerHitCollection.h"

namespace Jug {
  namespace Digi {
  
    /** Ultra-fast silicon detector digitization.
     *
     *
     */
   class UFSDTrackerDigi : public GaudiAlgorithm {
   public:
    //  ill-formed: using GaudiAlgorithm::GaudiAlgorithm;
    UFSDTrackerDigi(const std::string& name, ISvcLocator* svcLoc)
        : GaudiAlgorithm(name, svcLoc) {
          declareProperty("inputHitCollection", m_inputHitCollection,"");
          declareProperty("outputHitCollection", m_outputHitCollection, "");
        }
    StatusCode initialize() override {
      if (GaudiAlgorithm::initialize().isFailure())
        return StatusCode::FAILURE;
      IRndmGenSvc* randSvc = svc<IRndmGenSvc>("RndmGenSvc", true);
      StatusCode   sc      = m_gaussDist.initialize( randSvc, Rndm::Gauss(0.0, m_timeResolution.value()));
      if (!sc.isSuccess()) {
        return StatusCode::FAILURE;
      }
      return StatusCode::SUCCESS;
    }
    StatusCode execute() override {
      // input collection
      const dd4pod::TrackerHitCollection* simhits = m_inputHitCollection.get();
      // Create output collections
      auto rawhits = m_outputHitCollection.createAndPut();
      eic::RawTrackerHitCollection* rawHitCollection = new eic::RawTrackerHitCollection();
      std::map<long long,int> cell_hit_map;
      for(const auto& ahit : *simhits) {
        //std::cout << ahit << "\n";
        if(cell_hit_map.count(ahit.cellID()) == 0) {
          cell_hit_map[ahit.cellID()] = rawhits->size();
          eic::RawTrackerHit rawhit((long long)ahit.cellID(), ahit.truth().time*1000, (int)ahit.energyDeposit() * 10000);
          rawhits->push_back(rawhit);
        } else {
           auto hit = (*rawhits)[cell_hit_map[ahit.cellID()]];
           auto ch = hit.charge();
           hit.charge( ch  + (int)ahit.energyDeposit() * 10000);
           hit.time(ahit.truth().time*1000+m_gaussDist());
        }
      }
      return StatusCode::SUCCESS;
    }

    Gaudi::Property<double>      m_timeResolution{this, "timeResolution", 10};
    Rndm::Numbers m_gaussDist;
    DataHandle<dd4pod::TrackerHitCollection> m_inputHitCollection{"inputHitCollection", Gaudi::DataHandle::Reader, this};
    DataHandle<eic::RawTrackerHitCollection> m_outputHitCollection{"outputHitCollection", Gaudi::DataHandle::Writer, this};
  };
  DECLARE_COMPONENT(UFSDTrackerDigi)

  // class DataProducerProp : public GaudiAlgorithm {
  // public:
  //  //  ill-formed: using GaudiAlgorithm::GaudiAlgorithm;
  //  DataProducerProp(const std::string& name, ISvcLocator* svcLoc)
  //      : GaudiAlgorithm(name, svcLoc) {}
  //  StatusCode initialize() override {
  //    f_counter = m_starting_value.value();
  //    return StatusCode::SUCCESS;
  //  }
  //  StatusCode execute() override {
  //    m_vec.put(ThreeVecEx{f_counter, f_counter + 1, f_counter + 2});

  //    info() << "executing DataProducer (prop): " << f_counter << " ..."
  //           << endmsg;
  //    f_counter++;
  //    return StatusCode::SUCCESS;
  //  }

  // private:
  //  Gaudi::Property<int>      m_starting_value{this, "StartingValue", 0};
  //  AnyDataHandle<ThreeVecEx> m_vec{"/Event/UnknownVec2",
  //                                  Gaudi::DataHandle::Writer, this};
  //  int                       f_counter = 0;
  //};

  // DECLARE_COMPONENT(DataProducerProp)

  // class DataConsumerProp : public GaudiAlgorithm {
  // public:
  //  // using GaudiAlgorithm::GaudiAlgorithm;
  //  DataConsumerProp(const std::string& name, ISvcLocator* svcLoc)
  //      : GaudiAlgorithm(name, svcLoc) {}
  //  StatusCode execute() override {
  //    info() << "executing DataConsumer (prop): {" << *m_vec.get() << "}"
  //           << endmsg;
  //    return StatusCode::SUCCESS;
  //  }

  // private:
  //  AnyDataHandle<ThreeVecEx> m_vec{"/Event/UnknownVec2",
  //                                  Gaudi::DataHandle::Reader, this};
  //};

  // DECLARE_COMPONENT(DataConsumerProp)

  } // namespace Examples
} // namespace Gaudi
