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

# see https://jamielinux.com/docs/openssl-certificate-authority/index.html
set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_CA_DIR="${SCRIPT_DIR}/ca/root"
INTR_CA_DIR="${SCRIPT_DIR}/ca/intermediate"

echo -e "\033[0;31mTHIS IS JUST AN EXAMPLE SCRIPT, DO NOT USE IN PRODUCTION\033[0m"

### Root CA
echo "Creating root CA initial dir structure..."
mkdir -p "${ROOT_CA_DIR}"/{private,certs,crl,newcerts}
chmod 700 "${ROOT_CA_DIR}/private"
touch "${ROOT_CA_DIR}/index.txt"
echo 1000 > "${ROOT_CA_DIR}/serial"

echo "Generating CA private key..."
openssl genrsa -aes256 -out "${ROOT_CA_DIR}/private/ca.key.pem" 4096
chmod 400 "${ROOT_CA_DIR}/private/ca.key.pem"

echo "Generating root CA certificate (validity=100y)..."
cp "${SCRIPT_DIR}/root-openssl-template.cnf" "${ROOT_CA_DIR}/openssl.cnf"
CA_DIR=${ROOT_CA_DIR} openssl req -config "${ROOT_CA_DIR}/openssl.cnf" -key "${ROOT_CA_DIR}/private/ca.key.pem" \
    -new -x509 -days 36500 -sha256 -extensions v3_ca \
    -out "${ROOT_CA_DIR}/certs/ca.cert.pem"
chmod 444 "${ROOT_CA_DIR}/certs/ca.cert.pem"

echo "Verifying certificate..."
openssl x509 -noout -text -in "${ROOT_CA_DIR}/certs/ca.cert.pem"

##### Intermediate CA
echo "Creating intermediate CA initial dir structure..."
mkdir -p "${INTR_CA_DIR}"/{private,certs,csr,crl,newcerts}
chmod 700 "${INTR_CA_DIR}/private"
touch "${INTR_CA_DIR}/index.txt"
echo 1000 > "${INTR_CA_DIR}/serial"
echo 1000 > "${INTR_CA_DIR}/crlnumber"

echo "Generating intermediate CA private key..."
openssl genrsa -aes256 -out "${INTR_CA_DIR}/private/intermediate.key.pem" 4096
chmod 400 "${INTR_CA_DIR}/private/intermediate.key.pem"

echo "Generating intermediate CA certificate"
cp "${SCRIPT_DIR}/intermediate-openssl-template.cnf" "${INTR_CA_DIR}/openssl.cnf"
CA_DIR=${INTR_CA_DIR} SAN="DNS:intermediate.ca" openssl req -config "${INTR_CA_DIR}/openssl.cnf" -new -sha256 \
    -key "${INTR_CA_DIR}/private/intermediate.key.pem" \
    -out "${INTR_CA_DIR}/csr/intermediate.csr.pem"

echo "Signing intermediate CA certificate (validity=100y) with root CA"
CA_DIR=${ROOT_CA_DIR} openssl ca -config "${ROOT_CA_DIR}/openssl.cnf" -extensions v3_intermediate_ca \
      -days 36500 -notext -md sha256 \
      -in "${INTR_CA_DIR}/csr/intermediate.csr.pem" \
      -out "${INTR_CA_DIR}/certs/intermediate.cert.pem"
chmod 444 "${INTR_CA_DIR}/certs/intermediate.cert.pem"

echo "Verifying intermediate CA certificate"
openssl x509 -noout -text -in "${INTR_CA_DIR}/certs/intermediate.cert.pem"
openssl verify -CAfile "${ROOT_CA_DIR}/certs/ca.cert.pem" "${INTR_CA_DIR}/certs/intermediate.cert.pem"

echo "Creating certificate chain"
cat "${INTR_CA_DIR}/certs/intermediate.cert.pem" "${ROOT_CA_DIR}/certs/ca.cert.pem" > "${INTR_CA_DIR}/certs/ca-chain.cert.pem"
chmod 444 "${INTR_CA_DIR}/certs/ca-chain.cert.pem"
