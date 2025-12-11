#!/usr/bin/env python3
"""
Wake-on-LAN (WOL) script for CafeInternetManager
Sends magic packets to wake client machines on the local network.

Usage:
    ./wol.py <mac_address>           # Wake a single machine
    ./wol.py --all                   # Wake all machines from config
    ./wol.py --list                  # List configured machines

Prerequisites on client machines:
    1. Enable WOL in BIOS/UEFI
    2. Enable WOL in network driver: sudo ethtool -s eth0 wol g
    3. Some systems may need: sudo systemctl enable wol@eth0
"""

import socket
import sys
import os
import json
from pathlib import Path

# Configuration file path (same directory as this script)
CONFIG_FILE = Path(__file__).parent / "machines.json"

# Default broadcast address (works for most LANs)
BROADCAST_ADDR = "255.255.255.255"
WOL_PORT = 9  # Standard WOL port


def create_magic_packet(mac_address: str) -> bytes:
    """
    Create a Wake-on-LAN magic packet.

    The magic packet consists of:
    - 6 bytes of 0xFF
    - 16 repetitions of the target MAC address (6 bytes each)
    Total: 102 bytes
    """
    # Clean MAC address (remove separators)
    mac = mac_address.replace(":", "").replace("-", "").replace(".", "")

    if len(mac) != 12:
        raise ValueError(f"Invalid MAC address: {mac_address}")

    # Validate hex characters
    try:
        mac_bytes = bytes.fromhex(mac)
    except ValueError:
        raise ValueError(f"Invalid MAC address (non-hex characters): {mac_address}")

    # Build magic packet: 6 x 0xFF + 16 x MAC
    magic_packet = b"\xff" * 6 + mac_bytes * 16
    return magic_packet


def send_wol(
    mac_address: str, broadcast: str = BROADCAST_ADDR, port: int = WOL_PORT
) -> bool:
    """
    Send a Wake-on-LAN magic packet to the specified MAC address.

    Returns True on success, False on failure.
    """
    try:
        packet = create_magic_packet(mac_address)

        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            sock.sendto(packet, (broadcast, port))

        print(f"✓ Magic packet sent to {mac_address}")
        return True

    except ValueError as e:
        print(f"✗ Error: {e}")
        return False
    except OSError as e:
        print(f"✗ Network error: {e}")
        return False


def load_machines() -> dict:
    """Load machine configurations from JSON file."""
    if not CONFIG_FILE.exists():
        return {}

    try:
        with open(CONFIG_FILE) as f:
            return json.load(f)
    except json.JSONDecodeError as e:
        print(f"Error parsing {CONFIG_FILE}: {e}")
        return {}


def save_machines(machines: dict) -> None:
    """Save machine configurations to JSON file."""
    with open(CONFIG_FILE, "w") as f:
        json.dump(machines, f, indent=2)


def create_sample_config() -> None:
    """Create a sample configuration file if it doesn't exist."""
    if CONFIG_FILE.exists():
        print(f"Config file already exists: {CONFIG_FILE}")
        return

    sample = {
        "pc1": {
            "mac": "AA:BB:CC:DD:EE:01",
            "ip": "192.168.1.101",
            "description": "Workstation 1",
        },
        "pc2": {
            "mac": "AA:BB:CC:DD:EE:02",
            "ip": "192.168.1.102",
            "description": "Workstation 2",
        },
    }
    save_machines(sample)
    print(f"Created sample config: {CONFIG_FILE}")
    print("Edit this file to add your machine MAC addresses.")


def list_machines() -> None:
    """List all configured machines."""
    machines = load_machines()

    if not machines:
        print("No machines configured.")
        print(f"Run: {sys.argv[0]} --init  to create a sample config")
        return

    print(f"{'Name':<15} {'MAC Address':<20} {'IP':<15} {'Description'}")
    print("-" * 70)

    for name, info in machines.items():
        mac = info.get("mac", "N/A")
        ip = info.get("ip", "N/A")
        desc = info.get("description", "")
        print(f"{name:<15} {mac:<20} {ip:<15} {desc}")


def wake_all() -> None:
    """Wake all configured machines."""
    machines = load_machines()

    if not machines:
        print("No machines configured.")
        return

    print(f"Waking {len(machines)} machine(s)...")
    success = 0

    for name, info in machines.items():
        mac = info.get("mac")
        if mac:
            print(f"  {name}: ", end="")
            if send_wol(mac):
                success += 1

    print(f"\nSent {success}/{len(machines)} wake packets.")


def wake_by_name(name: str) -> None:
    """Wake a machine by its configured name."""
    machines = load_machines()

    if name not in machines:
        print(f"Unknown machine: {name}")
        print(
            "Available machines:", ", ".join(machines.keys()) if machines else "(none)"
        )
        return

    mac = machines[name].get("mac")
    if mac:
        send_wol(mac)
    else:
        print(f"No MAC address configured for {name}")


def print_usage() -> None:
    """Print usage information."""
    print(__doc__)
    print("Commands:")
    print("  wol.py <mac_address>     Wake machine by MAC (e.g., AA:BB:CC:DD:EE:FF)")
    print("  wol.py <machine_name>    Wake machine by configured name")
    print("  wol.py --all             Wake all configured machines")
    print("  wol.py --list            List configured machines")
    print("  wol.py --init            Create sample config file")
    print("  wol.py --help            Show this help")


def main() -> int:
    if len(sys.argv) < 2:
        print_usage()
        return 1

    arg = sys.argv[1]

    if arg in ("--help", "-h"):
        print_usage()
        return 0

    if arg == "--list":
        list_machines()
        return 0

    if arg == "--all":
        wake_all()
        return 0

    if arg == "--init":
        create_sample_config()
        return 0

    # Check if it looks like a MAC address (contains : or -)
    if ":" in arg or "-" in arg or len(arg) == 12:
        send_wol(arg)
    else:
        # Try as a machine name
        wake_by_name(arg)

    return 0


if __name__ == "__main__":
    sys.exit(main())
