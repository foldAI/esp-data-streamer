# ESP Data Streamer Client

A Python client for the data streamer example project. 
It discovers ESP devices on the local network and downloads files from them securely via mTLS.

## How It Works

1. The client discovers ESP devices advertising the ```_https._tcp.local.``` service via mDNS
2. Presents a list of discovered devices
3. After user selection, establishes secure connection using provided certificates
4. Downloads files from the "dir_stream" device endpoint

## Installation

```bash
pip install poetry
poetry install
```

## Usage

### Command Line

```bash
# run python client.py --help for more info on available arguments
python client.py --download-dir ./downloads \ 
                 --discovery-timeout 5
```

### Configuration File

You can also use a YAML configuration file to specify the options, for example:

```yaml
# config.yml
ca_path: /path/to/ca-chain.cert.pem
cert_path: /path/to/client.cert.pem
key_path: /path/to/client.key.pem
download_dir: ./downloads
discovery_timeout: 5
```

Then run with:

```bash
python client.py --config config.yml
```

Command line arguments take precedence over configuration file values.

## Options

- ```--download-dir```: Directory to store downloaded files (required)
- ```--ca-path```: Path to CA certificate chain (default: <script dir>/certs/ca.crt)
- ```--cert-path```: Path to client certificate (default: <script dir>/certs/client.crt)
- ```--key-path```: Path to client private key (default: <script dir>/certs/client.key)
- ```--discovery-timeout```: Timeout for device discovery in seconds (default: 3)
- ```--config```: Path to YAML configuration file (default: none)
- ```--from``` and ```--to```: Used to query the host as url params specifying the queried data range.

## Requirements

- Python 3.7+
- zeroconf
- requests
- click
- pyyaml

## License

[Apache 2.0](http://www.apache.org/licenses/LICENSE-2.0)
