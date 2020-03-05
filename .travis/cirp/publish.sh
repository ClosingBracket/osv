#!/usr/bin/env bash

set -euo pipefail

. .travis/cirp/check_precondition.sh

if [ ! -z "$TRAVIS_TEST_RESULT" ] && [ "$TRAVIS_TEST_RESULT" != "0" ]; then
  echo "Build has failed, skipping publishing"
  exit 0
fi

if [ "$#" != "2" ]; then
  echo "Error: No arguments provided. Please specify a directory containing artifacts as the first argument and the release name."
  exit 1
fi

ARTIFACTS_DIR="$1"
OSV_VERSION="$2"

. .travis/cirp/install.sh

ci-release-publisher publish --latest-release \
                             --latest-release-prerelease \
                             --latest-release-check-event-type push \
                             --numbered-release \
                             --numbered-release-keep-count 10 \
                             --numbered-release-prerelease \
                             --numbered-release-name "$OSV_VERSION" \
                             "$ARTIFACTS_DIR"
