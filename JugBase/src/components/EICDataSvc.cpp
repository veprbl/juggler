#include "EICDataSvc.h"

// Instantiation of a static factory class used by clients to create
// instances of this service
DECLARE_COMPONENT(EICDataSvc)

/// Standard Constructor
EICDataSvc::EICDataSvc(const std::string& name, ISvcLocator* svc) : PodioDataSvc(name, svc) {
  declareProperty("tree", m_treename = "", "Names of the tree read");
  declareProperty("inputs", m_filenames = {}, "Names of the files to read");
  declareProperty("input", m_filename = "", "Name of the file to read");
}

/// Standard Destructor
EICDataSvc::~EICDataSvc() {}