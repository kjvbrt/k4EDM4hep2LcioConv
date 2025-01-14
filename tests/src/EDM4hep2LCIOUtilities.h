#ifndef K4EDM4HEP2LCIOCONV_TEST_EDM4HEP2LCIOUTILITIES_H
#define K4EDM4HEP2LCIOCONV_TEST_EDM4HEP2LCIOUTILITIES_H

#include <cstddef>
#include <utility>
#include <vector>
#include <tuple>

namespace edm4hep {
  class CalorimeterHitCollection;
  class MCParticleCollection;
  class RawCalorimeterHitCollection;
  class RawTimeSeriesCollection;
  class TrackerHitCollection;
  class TrackCollection;
  class SimCalorimeterHitCollection;
  class CaloHitContributionCollection;
  class EventHeaderCollection;
} // namespace edm4hep

namespace podio {
  class Frame;
}

namespace test_config {
  constexpr static int nMCParticles = 5; ///< The number of MCParticles to generate

  using IdxPair = std::pair<std::size_t, std::size_t>;
  /// How to create the MC particle hierarchy, e.g. {4, 0} means that mc[4] will
  /// have mc[0] as a parent, and mc[0] will get mc[4] as a daughter
  const static std::vector<IdxPair> mcpParentIdcs = {{4, 0}, {4, 1}, {3, 2}, {3, 0}, {3, 1}, {2, 1}};

  constexpr static int nCaloHits = 2;    ///< The number of CalorimeterHits to generate
  constexpr static int nRawCaloHits = 2; ///< The number of RawCalorimeterHits to generate

  constexpr static int nTPCHits = 4;     ///< The number of TPCHits (RawTimeSeries) to generate
  constexpr static int nTPCRawWords = 6; ///< The number of raw words to put into each TPCHit

  constexpr static int nTrackerHits = 5; ///< The number of TrackerHits to generate

  constexpr static int nTracks = 4;                ///< The number of tracks to generate
  constexpr static int nSubdetectorHitNumbers = 4; ///< The number of subdetector hits for each track
  /// The tracker hits that should be added to each track
  const static std::vector<std::size_t> trackTrackerHitIdcs = {0, 2, 4};
  constexpr static int nTrackStates = 5; ///< The number of track states for each track
  /// The tracks that should be linked, first index is the track to which the
  /// second index will be added
  const static std::vector<IdxPair> trackTrackIdcs = {{0, 2}, {1, 3}, {2, 3}, {3, 2}, {3, 0}};

  constexpr static int nSimCaloHits = 3;          ///< The number of SimCalorimeterHits
  constexpr static int nCaloHitContributions = 4; ///< The number of CalorimeterHit Contributions
  /// idcs to setup relations between SimCalorimeterHits, CaloHitContributions
  /// and MCParticles (in this order)
  using CaloContIdx = std::tuple<std::size_t, std::size_t, std::size_t>;
  /// The index values to use for setting up the relations
  const static std::vector<CaloContIdx> simCaloHitMCIdcs = {
    {0, 0, 0},
    {0, 1, 2},
    {0, 2, 1},
    {0, 3, 4},
    {1, 0, 1},
    {1, 1, 3},
    {1, 2, 4},
    {1, 3, 4},
    {2, 0, 0},
    {2, 1, 3},
    {2, 2, 2},
    {2, 3, 0}};

} // namespace test_config

/**
 * Create an MCParticle collection with num_elements and create a parent
 * hierarchy using the passed mcp_parents_idx indices. The .first index will be
 * used to determine the daughter element, while the .second index will be used
 * to determine the parent.
 */
edm4hep::MCParticleCollection createMCParticles(
  const int num_elements,
  const std::vector<test_config::IdxPair>& mcp_parents_idx);

/**
 * Create a CalorimeterHit collection
 */
edm4hep::CalorimeterHitCollection createCalorimeterHits(const int num_elements);

/**
 * Create a RawCalorimeterHit collection
 */
edm4hep::RawCalorimeterHitCollection createRawCalorimeterHits(const int num_elements);

/**
 * Create a TPCHit collection
 */
edm4hep::RawTimeSeriesCollection createTPCHits(const int num_elements, const int num_rawwords);

/**
 * Create a TrackerHit collection
 */
edm4hep::TrackerHitCollection createTrackerHits(const int num_elements);

/**
 * Create a track collection with tracks that have links to other tracks (in the
 * same collection) and tracker hits
 */
edm4hep::TrackCollection createTracks(
  const int num_elements,
  const int subdetectorhitnumbers,
  const int num_track_states,
  const edm4hep::TrackerHitCollection& trackerHits,
  const std::vector<std::size_t>& link_trackerhit_idcs,
  const std::vector<test_config::IdxPair>& track_link_tracks_idcs);

/**
 * Create a SimCalorimeterHit (and an accompanying CaloHitContribution)
 * collection with links to MCParticles
 */
std::pair<edm4hep::SimCalorimeterHitCollection, edm4hep::CaloHitContributionCollection> createSimCalorimeterHits(
  const int num_elements,
  const int num_contributions,
  const edm4hep::MCParticleCollection& mcParticles,
  const std::vector<test_config::CaloContIdx>& link_mcparticles_idcs);

edm4hep::EventHeaderCollection createEventHeader();

/**
 * Create an example event that can be used to test the converter.
 *
 * Content:
 *
 * The following table gives an overview of the contents. The arguments for
 * calling the individual creation functions are taken from the test_config
 * namespace.
 * | Name                 | Data type           | comment                  |
 * |----------------------+---------------------+--------------------------|
 * | mcParticles          | MCParticle          | createMCParticles        |
 * | caloHits             | CalorimeterHit      | createCalorimeterHits    |
 * | rawCaloHits          | RawCalorimeterHit   | createRawCalorimeterHits |
 * | tpcHits              | RawTimeSeries       | createTPCHits            |
 * | trackerHits          | TrackerHit          | createTrackerHits        |
 * | simCaloHits          | SimCalorimeterHit   | createSimCalorimeterHits |
 * | caloHitContributions | CaloHitContribution | createSimCalorimeterHits |
 */
podio::Frame createExampleEvent();

#endif // K4EDM4HEP2LCIOCONV_TEST_COMPAREEDM4HEPLCIO_H
