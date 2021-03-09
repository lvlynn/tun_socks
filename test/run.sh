#!/bin/bash

tun_name="nt"
#tun_name="tun0"
#./test &
ifconfig $tun_name 10.0.0.1
#ip route add 14.215.177.39 dev tun0
ip route add default dev $tun_name table 100

ip rule del from 172.16.254.151
ip rule add from 172.16.254.151 table 100
ip rule del from 172.16.254.182
ip rule add from 172.16.254.182 table 100
