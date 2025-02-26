# Creating certificates

This is not a production-ready way to create and manage certificates for your devices. It is just a way to bootstrap
certificates in order to run the ESP server(s) and remote client(s) and have them communicate securely over
the local network. Public Key Infrastructure management is a huge topic and outside the scope of this project.

Here, we will create a certificate authority that can issue certificates for our ESP server and any client we want to
spin up.

Please check out https://jamielinux.com/docs/openssl-certificate-authority/index.html for a walk-through of what we
are trying to accomplish.

## Certificate Authority creation

Go to scripts, run `scripts/create_CAs.sh`. This will locally create a root certificate authority, and an intermediate one.
The intermediate CA is used to create certificates for your ESP server, as well as for the client. With these,
server and clients can perform mutual TLS authentication and securely communicate with each other.
Make sure you specify a Common Name for the certificates.

## Creating the ESP server keys

Plug your esp device and run `scripts/create_server_key.sh`. This will read the MAC of the plugged-in esp,
so make sure it is connected (and no other esp device is connected at the same time).

Then:
- copy `ca/intermediate/certs/ca-chain.cert.pem` to `main/certs/ca.crt`
- copy `ca/intermediate/certs/esp-<MAC>.local.cert.pem` to `main/certs/server.crt`
- copy `ca/intermediate/private/esp-<MAC>.local.key.pem` to `main/certs/server.key`

## Creating the client keys

Run `scripts/create_client_key.sh`. You will find the generated keys
in `ca/intermediate/certs/client.cert.pem` and `ca/intermediate/private/client.key.pem`. The CA certificate is instead at
`ca/intermediate/certs/ca-chain.cert.pem`.

You will need these keys to run the client in `python/esp-data-sync/client.py`.
