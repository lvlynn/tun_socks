#!/bin/bash

#./test &
ifconfig tun0 10.0.0.1
#ip route add 14.215.177.39 dev tun0
ip route add default dev tun0 table 100
ip rule del from 172.16.254.157
ip rule add from 172.16.254.157 table 100
