#!/usr/bin/env bash
set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-49913}"
URL="http://${HOST}:${PORT}/xlDoAutomation"

post() {
  local body="$1"
  echo
  echo ">>> ${body}"
  curl -sS "${URL}" \
    -H "Content-Type: application/json" \
    -H "Accept: application/json" \
    -d "${body}"
  echo
}

# Capability + discovery
post '{"apiVersion":2,"cmd":"system.getCapabilities"}'
DISCOVERY_BODY='{"apiVersion":2,"cmd":"timing.listAnalysisPlugins"}'
echo
echo ">>> ${DISCOVERY_BODY}"
DISCOVERY_RESPONSE="$(curl -sS "${URL}" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json" \
  -d "${DISCOVERY_BODY}")"
echo "${DISCOVERY_RESPONSE}"
echo

# Replace values for your show before running mutating calls.
ANALYSIS_URL="${ANALYSIS_URL:-}"
ANALYSIS_PROFILE="${ANALYSIS_PROFILE:-beats_v1}"
SOURCE_TRACK="${SOURCE_TRACK:-Beats}"
TARGET_AUDIO_TRACK="${TARGET_AUDIO_TRACK:-PR2 Audio Timing}"
TARGET_BARS_TRACK="${TARGET_BARS_TRACK:-PR2 Bars}"
TARGET_ENERGY_TRACK="${TARGET_ENERGY_TRACK:-PR2 Energy}"

# createFromAudio dry-run + apply (remote provider)
if [[ -z "${ANALYSIS_URL}" ]]; then
  echo ">>> Skipping timing.createFromAudio and timing.getTrackSummary (set ANALYSIS_URL for remote analysis)."
else
  post "{\"apiVersion\":2,\"cmd\":\"timing.createFromAudio\",\"params\":{\"analysisProvider\":\"remote\",\"analysisUrl\":\"${ANALYSIS_URL}\",\"analysisProfile\":\"${ANALYSIS_PROFILE}\",\"trackName\":\"${TARGET_AUDIO_TRACK}\",\"replaceIfExists\":true},\"options\":{\"dryRun\":true,\"requestId\":\"smoke-create-audio-dry\"}}"
  post "{\"apiVersion\":2,\"cmd\":\"timing.createFromAudio\",\"params\":{\"analysisProvider\":\"remote\",\"analysisUrl\":\"${ANALYSIS_URL}\",\"analysisProfile\":\"${ANALYSIS_PROFILE}\",\"trackName\":\"${TARGET_AUDIO_TRACK}\",\"replaceIfExists\":true},\"options\":{\"dryRun\":false,\"requestId\":\"smoke-create-audio\"}}"

  # track summary
  post "{\"apiVersion\":2,\"cmd\":\"timing.getTrackSummary\",\"params\":{\"trackName\":\"${TARGET_AUDIO_TRACK}\"},\"options\":{\"requestId\":\"smoke-summary\"}}"
fi

# bars from beats dry-run + apply
post "{\"apiVersion\":2,\"cmd\":\"timing.createBarsFromBeats\",\"params\":{\"sourceTrackName\":\"${SOURCE_TRACK}\",\"trackName\":\"${TARGET_BARS_TRACK}\",\"beatsPerBar\":4,\"replaceIfExists\":true},\"options\":{\"dryRun\":true,\"requestId\":\"smoke-bars-dry\"}}"
post "{\"apiVersion\":2,\"cmd\":\"timing.createBarsFromBeats\",\"params\":{\"sourceTrackName\":\"${SOURCE_TRACK}\",\"trackName\":\"${TARGET_BARS_TRACK}\",\"beatsPerBar\":4,\"replaceIfExists\":true},\"options\":{\"dryRun\":false,\"requestId\":\"smoke-bars\"}}"

# energy sections dry-run + apply
post "{\"apiVersion\":2,\"cmd\":\"timing.createEnergySections\",\"params\":{\"trackName\":\"${TARGET_ENERGY_TRACK}\",\"replaceIfExists\":true,\"levels\":[\"low\",\"medium\",\"high\"],\"smoothingMs\":500},\"options\":{\"dryRun\":true,\"requestId\":\"smoke-energy-dry\"}}"
post "{\"apiVersion\":2,\"cmd\":\"timing.createEnergySections\",\"params\":{\"trackName\":\"${TARGET_ENERGY_TRACK}\",\"replaceIfExists\":true,\"levels\":[\"low\",\"medium\",\"high\"],\"smoothingMs\":500},\"options\":{\"dryRun\":false,\"requestId\":\"smoke-energy\"}}"
