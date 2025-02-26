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
CA_DIR="${SCRIPT_DIR}/ca/intermediate"
CLIENT_NAME="client"

echo -e "\033[0;31mTHIS IS JUST AN EXAMPLE SCRIPT, DO NOT USE IN PRODUCTION\033[0m"

echo "Creating client private key."
openssl genrsa -out "${CA_DIR}/private/${CLIENT_NAME}.key.pem" 2048
chmod 400 "${CA_DIR}/private/${CLIENT_NAME}.key.pem"

echo "Generating signing request"
CA_DIR="${CA_DIR}" SAN="DNS:client" openssl req -config "${CA_DIR}/openssl.cnf" \
      -key "${CA_DIR}/private/${CLIENT_NAME}.key.pem" \
      -new -sha256 -out "${CA_DIR}/csr/${CLIENT_NAME}.csr.pem"


echo "Signing client key"
CA_DIR="${CA_DIR}" SAN="DNS:client" openssl ca -config "${CA_DIR}/openssl.cnf" \
      -extensions usr_cert -days 36500 -notext -md sha256 \
      -in "${CA_DIR}/csr/${CLIENT_NAME}.csr.pem" \
      -out "${CA_DIR}/certs/${CLIENT_NAME}.cert.pem"
chmod 444 "${CA_DIR}/certs/${CLIENT_NAME}.cert.pem"

echo "Verifying client key"
openssl x509 -noout -text -in "${CA_DIR}/certs/${CLIENT_NAME}.cert.pem"
openssl verify -CAfile "${CA_DIR}/certs/ca-chain.cert.pem" \
      "${CA_DIR}/certs/${CLIENT_NAME}.cert.pem"