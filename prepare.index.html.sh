#!/bin/bash

# npm install -g inliner

cd $(dirname $0)
inliner data.in/index.html > data/index.html
