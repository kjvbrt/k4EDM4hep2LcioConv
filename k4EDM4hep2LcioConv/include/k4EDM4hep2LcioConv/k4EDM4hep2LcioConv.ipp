#include "k4EDM4hep2LcioConv/MappingUtils.h"

#include <cassert>

namespace EDM4hep2LCIOConv {

  // Convert EDM4hep Tracks to LCIO
  // Add converted LCIO ptr and original EDM4hep collection to vector of pairs
  // Add LCIO Collection Vector to LCIO event
  template<typename TrackMapT, typename TrackerHitMapT>
  lcio::LCCollectionVec* convTracks(
    const edm4hep::TrackCollection* const tracks_coll,
    TrackMapT& tracks_vec,
    const TrackerHitMapT& trackerhits_vec)
  {
    auto* tracks = new lcio::LCCollectionVec(lcio::LCIO::TRACK);

    // Loop over EDM4hep tracks converting them to lcio tracks.
    for (const auto& edm_tr : (*tracks_coll)) {
      if (edm_tr.isAvailable()) {
        auto* lcio_tr = new lcio::TrackImpl();
        // The Type of the Tracks need to be set bitwise in LCIO since the setType(int) function is private for the LCIO
        // TrackImpl and only a setTypeBit(bitnumber) function can be used to set the Type bit by bit.
        int type = edm_tr.getType();
        for (int i = 0; i < sizeof(int) * 8; i++) {
          lcio_tr->setTypeBit(i, type & (1 << i));
        }
        lcio_tr->setChi2(edm_tr.getChi2());
        lcio_tr->setNdf(edm_tr.getNdf());
        lcio_tr->setdEdx(edm_tr.getDEdx());
        lcio_tr->setdEdxError(edm_tr.getDEdxError());
        lcio_tr->setRadiusOfInnermostHit(edm_tr.getRadiusOfInnermostHit());

#if EDM4HEP_BUILD_VERSION > EDM4HEP_VERSION(0, 9, 0)
        // Loop over the hit Numbers in the track
        lcio_tr->subdetectorHitNumbers().resize(edm_tr.subdetectorHitNumbers_size());
        for (int i = 0; i < edm_tr.subdetectorHitNumbers_size(); ++i) {
          lcio_tr->subdetectorHitNumbers()[i] = edm_tr.getSubdetectorHitNumbers(i);
        }

        // Pad until 50 hitnumbers are resized
        const int hit_number_limit = 50;
        if (edm_tr.subdetectorHitNumbers_size() < hit_number_limit) {
          lcio_tr->subdetectorHitNumbers().resize(hit_number_limit);
          for (int i = edm_tr.subdetectorHitNumbers_size(); i < hit_number_limit; ++i) {
            lcio_tr->subdetectorHitNumbers()[i] = 0;
          }
        }
#else
        // Loop over the hit Numbers in the track
        lcio_tr->subdetectorHitNumbers().resize(edm_tr.subDetectorHitNumbers_size());
        for (int i = 0; i < edm_tr.subDetectorHitNumbers_size(); ++i) {
          lcio_tr->subdetectorHitNumbers()[i] = edm_tr.getSubDetectorHitNumbers(i);
        }

        // Pad until 50 hitnumbers are resized
        const int hit_number_limit = 50;
        if (edm_tr.subDetectorHitNumbers_size() < hit_number_limit) {
          lcio_tr->subdetectorHitNumbers().resize(hit_number_limit);
          for (int i = edm_tr.subDetectorHitNumbers_size(); i < hit_number_limit; ++i) {
            lcio_tr->subdetectorHitNumbers()[i] = 0;
          }
        }
#endif

        // Link multiple associated TrackerHits if found in converted ones
        for (const auto& edm_rp_trh : edm_tr.getTrackerHits()) {
          if (edm_rp_trh.isAvailable()) {
            if (const auto lcio_trh = k4EDM4hep2LcioConv::detail::mapLookupFrom(edm_rp_trh, trackerhits_vec)) {
              lcio_tr->addHit(lcio_trh.value());
            }
          }
        }

        // Loop over the track states in the track
        const podio::RelationRange<edm4hep::TrackState> edm_track_states = edm_tr.getTrackStates();
        for (const auto& tr_state : edm_track_states) {
          const auto& cov = tr_state.covMatrix;
          std::array<float, 3> refP = {tr_state.referencePoint.x, tr_state.referencePoint.y, tr_state.referencePoint.z};

          auto* lcio_tr_state = new lcio::TrackStateImpl(
            tr_state.location,
            tr_state.D0,
            tr_state.phi,
            tr_state.omega,
            tr_state.Z0,
            tr_state.tanLambda,
            cov.data(),
            refP.data());

          lcio_tr->addTrackState(lcio_tr_state);
        }

        // Save intermediate tracks ref
        k4EDM4hep2LcioConv::detail::mapInsert(lcio_tr, edm_tr, tracks_vec);

        // Add to lcio tracks collection
        tracks->addElement(lcio_tr);
      }
    }

    // Link associated tracks after converting all tracks
    for (auto& [lcio_tr, edm_tr] : tracks_vec) {
      for (const auto& edm_linked_tr : edm_tr.getTracks()) {
        if (edm_linked_tr.isAvailable()) {
          // Search the linked track in the converted vector
          if (const auto lcio_tr_linked = k4EDM4hep2LcioConv::detail::mapLookupFrom(edm_linked_tr, tracks_vec))
            lcio_tr->addTrack(lcio_tr_linked.value());
        }
      }
    }

    return tracks;
  }

  // Convert EDM4hep TrackerHits to LCIO
  // Add converted LCIO ptr and original EDM4hep collection to vector of pairs
  // Add LCIO Collection Vector to LCIO event
  template<typename TrackerHitMapT>
  lcio::LCCollectionVec* convTrackerHits(
    const edm4hep::TrackerHitCollection* const trackerhits_coll,
    const std::string& cellIDstr,
    TrackerHitMapT& trackerhits_vec)
  {
    auto* trackerhits = new lcio::LCCollectionVec(lcio::LCIO::TRACKERHIT);

    if (cellIDstr != "") {
      lcio::CellIDEncoder<lcio::SimCalorimeterHitImpl> idEnc(cellIDstr, trackerhits);
    }

    // Loop over EDM4hep trackerhits converting them to lcio trackerhits
    for (const auto& edm_trh : (*trackerhits_coll)) {
      if (edm_trh.isAvailable()) {
        auto* lcio_trh = new lcio::TrackerHitImpl();

        uint64_t combined_value = edm_trh.getCellID();
        uint32_t* combined_value_ptr = reinterpret_cast<uint32_t*>(&combined_value);
        lcio_trh->setCellID0(combined_value_ptr[0]);
        lcio_trh->setCellID1(combined_value_ptr[1]);
        lcio_trh->setType(edm_trh.getType());
        std::array<double, 3> positions {edm_trh.getPosition()[0], edm_trh.getPosition()[1], edm_trh.getPosition()[2]};
        lcio_trh->setPosition(positions.data());
        lcio_trh->setCovMatrix(edm_trh.getCovMatrix().data());
        lcio_trh->setEDep(edm_trh.getEDep());
        lcio_trh->setEDepError(edm_trh.getEDepError());
        lcio_trh->setTime(edm_trh.getTime());
        lcio_trh->setQuality(edm_trh.getQuality());
        std::bitset<sizeof(uint32_t)> type_bits = edm_trh.getQuality();
        for (int j = 0; j < sizeof(uint32_t); j++) {
          lcio_trh->setQualityBit(j, (type_bits[j] == 0) ? 0 : 1);
        }

        // Save intermediate trackerhits ref
        k4EDM4hep2LcioConv::detail::mapInsert(lcio_trh, edm_trh, trackerhits_vec);

        // Add to lcio trackerhits collection
        trackerhits->addElement(lcio_trh);
      }
    }

    return trackerhits;
  }

  // Convert EDM4hep SimTrackerHits to LCIO
  // Add converted LCIO ptr and original EDM4hep collection to vector of pairs
  // Add LCIO Collection Vector to LCIO event
  template<typename SimTrHitMapT, typename MCParticleMapT>
  lcio::LCCollectionVec* convSimTrackerHits(
    const edm4hep::SimTrackerHitCollection* const simtrackerhits_coll,
    const std::string& cellIDstr,
    SimTrHitMapT& simtrackerhits_vec,
    const MCParticleMapT& mcparticles_vec)
  {
    auto* simtrackerhits = new lcio::LCCollectionVec(lcio::LCIO::SIMTRACKERHIT);

    if (cellIDstr != "") {
      lcio::CellIDEncoder<lcio::SimTrackerHitImpl> idEnc(cellIDstr, simtrackerhits);
    }

    // Loop over EDM4hep simtrackerhits converting them to LCIO simtrackerhits
    for (const auto& edm_strh : (*simtrackerhits_coll)) {
      if (edm_strh.isAvailable()) {
        auto* lcio_strh = new lcio::SimTrackerHitImpl();

        uint64_t combined_value = edm_strh.getCellID();
        uint32_t* combined_value_ptr = reinterpret_cast<uint32_t*>(&combined_value);
        lcio_strh->setCellID0(combined_value_ptr[0]);
        lcio_strh->setCellID1(combined_value_ptr[1]);
        std::array<double, 3> positions {
          edm_strh.getPosition()[0], edm_strh.getPosition()[1], edm_strh.getPosition()[2]};
        lcio_strh->setPosition(positions.data());
        lcio_strh->setEDep(edm_strh.getEDep());
        lcio_strh->setTime(edm_strh.getTime());
        lcio_strh->setMomentum(edm_strh.getMomentum()[0], edm_strh.getMomentum()[1], edm_strh.getMomentum()[2]);
        lcio_strh->setPathLength(edm_strh.getPathLength());
        lcio_strh->setQuality(edm_strh.getQuality());
        // lcio_strh->setQualityBit( int bit , bool val=true ) ;
        lcio_strh->setOverlay(edm_strh.isOverlay());
        lcio_strh->setProducedBySecondary(edm_strh.isProducedBySecondary());

        // Link converted MCParticle to the SimTrackerHit if found
        const auto edm_strh_mcp = edm_strh.getMCParticle();
        if (edm_strh_mcp.isAvailable()) {
          if (const auto& lcio_mcp = k4EDM4hep2LcioConv::detail::mapLookupFrom(edm_strh_mcp, mcparticles_vec)) {
            lcio_strh->setMCParticle(lcio_mcp.value());
          }
          else {
            // If MCParticle available, but not found in converted vec, add nullptr
            lcio_strh->setMCParticle(nullptr);
          }
        }

        // Save intermediate simtrackerhits ref
        k4EDM4hep2LcioConv::detail::mapInsert(lcio_strh, edm_strh, simtrackerhits_vec);

        // Add to lcio simtrackerhits collection
        simtrackerhits->addElement(lcio_strh);
      }
    }

    return simtrackerhits;
  }

  // Convert EDM4hep Calorimeter Hits to LCIO
  // Add converted LCIO ptr and original EDM4hep collection to vector of pairs
  // Add converted LCIO Collection Vector to LCIO event
  template<typename CaloHitMapT>
  lcio::LCCollectionVec* convCalorimeterHits(
    const edm4hep::CalorimeterHitCollection* const calohit_coll,
    const std::string& cellIDstr,
    CaloHitMapT& calo_hits_vec)
  {
    auto* calohits = new lcio::LCCollectionVec(lcio::LCIO::CALORIMETERHIT);

    if (cellIDstr != "") {
      lcio::CellIDEncoder<lcio::SimCalorimeterHitImpl> idEnc(cellIDstr, calohits);
    }

    for (const auto& edm_calohit : (*calohit_coll)) {
      if (edm_calohit.isAvailable()) {
        auto* lcio_calohit = new lcio::CalorimeterHitImpl();

        uint64_t combined_value = edm_calohit.getCellID();
        uint32_t* combined_value_ptr = reinterpret_cast<uint32_t*>(&combined_value);
        lcio_calohit->setCellID0(combined_value_ptr[0]);
        lcio_calohit->setCellID1(combined_value_ptr[1]);
        lcio_calohit->setEnergy(edm_calohit.getEnergy());
        lcio_calohit->setEnergyError(edm_calohit.getEnergyError());
        lcio_calohit->setTime(edm_calohit.getTime());
        std::array<float, 3> positions {
          edm_calohit.getPosition()[0], edm_calohit.getPosition()[1], edm_calohit.getPosition()[2]};
        lcio_calohit->setPosition(positions.data());
        lcio_calohit->setType(edm_calohit.getType());

        // TODO
        // lcio_calohit->setRawHit(EVENT::LCObject* rawHit );

        // Save Calorimeter Hits LCIO and EDM4hep collections
        k4EDM4hep2LcioConv::detail::mapInsert(lcio_calohit, edm_calohit, calo_hits_vec);

        // Add to lcio tracks collection
        calohits->addElement(lcio_calohit);
      }
    }

    return calohits;
  }

  // Convert EDM4hep RAW Calorimeter Hits to LCIO
  // Add converted LCIO ptr and original EDM4hep collection to vector of pairs
  // Add converted LCIO Collection Vector to LCIO event
  template<typename RawCaloHitMapT>
  lcio::LCCollectionVec* convRawCalorimeterHits(
    const edm4hep::RawCalorimeterHitCollection* const rawcalohit_coll,
    RawCaloHitMapT& raw_calo_hits_vec)
  {
    auto* rawcalohits = new lcio::LCCollectionVec(lcio::LCIO::RAWCALORIMETERHIT);

    for (const auto& edm_raw_calohit : (*rawcalohit_coll)) {
      if (edm_raw_calohit.isAvailable()) {
        auto* lcio_rawcalohit = new lcio::RawCalorimeterHitImpl();

        uint64_t combined_value = edm_raw_calohit.getCellID();
        uint32_t* combined_value_ptr = reinterpret_cast<uint32_t*>(&combined_value);
        lcio_rawcalohit->setCellID0(combined_value_ptr[0]);
        lcio_rawcalohit->setCellID1(combined_value_ptr[1]);
        lcio_rawcalohit->setAmplitude(edm_raw_calohit.getAmplitude());
        lcio_rawcalohit->setTimeStamp(edm_raw_calohit.getTimeStamp());

        // Save Raw Calorimeter Hits LCIO and EDM4hep collections
        k4EDM4hep2LcioConv::detail::mapInsert(lcio_rawcalohit, edm_raw_calohit, raw_calo_hits_vec);

        // Add to lcio tracks collection
        rawcalohits->addElement(lcio_rawcalohit);
      }
    }

    return rawcalohits;
  }

  // Convert EDM4hep Sim Calorimeter Hits to LCIO
  // Add converted LCIO ptr and original EDM4hep collection to vector of pairs
  // Add converted LCIO Collection Vector to LCIO event
  template<typename SimCaloHitMapT, typename MCParticleMapT>
  lcio::LCCollectionVec* convSimCalorimeterHits(
    const edm4hep::SimCalorimeterHitCollection* const simcalohit_coll,
    const std::string& cellIDstr,
    SimCaloHitMapT& sim_calo_hits_vec,
    const MCParticleMapT& mcparticles)
  {
    auto* simcalohits = new lcio::LCCollectionVec(lcio::LCIO::SIMCALORIMETERHIT);

    if (cellIDstr != "") {
      lcio::CellIDEncoder<lcio::SimCalorimeterHitImpl> idEnc(cellIDstr, simcalohits);
    }

    for (const auto& edm_sim_calohit : (*simcalohit_coll)) {
      if (edm_sim_calohit.isAvailable()) {
        auto* lcio_simcalohit = new lcio::SimCalorimeterHitImpl();

        uint64_t combined_value = edm_sim_calohit.getCellID();
        uint32_t* combined_value_ptr = reinterpret_cast<uint32_t*>(&combined_value);
        lcio_simcalohit->setCellID0(combined_value_ptr[0]);
        lcio_simcalohit->setCellID1(combined_value_ptr[1]);
        lcio_simcalohit->setEnergy(edm_sim_calohit.getEnergy());
        std::array<float, 3> positions {
          edm_sim_calohit.getPosition()[0], edm_sim_calohit.getPosition()[1], edm_sim_calohit.getPosition()[2]};
        lcio_simcalohit->setPosition(positions.data());

        // Contributions are converted in FillMissingCollections to make it a higher probability that we have the
        // MCParticles converted

        // Save Sim Calorimeter Hits LCIO and EDM4hep collections
        k4EDM4hep2LcioConv::detail::mapInsert(lcio_simcalohit, edm_sim_calohit, sim_calo_hits_vec);

        // Add to sim calo hits collection
        simcalohits->addElement(lcio_simcalohit);
      }
    }

    return simcalohits;
  }

  // Convert EDM4hep TPC Hits to LCIO
  // Add converted LCIO ptr and original EDM4hep collection to vector of pairs
  // Add converted LCIO Collection Vector to LCIO event
  template<typename TPCHitMapT>
  lcio::LCCollectionVec* convTPCHits(
    const edm4hep::RawTimeSeriesCollection* const tpchit_coll,
    TPCHitMapT& tpc_hits_vec)
  {
    auto* tpchits = new lcio::LCCollectionVec(lcio::LCIO::TPCHIT);

    for (const auto& edm_tpchit : (*tpchit_coll)) {
      if (edm_tpchit.isAvailable()) {
        auto* lcio_tpchit = new lcio::TPCHitImpl();

#warning "unsigned long long conversion to int"
        lcio_tpchit->setCellID(edm_tpchit.getCellID());
        lcio_tpchit->setTime(edm_tpchit.getTime());
        lcio_tpchit->setCharge(edm_tpchit.getCharge());
        lcio_tpchit->setQuality(edm_tpchit.getQuality());

        std::vector<int> rawdata;
#if EDM4HEP_BUILD_VERSION > EDM4HEP_VERSION(0, 7, 2)
        for (int i = 0; i < edm_tpchit.adcCounts_size(); ++i) {
          rawdata.push_back(edm_tpchit.getAdcCounts(i));
        }

        lcio_tpchit->setRawData(rawdata.data(), edm_tpchit.adcCounts_size());
#else
        for (int i = 0; i < edm_tpchit.rawDataWords_size(); ++i) {
          rawdata.push_back(edm_tpchit.getRawDataWords(i));
        }

        lcio_tpchit->setRawData(rawdata.data(), edm_tpchit.rawDataWords_size());
#endif

        // Save TPC Hits LCIO and EDM4hep collections
        k4EDM4hep2LcioConv::detail::mapInsert(lcio_tpchit, edm_tpchit, tpc_hits_vec);

        // Add to lcio tracks collection
        tpchits->addElement(lcio_tpchit);
      }
    }

    return tpchits;
  }

  // Convert EDM4hep Clusters to LCIO
  // Add converted LCIO ptr and original EDM4hep collection to vector of pairs
  // Add converted LCIO Collection Vector to LCIO event
  template<typename ClusterMapT, typename CaloHitMapT>
  lcio::LCCollectionVec* convClusters(
    const edm4hep::ClusterCollection* const cluster_coll,
    ClusterMapT& cluster_vec,
    const CaloHitMapT& calohits_vec)
  {
    auto* clusters = new lcio::LCCollectionVec(lcio::LCIO::CLUSTER);

    // Loop over EDM4hep clusters converting them to lcio clusters
    for (const auto& edm_cluster : (*cluster_coll)) {
      if (edm_cluster.isAvailable()) {
        auto* lcio_cluster = new lcio::ClusterImpl();

        std::bitset<sizeof(uint32_t)> type_bits = edm_cluster.getType();
        for (int j = 0; j < sizeof(uint32_t); j++) {
          lcio_cluster->setTypeBit(j, (type_bits[j] == 0) ? false : true);
        }
        lcio_cluster->setEnergy(edm_cluster.getEnergy());
        lcio_cluster->setEnergyError(edm_cluster.getEnergyError());

        std::array<float, 3> edm_cluster_pos = {
          edm_cluster.getPosition().x, edm_cluster.getPosition().y, edm_cluster.getPosition().z};
        lcio_cluster->setPosition(edm_cluster_pos.data());

        lcio_cluster->setPositionError(edm_cluster.getPositionError().data());
        lcio_cluster->setITheta(edm_cluster.getITheta());
        lcio_cluster->setIPhi(edm_cluster.getPhi());
        std::array<float, 3> edm_cluster_dir_err = {
          edm_cluster.getPosition().x, edm_cluster.getPosition().y, edm_cluster.getPosition().z};
        lcio_cluster->setDirectionError(edm_cluster_dir_err.data());

        EVENT::FloatVec shape_vec {};
        for (auto& param : edm_cluster.getShapeParameters()) {
          shape_vec.push_back(param);
        }
        lcio_cluster->setShape(shape_vec);

        // Convert ParticleIDs associated to the recoparticle
        for (const auto& edm_pid : edm_cluster.getParticleIDs()) {
          if (edm_pid.isAvailable()) {
            auto* lcio_pid = new lcio::ParticleIDImpl;

            lcio_pid->setType(edm_pid.getType());
            lcio_pid->setPDG(edm_pid.getPDG());
            lcio_pid->setLikelihood(edm_pid.getLikelihood());
            lcio_pid->setAlgorithmType(edm_pid.getAlgorithmType());
            for (const auto& param : edm_pid.getParameters()) {
              lcio_pid->addParameter(param);
            }

            lcio_cluster->addParticleID(lcio_pid);
          }
        }

        // Add LCIO and EDM4hep pair collections to vec
        k4EDM4hep2LcioConv::detail::mapInsert(lcio_cluster, edm_cluster, cluster_vec);

        // Add to lcio tracks collection
        clusters->addElement(lcio_cluster);
      }
    }

    // Link associated clusters after converting all clusters
    for (auto& [lcio_cluster, edm_cluster] : cluster_vec) {
      for (const auto& edm_linked_cluster : edm_cluster.getClusters()) {
        if (edm_linked_cluster.isAvailable()) {
          if (
            const auto lcio_cluster_linked =
              k4EDM4hep2LcioConv::detail::mapLookupFrom(edm_linked_cluster, cluster_vec)) {
            lcio_cluster->addCluster(lcio_cluster_linked.value());
          }
        }
      }
    }

    return clusters;
  }

  // Convert EDM4hep Vertices to LCIO
  // Add converted LCIO ptr and original EDM4hep collection to vector of pairs
  // Add converted LCIO Collection Vector to LCIO event
  template<typename VertexMapT, typename RecoPartMapT>
  lcio::LCCollectionVec* convVertices(
    const edm4hep::VertexCollection* const vertex_coll,
    VertexMapT& vertex_vec,
    const RecoPartMapT& recoparticles_vec)
  {
    auto* vertices = new lcio::LCCollectionVec(lcio::LCIO::VERTEX);

    // Loop over EDM4hep vertex converting them to lcio vertex
    for (const auto& edm_vertex : (*vertex_coll)) {
      if (edm_vertex.isAvailable()) {
        auto* lcio_vertex = new lcio::VertexImpl();
        lcio_vertex->setPrimary(edm_vertex.getPrimary());
        lcio_vertex->setAlgorithmType(std::to_string(edm_vertex.getAlgorithmType()));
        lcio_vertex->setChi2(edm_vertex.getChi2());
        lcio_vertex->setProbability(edm_vertex.getProbability());
        lcio_vertex->setPosition(edm_vertex.getPosition()[0], edm_vertex.getPosition()[1], edm_vertex.getPosition()[2]);
        lcio_vertex->setCovMatrix(edm_vertex.getCovMatrix().data());

        for (auto& param : edm_vertex.getParameters()) {
          lcio_vertex->addParameter(param);
        }

        // Link sinlge associated Particle if found in converted ones
        edm4hep::ReconstructedParticle vertex_rp = edm_vertex.getAssociatedParticle();
        if (vertex_rp.isAvailable()) {
          if (const auto lcio_rp = k4EDM4hep2LcioConv::detail::mapLookupFrom(vertex_rp, recoparticles_vec)) {
            lcio_vertex->setAssociatedParticle(lcio_rp.value());
          }
          else {
            lcio_vertex->setAssociatedParticle(nullptr);
          }
        }

        // Add LCIO and EDM4hep pair collections to vec
        k4EDM4hep2LcioConv::detail::mapInsert(lcio_vertex, edm_vertex, vertex_vec);

        // Add to lcio tracks collection
        vertices->addElement(lcio_vertex);
      }
    }

    return vertices;
  }

  // Convert EDM4hep RecoParticles to LCIO
  // Add converted LCIO ptr and original EDM4hep collection to vector of pairs
  // Add converted LCIO Collection Vector to LCIO event
  template<typename RecoPartMapT, typename TrackMapT, typename VertexMapT, typename ClusterMapT>
  lcio::LCCollectionVec* convReconstructedParticles(
    const edm4hep::ReconstructedParticleCollection* const recos_coll,
    RecoPartMapT& recoparticles_vec,
    const TrackMapT& tracks_vec,
    const VertexMapT& vertex_vec,
    const ClusterMapT& clusters_vec)
  {
    auto* recops = new lcio::LCCollectionVec(lcio::LCIO::RECONSTRUCTEDPARTICLE);

    for (const auto& edm_rp : (*recos_coll)) {
      auto* lcio_recp = new lcio::ReconstructedParticleImpl;
      if (edm_rp.isAvailable()) {
        lcio_recp->setType(edm_rp.getType());
        float m[3] = {edm_rp.getMomentum()[0], edm_rp.getMomentum()[1], edm_rp.getMomentum()[2]};
        lcio_recp->setMomentum(m);
        lcio_recp->setEnergy(edm_rp.getEnergy());
        lcio_recp->setCovMatrix(edm_rp.getCovMatrix().data()); // TODO Check lower or upper
        lcio_recp->setMass(edm_rp.getMass());
        lcio_recp->setCharge(edm_rp.getCharge());
        float rp[3] = {edm_rp.getReferencePoint()[0], edm_rp.getReferencePoint()[1], edm_rp.getReferencePoint()[2]};
        lcio_recp->setReferencePoint(rp);
        lcio_recp->setGoodnessOfPID(edm_rp.getGoodnessOfPID());

        // Convert ParticleIDs associated to the recoparticle
        for (const auto& edm_pid : edm_rp.getParticleIDs()) {
          if (edm_pid.isAvailable()) {
            auto* lcio_pid = new lcio::ParticleIDImpl;

            lcio_pid->setType(edm_pid.getType());
            lcio_pid->setPDG(edm_pid.getPDG());
            lcio_pid->setLikelihood(edm_pid.getLikelihood());
            lcio_pid->setAlgorithmType(edm_pid.getAlgorithmType());
            for (const auto& param : edm_pid.getParameters()) {
              lcio_pid->addParameter(param);
            }

            lcio_recp->addParticleID(lcio_pid);
          }
        }

        // Link sinlge associated Particle
        auto edm_pid_used = edm_rp.getParticleIDUsed();
        if (edm_pid_used.isAvailable()) {
          for (const auto& lcio_pid : lcio_recp->getParticleIDs()) {
            bool is_same = true;
            is_same = is_same && (lcio_pid->getType() == edm_pid_used.getType());
            is_same = is_same && (lcio_pid->getPDG() == edm_pid_used.getPDG());
            is_same = is_same && (lcio_pid->getLikelihood() == edm_pid_used.getLikelihood());
            is_same = is_same && (lcio_pid->getAlgorithmType() == edm_pid_used.getAlgorithmType());
            for (int i = 0; i < edm_pid_used.parameters_size(); ++i) {
              is_same = is_same && (edm_pid_used.getParameters(i) == lcio_pid->getParameters()[i]);
            }
            if (is_same) {
              lcio_recp->setParticleIDUsed(lcio_pid);
              break;
            }
          }
        }

        // Link sinlge associated Vertex if found in converted ones
        auto vertex = edm_rp.getStartVertex();
        if (vertex.isAvailable()) {
          if (const auto lcio_vertex = k4EDM4hep2LcioConv::detail::mapLookupFrom(vertex, vertex_vec)) {
            lcio_recp->setStartVertex(lcio_vertex.value());
          }
          else {
            lcio_recp->setStartVertex(nullptr);
          }
        }

        // Link multiple associated Tracks if found in converted ones
        for (const auto& edm_rp_tr : edm_rp.getTracks()) {
          if (edm_rp_tr.isAvailable()) {
            if (const auto lcio_tr = k4EDM4hep2LcioConv::detail::mapLookupFrom(edm_rp_tr, tracks_vec)) {
              lcio_recp->addTrack(lcio_tr.value());
            }
            else {
              lcio_recp->addTrack(nullptr);
            }
          }
        }

        // Link multiple associated Clusters if found in converted ones
        for (const auto& edm_rp_cluster : edm_rp.getClusters()) {
          if (edm_rp_cluster.isAvailable()) {
            if (const auto lcio_cluster = k4EDM4hep2LcioConv::detail::mapLookupFrom(edm_rp_cluster, clusters_vec)) {
              lcio_recp->addCluster(lcio_cluster.value());
            }
            else {
              lcio_recp->addCluster(nullptr);
            }
          }
        }

        // Add LCIO and EDM4hep pair collections to vec
        k4EDM4hep2LcioConv::detail::mapInsert(lcio_recp, edm_rp, recoparticles_vec);

        // Add to reconstructed particles collection
        recops->addElement(lcio_recp);
      }
    }

    // Link associated recopartilces after converting all recoparticles
    for (auto& [lcio_rp, edm_rp] : recoparticles_vec) {
      for (const auto& edm_linked_rp : edm_rp.getParticles()) {
        if (edm_linked_rp.isAvailable()) {
          // Search the linked track in the converted vector
          if (const auto lcio_rp_linked = k4EDM4hep2LcioConv::detail::mapLookupFrom(edm_linked_rp, recoparticles_vec)) {
            lcio_rp->addParticle(lcio_rp_linked.value());
          }
        }
      }
    }

    return recops;
  }

  // Convert MC Particles to LCIO
  // Add converted LCIO ptr and original EDM4hep collection to vector of pairs
  // Add converted LCIO Collection Vector to LCIO event
  template<typename MCPartMapT>
  lcio::LCCollectionVec* convMCParticles(
    const edm4hep::MCParticleCollection* const mcparticle_coll,
    MCPartMapT& mc_particles_vec)
  {
    auto* mcparticles = new lcio::LCCollectionVec(lcio::LCIO::MCPARTICLE);

    for (const auto& edm_mcp : (*mcparticle_coll)) {
      auto* lcio_mcp = new lcio::MCParticleImpl;
      if (edm_mcp.isAvailable()) {
        lcio_mcp->setPDG(edm_mcp.getPDG());
        lcio_mcp->setGeneratorStatus(edm_mcp.getGeneratorStatus());
        // Note LCIO sets some Bits during writing which makes a trivial integer conversion afterwards not work
        int status = edm_mcp.getSimulatorStatus();
        lcio_mcp->setSimulatorStatus(status);

        double vertex[3] = {edm_mcp.getVertex()[0], edm_mcp.getVertex()[1], edm_mcp.getVertex()[2]};
        lcio_mcp->setVertex(vertex);
        lcio_mcp->setTime(edm_mcp.getTime());
        double endpoint[3] = {edm_mcp.getEndpoint()[0], edm_mcp.getEndpoint()[1], edm_mcp.getEndpoint()[2]};
        lcio_mcp->setEndpoint(endpoint);
        double momentum[3] = {edm_mcp.getMomentum()[0], edm_mcp.getMomentum()[1], edm_mcp.getMomentum()[2]};
        lcio_mcp->setMomentum(momentum);
        float momentumEndpoint[3] = {
          edm_mcp.getMomentumAtEndpoint()[0], edm_mcp.getMomentumAtEndpoint()[1], edm_mcp.getMomentumAtEndpoint()[2]};
        lcio_mcp->setMomentumAtEndpoint(momentumEndpoint);
#warning "double to float"
        lcio_mcp->setMass(edm_mcp.getMass());
        lcio_mcp->setCharge(edm_mcp.getCharge());
        float spin[3] = {edm_mcp.getSpin()[0], edm_mcp.getSpin()[1], edm_mcp.getSpin()[2]};
        lcio_mcp->setSpin(spin);
        int colorflow[2] = {edm_mcp.getColorFlow()[0], edm_mcp.getColorFlow()[1]};
        lcio_mcp->setColorFlow(colorflow);

        lcio_mcp->setCreatedInSimulation(edm_mcp.isCreatedInSimulation());
        lcio_mcp->setBackscatter(edm_mcp.isBackscatter());
        lcio_mcp->setVertexIsNotEndpointOfParent(edm_mcp.vertexIsNotEndpointOfParent());
        lcio_mcp->setDecayedInTracker(edm_mcp.isDecayedInTracker());
        lcio_mcp->setDecayedInCalorimeter(edm_mcp.isDecayedInCalorimeter());
        lcio_mcp->setHasLeftDetector(edm_mcp.hasLeftDetector());
        lcio_mcp->setStopped(edm_mcp.isStopped());
        lcio_mcp->setOverlay(edm_mcp.isOverlay());

        // Add LCIO and EDM4hep pair collections to vec
        k4EDM4hep2LcioConv::detail::mapInsert(lcio_mcp, edm_mcp, mc_particles_vec);

        // Add to reconstructed particles collection
        mcparticles->addElement(lcio_mcp);
      }
    }

    // Add parent MCParticles after converting all MCparticles
    for (auto& [lcio_mcp, edm_mcp] : mc_particles_vec) {
      for (const auto& emd_parent_mcp : edm_mcp.getParents()) {
        if (emd_parent_mcp.isAvailable()) {
          // Search for the parent mcparticle in the converted vector
          if (
            const auto lcio_mcp_linked = k4EDM4hep2LcioConv::detail::mapLookupFrom(emd_parent_mcp, mc_particles_vec)) {
            lcio_mcp->addParent(lcio_mcp_linked.value());
          }
        }
      }
    }

    return mcparticles;
  }

  template<typename ObjectMappingT>
  void FillMissingCollections(ObjectMappingT& collection_pairs)
  {
    FillMissingCollections(collection_pairs, collection_pairs);
  }

  // Depending on the order of the collections in the parameters,
  // and for the mutual dependencies between some collections,
  // go over the possible missing associated collections and fill them.
  template<typename ObjectMappingT, typename ObjectMappingU>
  void FillMissingCollections(ObjectMappingT& update_pairs, const ObjectMappingU& lookup_pairs)
  {
    // Fill missing Tracks collections
    for (auto& [lcio_tr, edm_tr] : update_pairs.tracks) {
      if (lcio_tr->getTrackerHits().size() == 0) {
        for (const auto& edm_tr_trh : edm_tr.getTrackerHits()) {
          if (const auto lcio_trh = k4EDM4hep2LcioConv::detail::mapLookupFrom(edm_tr_trh, lookup_pairs.trackerHits)) {
            lcio_tr->addHit(lcio_trh.value());
          }
        }
      }
    }

    // Fill missing ReconstructedParticle collections
    for (auto& [lcio_rp, edm_rp] : update_pairs.recoParticles) {
      // Link Vertex
      if (lcio_rp->getStartVertex() == nullptr) {
        if (edm_rp.getStartVertex().isAvailable()) {
          if (
            const auto lcio_vertex =
              k4EDM4hep2LcioConv::detail::mapLookupFrom(edm_rp.getStartVertex(), lookup_pairs.vertices)) {
            lcio_rp->setStartVertex(lcio_vertex.value());
          }
        }
      }

      // Link Tracks
      if (lcio_rp->getTracks().size() != edm_rp.tracks_size()) {
        assert(lcio_rp->getTracks().size() == 0);
        for (const auto& edm_rp_tr : edm_rp.getTracks()) {
          if (const auto lcio_tr = k4EDM4hep2LcioConv::detail::mapLookupFrom(edm_rp_tr, lookup_pairs.tracks)) {
            lcio_rp->addTrack(lcio_tr.value());
          }
        }
      }

      // Link Clusters
      if (lcio_rp->getClusters().size() != edm_rp.clusters_size()) {
        assert(lcio_rp->getClusters().size() == 0);
        for (const auto& edm_rp_cluster : edm_rp.getClusters()) {
          if (
            const auto lcio_cluster =
              k4EDM4hep2LcioConv::detail::mapLookupFrom(edm_rp_cluster, lookup_pairs.clusters)) {
            lcio_rp->addCluster(lcio_cluster.value());
          }
        }
      }

    } // reconstructed particles

    // Fill missing Vertices collections
    for (auto& [lcio_vertex, edm_vertex] : update_pairs.vertices) {
      // Link Reconstructed Particles
      if (lcio_vertex->getAssociatedParticle() == nullptr) {
        const auto edm_rp = edm_vertex.getAssociatedParticle();
        if (edm_rp.isAvailable()) {
          if (const auto lcio_rp = k4EDM4hep2LcioConv::detail::mapLookupFrom(edm_rp, lookup_pairs.recoParticles)) {
            lcio_vertex->setAssociatedParticle(lcio_rp.value());
          }
        }
      }

    } // vertices

    // Fill SimCaloHit collections with contributions
    //
    // We loop over all pairs of lcio and edm4hep simcalo hits and add the contributions, by now MCParticle
    // collection(s) should be converted!
    for (auto& [lcio_sch, edm_sch] : update_pairs.simCaloHits) {
      // add associated Contributions (MCParticles)
      for (int i = 0; i < edm_sch.contributions_size(); ++i) {
        const auto& contrib = edm_sch.getContributions(i);
        if (not contrib.isAvailable()) {
          // We need a logging library independent of Gaudi for this!
          // std::cout << "WARNING: CaloHit Contribution is not available!" << std::endl;
          continue;
        }
        auto edm_contrib_mcp = contrib.getParticle();
        std::array<float, 3> step_position {
          contrib.getStepPosition()[0], contrib.getStepPosition()[1], contrib.getStepPosition()[2]};
        bool mcp_found = false;
        EVENT::MCParticle* lcio_mcp = nullptr;
        if (edm_contrib_mcp.isAvailable()) {
          // if we have the MCParticle we look for its partner
          lcio_mcp =
            k4EDM4hep2LcioConv::detail::mapLookupFrom(edm_contrib_mcp, lookup_pairs.mcParticles).value_or(nullptr);
        }
        else { // edm mcp available
               // std::cout << "WARNING: edm4hep contribution is not available!"  << std::endl;
        }
        // we add contribution with whatever lcio mc particle we found
        lcio_sch->addMCParticleContribution(
          lcio_mcp, contrib.getEnergy(), contrib.getTime(), contrib.getPDG(), step_position.data());
        // if (!lcio_mcp) {
        //   std::cout << "WARNING: No MCParticle found for this contribution."
        //             << "Make Sure MCParticles are converted! "
        //             << edm_contrib_mcp.id()
        //             << std::endl;
        // }
      } // all emd4hep contributions
      // We need to reset the energy to the original one, because adding
      // contributions alters the energy in LCIO
      lcio_sch->setEnergy(edm_sch.getEnergy());
    } // SimCaloHit

    // Fill missing SimTrackerHit collections
    for (auto& [lcio_strh, edm_strh] : update_pairs.simTrackerHits) {
      const auto lcio_strh_mcp = lcio_strh->getMCParticle();
      if (lcio_strh_mcp == nullptr) {
        const auto edm_strh_mcp = edm_strh.getMCParticle();
        if (const auto lcio_mcp = k4EDM4hep2LcioConv::detail::mapLookupFrom(edm_strh_mcp, lookup_pairs.mcParticles)) {
          lcio_strh->setMCParticle(lcio_mcp.value());
        }
      }

    } // SimTrackerHits
  }

} // namespace EDM4hep2LCIOConv
