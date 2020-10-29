#!/bin/bash


./../bin/hev-socks5-tunnel ./../conf/main.yml

ip route add dev hev0 table 100

 ip rule add from 172.16.100.37 table 100
