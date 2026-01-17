#!/bin/bash

export RAYNET_HOME=$HOME/raynet

# Patch OMNETPP to allow multiple calls to doneLoadingFiles
cd $HOME/omnetpp-6.0.1/src/nedxml
patch -p0 <$RAYNET_HOME/installhelpers/nedresourcecache.cc.patch
