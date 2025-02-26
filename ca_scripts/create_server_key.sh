#!/usr/bin/env bash

# Copyright 2025 OIST
# Copyright 2025 fold ecosystemics
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# see https://jamielinux.com/docs/openssl-certificate-authority/sign-server-and-client-certificates.html
set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
export CA_DIR="${SCRIPT_DIR}/ca/intermediate"

echo -e "\033[0;31mTHIS IS JUST AN EXAMPLE SCRIPT, DO NOT USE IN PRODUCTION\033[0m"

# Read MAC address from ESP and format it
# Using the first MAC address occurrence in the output
read -p "Would you like to read the device MAC address to set the server name? (y/N) " response
if [[ "${response,,}" =~ ^y(es)?$ ]]; then
    if ! MAC_ADDR=$(esptool.py --port /dev/ttyACM0 read_mac | grep "MAC:" | head -n1 | cut -d' ' -f2 | tr -d ':' | tr '[:upper:]' '[:lower:]' 2>/dev/null); then
        echo "Warning: Failed to read MAC address from device" >&2
        exit 1
    else
        SRV_NAME="esp-${MAC_ADDR}.local"
    fi
else
    SRV_NAME="esp-host.local"
fi
echo "Server name set to: ${SRV_NAME}"
export SAN="DNS:${SRV_NAME}"  # used in openssl.cnf file as env variable for Subject Alt Name

echo "Creating server private key"
openssl genrsa -out "${CA_DIR}/private/${SRV_NAME}.key.pem" 2048
chmod 400 "${CA_DIR}/private/${SRV_NAME}.key.pem"

echo "Generating signing request"
# Using -subj to automatically set the Common Name without interactive prompt
openssl req -config "${CA_DIR}/openssl.cnf" \
      -key "${CA_DIR}/private/${SRV_NAME}.key.pem" \
      -new -sha256 -out "${CA_DIR}/csr/${SRV_NAME}.csr.pem" \
      -subj "/CN=${SRV_NAME}"

echo "Signing server key"
openssl ca -config "${CA_DIR}/openssl.cnf" \
      -extensions server_cert -days 36500 -notext -md sha256 \
      -in "${CA_DIR}/csr/${SRV_NAME}.csr.pem" \
      -out "${CA_DIR}/certs/${SRV_NAME}.cert.pem"
chmod 444 "${CA_DIR}/certs/${SRV_NAME}.cert.pem"

echo "Verifying server key"
openssl x509 -noout -text -in "${CA_DIR}/certs/${SRV_NAME}.cert.pem"
openssl verify -CAfile "${CA_DIR}/certs/ca-chain.cert.pem" \
      "${CA_DIR}/certs/${SRV_NAME}.cert.pem"
