#!/bin/bash

echo "#define UPCXX_NETWORK_$(tr '[a-z]' '[A-Z]' <<<$UPCXX_NETWORK) 1"
