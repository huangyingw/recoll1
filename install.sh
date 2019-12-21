#!/bin/bash -
SCRIPT=$(realpath "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
cd "$SCRIPTPATH"

cp -fv ./packaging/homebrew/recoll.rb /usr/local/Homebrew/Library/Taps/homebrew/homebrew-core/Formula/recoll.rb
brew install recoll
