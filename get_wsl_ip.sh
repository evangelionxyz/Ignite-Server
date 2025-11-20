#!/bin/bash

echo "=== Network Information ==="
echo ""
echo "WSL IP Address:"
hostname -I | awk '{print $1}'
echo ""
echo "Windows Host IP (from WSL):"
ip route show | grep -i default | awk '{ print $3}'
echo ""
echo "=== Connection Instructions ==="
echo ""
echo "From WSL:"
echo "  ./build/TestClient/TestClient"
echo ""
echo "From Windows (same PC):"
WSL_IP=$(hostname -I | awk '{print $1}')
echo "  TestClient.exe ${WSL_IP}:8192"
echo ""
