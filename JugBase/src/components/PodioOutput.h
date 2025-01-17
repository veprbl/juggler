// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2022 Whitney Armstrong, Wouter Deconinck

#ifndef JUGBASE_PODIOOUTPUT_H
#define JUGBASE_PODIOOUTPUT_H

#include "JugBase/KeepDropSwitch.h"
#include "GaudiAlg/GaudiAlgorithm.h"
#include "podio/CollectionBase.h"

#include "TTree.h"

#include <vector>
#include <gsl/gsl>

// forward declarations
class TFile;
class PodioDataSvc;

class PodioOutput : public GaudiAlgorithm {

public:
  /// Constructor.
  PodioOutput(const std::string& name, ISvcLocator* svcLoc);

  /// Initialization of PodioOutput. Acquires the data service, creates trees and root file.
  virtual StatusCode initialize();
  /// Execute. For the first event creates branches for all collections known to PodioDataSvc and prepares them for
  /// writing. For the following events it reconnects the branches with collections and prepares them for write.
  virtual StatusCode execute();
  /// Finalize. Writes the meta data tree; writes file and cleans up all ROOT-pointers.
  virtual StatusCode finalize();

private:
  void resetBranches(const std::vector<std::pair<std::string, podio::CollectionBase*>>& collections);
  void createBranches(const std::vector<std::pair<std::string, podio::CollectionBase*>>& collections);
  /// First event or not
  bool m_firstEvent;
  /// Root file name the output is written to
  Gaudi::Property<std::string> m_filename{this, "filename", "output.root", "Name of the file to create"};
  /// Commands which output is to be kept
  Gaudi::Property<std::vector<std::string>> m_outputCommands{
      this, "outputCommands", {"keep *"}, "A set of commands to declare which collections to keep or drop."};
  Gaudi::Property<std::string> m_filenameRemote{
      this, "filenameRemote", "", "An optional file path to copy the outputfile to."};
  /// Switch for keeping or dropping outputs
  KeepDropSwitch m_switch;
  /// Needed for collection ID table
  PodioDataSvc* m_podioDataSvc;
  /// The actual ROOT file
  std::unique_ptr<TFile> m_file;
  /// The tree to be filled with collections
  gsl::owner<TTree*> m_datatree;
  /// The tree to be filled with meta data
  gsl::owner<TTree*> m_metadatatree;
  gsl::owner<TTree*> m_runMDtree;
  gsl::owner<TTree*> m_evtMDtree;
  gsl::owner<TTree*> m_colMDtree;
  /// The stored collections
  std::vector<podio::CollectionBase*> m_storedCollections;
  std::vector<std::tuple<int, std::string, bool>> m_collectionInfo;
};

#endif
