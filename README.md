# Smart Gas Truck Tracker – Jarat Gaz Project

A network-based simulation system implemented entirely in C that enables real-time tracking of gas delivery trucks, client-to-truck communication, and service request handling. This project models a modern and efficient replacement for traditional gas truck alerts by using multicast discovery, TCP messaging, and an optional graphical user interface.

# 1. Overview

This system follows a client–server model with the following components:

**Truck Program (Server)**

Simulates a gas truck with GPS movement.

Broadcasts heartbeat packets (location, ID, TCP port) using UDP multicast.

Accepts delivery requests (PING messages) from clients over TCP.

Responds with an estimated arrival time (ETA) and queue length.

Logs all received requests to a CSV file.

**Client Program (User)**

The client has two operational modes:

List Mode:
Displays all discovered trucks in real time, sorted by distance and updated every second.

Ping Mode:
Sends a structured service request to a specific truck and prints the server’s acknowledgment.

**Graphical UI**

A separate graphical interface is provided to display:

**Active trucks**

Request/response traffic

System logs

Status updates

The UI mirrors the core system activity shown in the terminal.

# 2. Building the Project (CMake)

From the root of the repository:

mkdir build
cd build
cmake ..
cmake --build .


**This produces two executables:**

build/truck
build/client

**Running Tests (GoogleTest)**
cd build
ctest --output-on-failure


GoogleTest is automatically downloaded and compiled as part of the CMake configuration.

# 3. Running the System

Because this project simulates a distributed system, two or three terminals are required.

Terminal 1: Start the Truck (Server)
cd build
./truck --id T1 --tcp 5555 --start-lat 31.95 --start-lon 35.94


**Expected server output:**

truck T1 up: tcp=5555, mc=239.255.0.1:5000


**The truck now:**

Broadcasts heartbeats every second

Accepts PING requests

Terminal 2: Start the Client in List Mode
cd build
./client


**Expected output (updated every second):**

truck_id       distance_km  last_seen_s  tcp_port  ip
T1                 0.521           0       5555     192.168.1.8


**The client automatically:**

Joins the multicast group

Receives truck heartbeats

Sorts trucks by distance

Terminal 3: Send a PING Request
cd build
./client \
  --truck T1 \
  --user USR1 \
  --addr "Irbid" \
  --note "Deliver gas"


**The client sends:**

PING truck_id=T1 user_id=USR1 addr="Irbid" note="Deliver gas"


The truck responds with:

ACK truck_id=T1 eta_min=4 queued=1


**Output on the client:**

ACK from T1: eta=4 min queued=1

# 4. Running the Graphical UI

A separate UI folder is included in the project. The UI displays truck data, client messages, acknowledgments, and system logs.

If the UI is written in Python
cd ui
python3 ui.py


The UI can run in parallel with both the truck and the client programs.

# 6. Notes

Running the system requires at least two terminals; ping mode requires a third.

The UI may be launched at any point and will reflect current system activity.

Network multicast must be supported by the host system or virtual environment.

This project is compatible with Linux, macOS, and WSL.
