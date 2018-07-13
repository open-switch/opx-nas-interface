#!/bin/sh

# Restart all interfaces except for eth0
ifup -a -X eth0
