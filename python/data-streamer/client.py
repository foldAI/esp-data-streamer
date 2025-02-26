#   Copyright 2025 OIST
#   Copyright 2025 fold ecosystemics
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
"""Simple script to:
- discover ESP devices in local network, advertising via mDNS
- connect to chosen device
- download its data
"""

import logging
import os
import re
from pathlib import Path
from time import time, sleep
from typing import Optional
from urllib.parse import urljoin

import click
import requests
import yaml
from requests.exceptions import ChunkedEncodingError
from zeroconf import ServiceListener, Zeroconf, ServiceBrowser

CHUNK_SIZE = 8192
SERVICE_TYPE = "_https._tcp.local."
ENDPOINT = "dir_stream"

logging.basicConfig(format='[%(asctime)s] %(message)s')
LOGGER = logging.getLogger(__name__)
LOGGER.setLevel(logging.DEBUG)


class ESPListener(ServiceListener):
    """mDNS listener for ESP devices advertising in local network"""
    def __init__(self):
        self.devices = {}

    def update_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        pass

    def remove_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        if name in self.devices:
            del self.devices[name]

    def add_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        if not name.startswith("esp-"):
            return
        info = zc.get_service_info(type_, name)
        if info:
            hostname = name.split(".")[0]
            self.devices[hostname] = info


class Client:
    def __init__(self, base_url: str, cert_path: str, key_path: str,
                 ca_path: str, download_dir: str):
        """Initialize the client with server details and certificates.

        Args:
            base_url: Base URL of the server (e.g., 'https://example.com')
            cert_path: Path to client certificate
            key_path: Path to client private key
            ca_path: Path to CA certificate
            download_dir: Directory where files will be downloaded
        """
        self.base_url = base_url.rstrip('/')
        self.download_dir = download_dir
        # Create download directory if it doesn't exist
        os.makedirs(download_dir, exist_ok=True)

        # Setup session with SSL certificates
        self.session = requests.Session()
        self.session.verify = ca_path
        self.session.cert = (cert_path, key_path)

    def _calculate_speed(self, bytes_received: int, total_bytes: int, elapsed_time: float) -> tuple[float, float]:
        """Calculate current and average speed in bytes/second."""
        if elapsed_time == 0:
            return 0.0, 0.0
        current_speed = bytes_received / elapsed_time
        avg_speed = total_bytes / elapsed_time
        return current_speed, avg_speed

    def _format_speed(self, speed: float) -> str:
        """Format speed in human-readable format."""
        for unit in ['B/s', 'KB/s', 'MB/s', 'GB/s']:
            if speed < 1024.0:
                return f"{speed:.2f} {unit}"
            speed /= 1024.0
        return f"{speed:.2f} TB/s"

    def _handle_multipart_response(self, response: requests.Response) -> None:
        """Handle multipart response and save files."""
        # Get boundary from Content-Type header
        start_time = time()
        content_type = response.headers.get('Content-Type', '')
        boundary_match = re.search(r'boundary=(.*)', content_type)
        if not boundary_match:
            raise ValueError("No boundary found in Content-Type header")

        boundary = boundary_match.group(1).encode("utf-8")
        current_file = None
        buffer = bytearray()
        tot_bytes = 0

        for chunk in response.iter_content(chunk_size=CHUNK_SIZE, decode_unicode=False):
            time_now = time()
            tot_bytes = tot_bytes + len(chunk)
            if chunk:
                buffer += chunk
                while buffer:
                    if buffer.startswith(b'\r\n--' + boundary):  # starting of part
                        if current_file:
                            current_file.close()
                            current_file = None
                        headers_end = buffer.find(b'\r\n\r\n')
                        if headers_end == -1:
                            break

                        headers = buffer[:headers_end].decode("utf-8")
                        name_match = re.search(r'X-Part-Name:\s*"([^"]+)"', headers)
                        if name_match:
                            current_filename = name_match.group(1)
                            current_file = open(os.path.join(self.download_dir, current_filename), 'wb')
                            LOGGER.info("Downloading: %s", current_filename)
                            _, avg_speed = self._calculate_speed(bytes_received=len(chunk), total_bytes=tot_bytes,
                                                                 elapsed_time=time_now - start_time)
                            LOGGER.info("Avg speed: %s", self._format_speed(avg_speed))

                        buffer = buffer[headers_end + 4:]

                    elif buffer.startswith(b'\r\n--%b--' % boundary):  # ending of all parts
                        print("Done")
                        if current_file:
                            current_file.close()
                        break

                    elif current_file:  # we're currently in the middle of a part
                        next_boundary = buffer.find(b'\r\n--%b' %boundary)
                        if next_boundary != -1:
                            data = buffer[:next_boundary]
                            current_file.write(data)
                            buffer = buffer[next_boundary:]
                        else:
                            safe_length = max(0, len(buffer) - CHUNK_SIZE)
                            if safe_length > 0:
                                data = buffer[:safe_length]
                                current_file.write(data)
                                buffer = buffer[safe_length:]
                            break
                    else:
                        break

    def _handle_single_file_response(self, response: requests.Response) -> None:
        """Handle single file response and save it."""
        # get filename from Content-Disposition header
        cd = response.headers.get('Content-Disposition')
        if not cd:
            raise ValueError("No Content-Disposition header")
        if cd:
            filename_match = re.search(r'filename="?([^"]+)"?', cd)
            if not filename_match:
                raise ValueError("No filename in Content-Disposition header")

        filename = filename_match.group(1)
        filepath = os.path.join(self.download_dir, filename)
        with open(filepath, 'wb') as f:
            for chunk in response.iter_content(chunk_size=8192):
                if chunk:
                    f.write(chunk)
        print(f"Downloaded: {filename}")

    def download(self, endpoint: str, from_=None, to=None) -> None:
        """Download data from the specified endpoint.

        Args:
            endpoint: The endpoint to retrieve from (e.g., '/files')
            from_: name for which any filename <= of url_from (lexicographically) is not downloaded
            to: name for which any filename > of url_to (lexicographically) is not downloaded
        """
        try:
            url = urljoin(self.base_url, endpoint.lstrip('/')) + ":443"
            params = {}
            if from_ is not None:
                params['from'] = from_
            if to is not None:
                params['to'] = to
            response = self.session.get(url, params=params, stream=True)
            response.raise_for_status()
            content_type = response.headers.get('Content-Type', '')
            if 'multipart' in content_type:
                self._handle_multipart_response(response)
            else:
                self._handle_single_file_response(response)
        except ChunkedEncodingError as e:
            print(f"Error during chunked transfer: {e}")
        except Exception as e:
            print(f"Error: {e}")

    def __del__(self):
        """Cleanup session on object destruction."""
        self.session.close()


def discover_devices(timeout: int=3) -> dict:
    """Discover ESP devices on the network.

    Args:
        timeout: How long to wait for device discovery (seconds)

    Returns:
        Dictionary of discovered devices {device_id: service_info}
    """
    zeroconf = Zeroconf()
    listener = ESPListener()
    browser = ServiceBrowser(zeroconf, SERVICE_TYPE, listener)

    # Wait for devices to be discovered
    sleep(timeout)

    zeroconf.close()
    return listener.devices


def config_callback(ctx, param, value):
    """Load config file and set it in context."""
    if value:
        map = yaml.safe_load(Path(value).read_text())
        if "from" in map:
            map["from_"] = map.pop("from")
        ctx.default_map = map
    return value


def get_default_cert_path(cert_name):
    """Return default value for certificate file path."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    return str(os.path.join(script_dir, 'certs', cert_name))


@click.command()
@click.option('--config',
              type=click.Path(exists=True, path_type=Path),
              is_eager=True,
              callback=config_callback,
              help='Path to YAML config file. Entries with same key as cli arguments will be substituted. '
                   'See https://click.palletsprojects.com/en/stable/commands/#overriding-defaults'
              )
@click.option('--ca-path',
              default=get_default_cert_path('ca.crt'),
              type=click.Path(exists=True, path_type=str),
              help='Path to CA certificate chain')
@click.option('--cert-path',
              default=get_default_cert_path('client.crt'),
              type=click.Path(exists=True, path_type=str),
              help='Path to client certificate')
@click.option('--key-path',
              default=get_default_cert_path('client.key'),
              type=click.Path(exists=True, path_type=str),
              help='Path to client private key')
@click.option('--download-dir', required=True, type=click.Path(),
              help='Directory in which to store downloaded files')
@click.option('--discovery-timeout', default=3, type=int,
              help='Timeout for device discovery (seconds)')
@click.option('--from', 'from_', default=None, type=str,
              help='Range start for query')
@click.option('--to', default=None, type=str,
              help='Range end for query')
def main(config: dict, ca_path: str, cert_path: str, key_path: str,
         download_dir: str, discovery_timeout: int,
         from_: Optional[str], to: Optional[str]) -> None:
    """ESP File Stream Client.

    Discovers ESP devices on the network and downloads files from the selected device.
    """
    click.echo("Discovering devices...")
    devices = discover_devices(discovery_timeout)
    if not devices:
        LOGGER.error("No devices found!")
        return

    # Present device selection menu
    click.echo("Available devices:")
    for idx, device_id in enumerate(devices.keys(), 1):
        click.echo(f"{idx}. {device_id}")
    while True:
        try:
            choice = click.prompt("Select device (number)", type=int)
            if 1 <= choice <= len(devices):
                break
            click.echo("Invalid selection. Please try again.")
        except ValueError:
            click.echo("Please enter a number.")

    # Get selected device's address
    device_id = list(devices.keys())[choice - 1]
    base_url = f"https://{device_id}.local"

    client = Client(
        base_url=base_url,
        ca_path=ca_path,
        cert_path=cert_path,
        key_path=key_path,
        download_dir=download_dir
    )
    click.echo(f"Connecting to {base_url}...")
    client.download(ENDPOINT, from_=from_, to=to)


if __name__ == "__main__":
    main()
