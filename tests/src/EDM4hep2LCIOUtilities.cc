#include "EDM4hep2LCIOUtilities.h"

#include "edm4hep/CalorimeterHitCollection.h"
#include "edm4hep/MCParticleCollection.h"
#include "edm4hep/RawCalorimeterHitCollection.h"
#include "edm4hep/SimCalorimeterHitCollection.h"
#include "edm4hep/TrackerHitCollection.h"
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/RawTimeSeriesCollection.h>
#include "edm4hep/TrackCollection.h"
#include "edm4hep/SimCalorimeterHitCollection.h"
#include "edm4hep/CaloHitContributionCollection.h"

#include "podio/Frame.h"

#include <array>
#include <cstdint>
#include <cmath>

constexpr std::uint64_t operator""_u64(unsigned long long num) { return static_cast<std::uint64_t>(num); }

// Some pre-defined cellIDs that can be used below
constexpr static std::array CELLIDS = {0xcaffee_u64, 0xbeef_u64, 0xfe47_u64, 0x12345678_u64, 0_u64, -1_u64};

constexpr static uint64_t createCellID(int i) { return CELLIDS[i % CELLIDS.size()]; }

/// Create a covariance matrix for N dimensions in lower triangular form
template<size_t N, typename T = float>
constexpr auto createCov()
{
  std::array<T, N*(N + 1) / 2> result {};

  // Calculate the flat index from the 2D index
  const auto to_lower_tri = [](int i, int j) {
    if (j < i) {
      std::swap(i, j);
    }
    return i * (2 * N - i - 1) / 2 + j;
  };

  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < i; ++j) {
      const auto index = to_lower_tri(i, j);
      result[index] = i + 10 * j;
    }
  }

  return result;
}

edm4hep::MCParticleCollection createMCParticles(
  const int num_elements,
  const std::vector<test_config::IdxPair>& mcp_parents_idx)
{
  auto coll = edm4hep::MCParticleCollection {};

  for (int i = 0; i < num_elements; ++i) {
    auto elem = coll.create();
    elem.setPDG(i);
    elem.setGeneratorStatus(i + 42);
    elem.setVertex({i * 10., i * 20., i * 30.});
    elem.setTime(i * 100);
    elem.setEndpoint({i * 30., i * 20., i * 30.});
    elem.setMomentum({i * 1.f, i * 2.f, i * 3.f});
    elem.setMomentumAtEndpoint({i * 3.f, i * 2.f, i * 1.f});
    elem.setMass(125. * i);
    elem.setSpin({i * 0.5f, i * 0.25f, i * 0.25f});
    elem.setColorFlow({i, i * 2});
    elem.setCreatedInSimulation(1);
    elem.setBackscatter(0);
    elem.setVertexIsNotEndpointOfParent(1);
    elem.setDecayedInTracker(0);
    elem.setDecayedInCalorimeter(1);
    elem.setHasLeftDetector(0);
    elem.setStopped(1);
    elem.setOverlay(0);
  }

  for (const auto& [orig_idx, link_idx] : mcp_parents_idx) {
    coll[orig_idx].addToParents(coll[link_idx]);
  }
  // We assign the daughters after all the parents are assigned simply because
  // LCIO adds the daughters in the call to add parents and relation comparison
  // in the tests assume that all relations are in the same order
  for (auto particle : coll) {
    // Workaround as proposed in https://github.com/AIDASoft/podio/issues/347
    for (auto p : particle.getParents()) {
      auto parent = coll[p.getObjectID().index];
      parent.addToDaughters(particle);
    }
  }

  return coll;
}

edm4hep::CalorimeterHitCollection createCalorimeterHits(const int num_elements)
{
  edm4hep::CalorimeterHitCollection coll {};
  for (int i = 0; i < num_elements; ++i) {
    auto elem = coll.create();
    elem.setCellID(createCellID(i));
    elem.setEnergy(i);
    elem.setEnergyError(i / std::sqrt(i + 1));
    elem.setTime(i * 10);
    elem.setPosition({i * 20.f, i * 30.f, i * 40.f});
    elem.setType(i * 123);
  }

  return coll;
}

edm4hep::RawCalorimeterHitCollection createRawCalorimeterHits(const int num_elements)
{
  edm4hep::RawCalorimeterHitCollection coll {};
  for (int i = 0; i < num_elements; ++i) {
    auto elem = coll.create();
    elem.setCellID(createCellID(i));
    elem.setAmplitude(i * 1000);
    elem.setTimeStamp(i * 100);
  }

  return coll;
}

edm4hep::RawTimeSeriesCollection createTPCHits(const int num_elements, const int num_rawwords)
{
  edm4hep::RawTimeSeriesCollection coll {};
  for (int i = 0; i < num_elements; ++i) {
    auto elem = coll.create();

    elem.setCellID(createCellID(i));
    elem.setQuality(i);
    elem.setTime(i * 10.f);
    elem.setCharge(i * 0.1f);

    for (int j = 0; j < num_rawwords; ++j) {
      elem.addToAdcCounts((i + 10) * j);
    }
  }
  return coll;
}

edm4hep::TrackerHitCollection createTrackerHits(const int num_elements)
{
  edm4hep::TrackerHitCollection coll {};

  for (int i = 0; i < num_elements; ++i) {
    auto elem = coll.create();

    elem.setCellID(createCellID(i));
    elem.setType(i * 1234);
    elem.setQuality(i * 321);
    elem.setTime(i * 100.f);
    elem.setEDep(i * 1111.f);
    elem.setEDepError(i * sqrt(10.f));
    elem.setPosition({i * 10., i * 20., i * 30.});
    elem.setCovMatrix(createCov<3>());
  }
  return coll;
}

edm4hep::TrackCollection createTracks(
  const int num_elements,
  const int subdetectorhitnumbers,
  const int num_track_states,
  const edm4hep::TrackerHitCollection& trackerHits,
  const std::vector<std::size_t>& link_trackerhit_idcs,
  const std::vector<test_config::IdxPair>& track_link_tracks_idcs)
{
  // edm4hep::Track
  auto track_coll = edm4hep::TrackCollection();

  for (int i = 0; i < num_elements; ++i) {
    auto elem = track_coll.create();

    elem.setType(2); // TODO specific type
    elem.setChi2(i * 10.f);
    elem.setNdf(i * 12);
    elem.setRadiusOfInnermostHit(i * 5.f);

    elem.setDEdx(i);
    elem.setDEdxError(i / std::sqrt(i + 1));
    // Also add a DxQuantity since the comparison expects that
    elem.addToDxQuantities({0, i * 1.f, i / std::sqrt(i + 1.f)});

    for (int j = 0; j < subdetectorhitnumbers; ++j) {
      elem.addToSubdetectorHitNumbers(i + 10 * j);
    }

    for (auto& idx : link_trackerhit_idcs) {
      elem.addToTrackerHits(trackerHits[idx]);
    }

    for (int j = 0; j < num_track_states; ++j) {
      edm4hep::TrackState trackstate;

      trackstate.location = j;
      trackstate.D0 = (i + j) * 2;
      trackstate.phi = (i - j) * 2;
      trackstate.omega = (i * j) * 2;
      trackstate.Z0 = (i + j) * 0.5;
      trackstate.tanLambda = j * 2;
      // edm4hep::Vector3f test_vec {float_cnt++, float_cnt++, float_cnt++};
      trackstate.referencePoint = {j * 1.f, i * 1.f, (j + i) * 1.f};
      trackstate.covMatrix = createCov<6>();

      elem.addToTrackStates(trackstate);
    }
  }

  for (const auto& [orig_idx, link_idx] : track_link_tracks_idcs) {
    track_coll[orig_idx].addToTracks(track_coll[link_idx]);
  }

  return track_coll;
}

std::pair<edm4hep::SimCalorimeterHitCollection, edm4hep::CaloHitContributionCollection> createSimCalorimeterHits(
  const int num_elements,
  const int num_contributions,
  const edm4hep::MCParticleCollection& mcParticles,
  const std::vector<test_config::CaloContIdx>& link_mcparticles_idcs)
{
  auto simcalohit_coll = edm4hep::SimCalorimeterHitCollection();
  auto contrib_coll = edm4hep::CaloHitContributionCollection {};

  for (int i = 0; i < num_elements; ++i) {
    // auto* elem = new edm4hep::SimCalorimeterHit();
    auto elem = simcalohit_coll.create();

    elem.setCellID(createCellID(i));
    elem.setEnergy(i * 1000.);
    elem.setPosition({i * 10.f, i * 20.f, i * 30.f});

    for (int j = 0; j < num_contributions; j++) {
      auto contrib = contrib_coll.create();
      contrib.setPDG(j * 42);
      contrib.setEnergy(j + i * 1000.f);
      contrib.setTime(j * 1000 - i);
      contrib.setStepPosition({j * 1.f, j * 2.f, j * 3.f});

      // add the corresponding mcparticle
      for (const auto& [simch_idx, contrib_idx, mcpart_idx] : link_mcparticles_idcs) {
        if (i == simch_idx && j == contrib_idx) {
          contrib.setParticle(mcParticles[mcpart_idx]);
        }
      }

      elem.addToContributions(contrib);
    }
  }

  return {std::move(simcalohit_coll), std::move(contrib_coll)};
}

edm4hep::EventHeaderCollection createEventHeader()
{
  auto evtHeaderColl = edm4hep::EventHeaderCollection {};
  auto evtHeader = evtHeaderColl.create();

  evtHeader.setWeight(3.14f);
  evtHeader.setEventNumber(123456789);
  evtHeader.setRunNumber(42);
  evtHeader.setTimeStamp(0x71AAE);

  return evtHeaderColl;
}

podio::Frame createExampleEvent()
{
  podio::Frame event;

  event.put(createEventHeader(), "EventHeader");
  const auto& mcParticles =
    event.put(createMCParticles(test_config::nMCParticles, test_config::mcpParentIdcs), "mcParticles");
  const auto& caloHits = event.put(createCalorimeterHits(test_config::nCaloHits), "caloHits");
  const auto& rawCaloHits = event.put(createRawCalorimeterHits(test_config::nRawCaloHits), "rawCaloHits");
  const auto& tpcHits = event.put(createTPCHits(test_config::nTPCHits, test_config::nTPCRawWords), "tpcHits");
  const auto& trackerHits = event.put(createTrackerHits(test_config::nTrackerHits), "trackerHits");
  const auto& tracks = event.put(
    createTracks(
      test_config::nTracks,
      test_config::nSubdetectorHitNumbers,
      test_config::nTrackStates,
      trackerHits,
      test_config::trackTrackerHitIdcs,
      test_config::trackTrackIdcs),
    "tracks");

  auto [tmpSimCaloHits, tmpCaloHitConts] = createSimCalorimeterHits(
    test_config::nSimCaloHits, test_config::nCaloHitContributions, mcParticles, test_config::simCaloHitMCIdcs);
  const auto& simCaloHits = event.put(std::move(tmpSimCaloHits), "simCaloHits");
  event.put(std::move(tmpCaloHitConts), "caloHitContributions");

  return event;
}
