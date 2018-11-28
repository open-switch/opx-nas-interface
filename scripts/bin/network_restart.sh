#!/bin/sh

# Bring down all interfaces (except system loopback) before restarting Networking service
# to ensure that all interface settings gets programmed into the NPU

/sbin/ifdown -a --exclude=lo
service networking restart
