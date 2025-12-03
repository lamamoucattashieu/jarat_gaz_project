import socket
import struct
import threading
import time
import math
import tkinter as tk
from tkinter import ttk, messagebox

# ---- Constants from your C project ----
MC_GROUP = "239.255.0.1"
MC_PORT = 5000
DROP_AGE_SEC = 3        # same as in common.h
HB_INTERVAL_MS = 1000

# Default user location (you can tweak in the UI)
DEFAULT_USER_LAT = 31.956
DEFAULT_USER_LON = 35.945


# ---- Haversine distance (same math as util.c) ----
def haversine_km(lat1, lon1, lat2, lon2):
    R = 6371.0
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = math.sin(dlat / 2) ** 2 + math.cos(math.radians(lat1)) * \
        math.cos(math.radians(lat2)) * math.sin(dlon / 2) ** 2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    return R * c


# ---- Data structure for trucks ----
class TruckInfo:
    def __init__(self, truck_id, lat, lon, tcp_port, ip):
        self.id = truck_id
        self.lat = lat
        self.lon = lon
        self.tcp_port = tcp_port
        self.ip = ip
        self.last_seen = time.time()

    def age_sec(self):
        return time.time() - self.last_seen


# ---- Multicast listener thread (HB parsing) ----
class TruckListener(threading.Thread):
    def __init__(self, trucks, lock):
        super().__init__(daemon=True)
        self.trucks = trucks
        self.lock = lock
        self.running = True
        self.seen_any = False  # for debug/status

    def run(self):
        # Create UDP socket and join multicast
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            # Bind to all interfaces on MC_PORT
            sock.bind(("", MC_PORT))
        except OSError as e:
            print(f"[UI] ERROR: could not bind UDP socket on port {MC_PORT}: {e}")
            return

        try:
            # More portable membership struct: group + interface
            mreq = struct.pack(
                "=4s4s",
                socket.inet_aton(MC_GROUP),
                socket.inet_aton("0.0.0.0"),
            )
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        except OSError as e:
            print(f"[UI] ERROR: could not join multicast group {MC_GROUP}: {e}")
            sock.close()
            return

        # Optional: enable loopback so sender on same host is seen
        try:
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
        except OSError:
            pass  # not fatal

        sock.settimeout(1.0)
        print(f"[UI] Listening for heartbeats on {MC_GROUP}:{MC_PORT} ...")

        while self.running:
            try:
                data, addr = sock.recvfrom(1024)
            except socket.timeout:
                continue
            except OSError:
                break

            line = data.decode("utf-8", errors="ignore").strip()
            if not line.startswith("HB "):
                continue

            parsed = self.parse_hb(line)
            if parsed is None:
                continue

            truck_id, lat, lon, tcp_port = parsed
            ip = addr[0]
            self.seen_any = True
            print(f"[UI] HB from {truck_id} @ {ip}:{tcp_port}  lat={lat} lon={lon}")

            with self.lock:
                t = self.trucks.get(truck_id)
                if t is None:
                    self.trucks[truck_id] = TruckInfo(truck_id, lat, lon, tcp_port, ip)
                else:
                    t.lat = lat
                    t.lon = lon
                    t.tcp_port = tcp_port
                    t.ip = ip
                    t.last_seen = time.time()

        sock.close()

    def parse_hb(self, line):
        # Expected HB line: HB truck_id=TRK01 lat=.. lon=.. ts=.. tcp=..
        parts = line.split()
        if len(parts) < 5:
            return None
        truck_id = None
        lat = lon = tcp = None
        for p in parts[1:]:
            if p.startswith("truck_id="):
                truck_id = p[len("truck_id="):]
            elif p.startswith("lat="):
                lat = float(p[len("lat="):])
            elif p.startswith("lon="):
                lon = float(p[len("lon="):])
            elif p.startswith("tcp="):
                tcp = int(p[len("tcp="):])
        if not truck_id or lat is None or lon is None or tcp is None:
            return None
        return truck_id, lat, lon, tcp

    def stop(self):
        self.running = False


# ---- TCP PING / ACK (same protocol as your C code) ----
def send_ping(truck, user_id, addr, note, timeout_sec=2.0):
    # PING format from protocol.c:
    # "PING truck_id=%s user_id=%s addr=\"%s\" note=\"%s\"\n"
    line = f'PING truck_id={truck.id} user_id={user_id} addr="{addr}" note="{note}"\n'

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout_sec)

    try:
        s.connect((truck.ip, truck.tcp_port))
        s.sendall(line.encode("utf-8"))

        # Read a single line until '\n'
        buf = b""
        while not buf.endswith(b"\n"):
            chunk = s.recv(1)
            if not chunk:
                break
            buf += chunk
        ack_line = buf.decode("utf-8", errors="ignore").strip()
    finally:
        s.close()

    # ACK format from protocol.c:
    # "ACK truck_id=%s eta_min=%d queued=%d\n"
    if not ack_line.startswith("ACK "):
        return {"raw": ack_line, "truck_id": None, "eta_min": None, "queued": None}

    parts = ack_line.split()
    truck_id = None
    eta = None
    queued = None
    for p in parts[1:]:
        if p.startswith("truck_id="):
            truck_id = p[len("truck_id="):]
        elif p.startswith("eta_min="):
            eta = int(p[len("eta_min="):])
        elif p.startswith("queued="):
            queued = int(p[len("queued="):])

    return {
        "raw": ack_line,
        "truck_id": truck_id,
        "eta_min": eta,
        "queued": queued,
    }


# ---- Modern minimal Tkinter UI ----
class App:
    def __init__(self, root):
        self.root = root
        self.root.title("Truck Dispatcher")
        self.root.geometry("780x520")
        self.root.minsize(720, 480)

        # Dark theme
        self._setup_style()

        # Shared state
        self.trucks = {}
        self.trucks_lock = threading.Lock()

        # ---- Layout ----
        outer = ttk.Frame(root, padding=16)
        outer.pack(fill="both", expand=True)

        # Title row
        title_row = ttk.Frame(outer)
        title_row.pack(fill="x")
        ttk.Label(
            title_row,
            text="Truck Dispatcher",
            style="Title.TLabel",
        ).pack(side="left")
        ttk.Label(
            title_row,
            text="Live trucks • click one to send a request",
            style="SubTitle.TLabel",
        ).pack(side="right")

        # Top controls (user location + info)
        top = ttk.Frame(outer)
        top.pack(fill="x", pady=(16, 8))

        # User location
        loc_frame = ttk.LabelFrame(top, text="User location", padding=8)
        loc_frame.pack(side="left", fill="x", expand=True, padx=(0, 8))

        ttk.Label(loc_frame, text="Lat").grid(row=0, column=0, sticky="w")
        self.ent_lat = ttk.Entry(loc_frame, width=10)
        self.ent_lat.insert(0, str(DEFAULT_USER_LAT))
        self.ent_lat.grid(row=0, column=1, padx=(4, 12))

        ttk.Label(loc_frame, text="Lon").grid(row=0, column=2, sticky="w")
        self.ent_lon = ttk.Entry(loc_frame, width=10)
        self.ent_lon.insert(0, str(DEFAULT_USER_LON))
        self.ent_lon.grid(row=0, column=3, padx=(4, 0))

        # User request
        req_frame = ttk.LabelFrame(top, text="Request", padding=8)
        req_frame.pack(side="left", fill="x", expand=True)

        ttk.Label(req_frame, text="User ID").grid(row=0, column=0, sticky="w")
        self.ent_user = ttk.Entry(req_frame, width=10)
        self.ent_user.insert(0, "USR1")
        self.ent_user.grid(row=0, column=1, padx=(4, 12))

        ttk.Label(req_frame, text="Address").grid(row=0, column=2, sticky="w")
        self.ent_addr = ttk.Entry(req_frame, width=24)
        self.ent_addr.grid(row=0, column=3, padx=(4, 12))

        ttk.Label(req_frame, text="Message to driver").grid(row=0, column=4, sticky="w")
        self.ent_note = ttk.Entry(req_frame, width=20)
        self.ent_note.grid(row=0, column=5, padx=(4, 0))

        # Middle: trucks list
        middle = ttk.Frame(outer)
        middle.pack(fill="both", expand=True, pady=(8, 8))

        list_frame = ttk.LabelFrame(middle, text="Trucks", padding=(8, 4))
        list_frame.pack(fill="both", expand=True, side="left")

        columns = ("id", "distance", "age", "tcp", "ip")
        self.tree = ttk.Treeview(
            list_frame,
            columns=columns,
            show="headings",
            selectmode="browse",
            height=12,
        )
        self.tree.pack(fill="both", expand=True)

        self.tree.heading("id", text="Truck ID")
        self.tree.heading("distance", text="Distance (km)")
        self.tree.heading("age", text="Last seen (s)")
        self.tree.heading("tcp", text="TCP")
        self.tree.heading("ip", text="IP")

        self.tree.column("id", width=100, anchor="w")
        self.tree.column("distance", width=120, anchor="e")
        self.tree.column("age", width=120, anchor="e")
        self.tree.column("tcp", width=60, anchor="center")
        self.tree.column("ip", width=140, anchor="w")

        # Scrollbar
        scrollbar = ttk.Scrollbar(
            list_frame, orient="vertical", command=self.tree.yview
        )
        self.tree.configure(yscrollcommand=scrollbar.set)
        scrollbar.pack(side="right", fill="y")

        # Right side: actions + ACK
        side = ttk.Frame(middle)
        side.pack(fill="y", side="left", padx=(8, 0))

        self.btn_refresh = ttk.Button(
            side, text="Refresh", command=self.refresh_list, style="Accent.TButton"
        )
        self.btn_refresh.pack(fill="x", pady=(0, 4))

        self.btn_ping = ttk.Button(
            side,
            text="Ping selected",
            command=self.do_ping_selected,
            style="Accent.TButton",
        )
        self.btn_ping.pack(fill="x", pady=(0, 12))

        ack_frame = ttk.LabelFrame(side, text="Last ACK", padding=8)
        ack_frame.pack(fill="both", expand=True)

        self.lbl_ack_title = ttk.Label(
            ack_frame, text="– no ACK yet –", style="AckTitle.TLabel", wraplength=220
        )
        self.lbl_ack_title.pack(anchor="w")

        self.lbl_ack_body = ttk.Label(
            ack_frame,
            text="",
            style="AckBody.TLabel",
            justify="left",
            wraplength=220,
        )
        self.lbl_ack_body.pack(anchor="w", pady=(4, 0))

        # Status bar
        self.status = ttk.Label(
            outer, text="Waiting for heartbeats…", style="Status.TLabel", anchor="w"
        )
        self.status.pack(fill="x", pady=(8, 0))

        # Start multicast listener
        self.listener = TruckListener(self.trucks, self.trucks_lock)
        self.listener.start()

        # Periodic refresh
        self.root.after(1000, self.refresh_and_prune)

        # Close hook
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    # ---------- Styling ----------
    def _setup_style(self):
        dark_bg = "#111218"
        mid_bg = "#181924"
        accent = "#4CAF50"
        accent_hover = "#66BB6A"
        text_main = "#F3F4F8"
        text_sub = "#A0A4B8"

        self.root.configure(bg=dark_bg)

        style = ttk.Style(self.root)
        style.theme_use("clam")

        style.configure(
            ".",  # base style
            background=dark_bg,
            foreground=text_main,
            fieldbackground=mid_bg,
            bordercolor=mid_bg,
            lightcolor=mid_bg,
            darkcolor=mid_bg,
            highlightthickness=0,
        )

        style.configure("TFrame", background=dark_bg)
        style.configure("TLabelframe", background=mid_bg, foreground=text_sub)
        style.configure("TLabelframe.Label", background=mid_bg, foreground=text_sub)
        style.configure("TLabel", background=dark_bg, foreground=text_main)
        style.configure("TEntry", padding=4, relief="flat", foreground=text_main)
        style.map("TEntry", fieldbackground=[("readonly", mid_bg)])

        style.configure(
            "Title.TLabel",
            font=("Segoe UI", 16, "bold"),
            foreground=text_main,
            background=dark_bg,
        )
        style.configure(
            "SubTitle.TLabel",
            font=("Segoe UI", 9),
            foreground=text_sub,
            background=dark_bg,
        )

        style.configure(
            "Status.TLabel",
            font=("Segoe UI", 9),
            foreground=text_sub,
            background=dark_bg,
        )

        style.configure(
            "AckTitle.TLabel",
            font=("Segoe UI", 10, "bold"),
            foreground=text_main,
            background=mid_bg,
        )
        style.configure(
            "AckBody.TLabel",
            font=("Segoe UI", 9),
            foreground=text_sub,
            background=mid_bg,
        )

        style.configure(
            "Accent.TButton",
            font=("Segoe UI", 9, "bold"),
            background=accent,
            foreground="#ffffff",
            borderwidth=0,
            focusthickness=0,
            padding=(10, 6),
        )
        style.map(
            "Accent.TButton",
            background=[("active", accent_hover)],
            foreground=[("disabled", "#777777")],
        )

        style.configure(
            "Treeview",
            background=mid_bg,
            fieldbackground=mid_bg,
            foreground=text_main,
            borderwidth=0,
            rowheight=22,
        )
        style.configure(
            "Treeview.Heading",
            background="#202231",
            foreground=text_sub,
            font=("Segoe UI", 9, "bold"),
        )
        style.map(
            "Treeview",
            background=[("selected", "#263238")],
            foreground=[("selected", "#ffffff")],
        )

    # ---------- Logic ----------
    def get_user_coords(self):
        try:
            lat = float(self.ent_lat.get().strip())
            lon = float(self.ent_lon.get().strip())
            return lat, lon
        except ValueError:
            messagebox.showerror("Invalid input", "User latitude/longitude are invalid.")
            return None, None

    def refresh_and_prune(self):
        now = time.time()
        # prune stale trucks
        with self.trucks_lock:
            to_delete = [
                tid for tid, t in self.trucks.items() if now - t.last_seen > DROP_AGE_SEC
            ]
            for tid in to_delete:
                del self.trucks[tid]

        self.refresh_list()

        # Update status if no heartbeats at all yet
        if not self.listener.seen_any:
            self.status.configure(
                text=f"Waiting for heartbeats on {MC_GROUP}:{MC_PORT}… "
                     f"(is the truck binary running in the same environment?)"
            )

        self.root.after(1000, self.refresh_and_prune)

    def refresh_list(self):
        lat, lon = self.get_user_coords()
        if lat is None:
            return

        # 🔹 Remember selection before refreshing
        selected = self.tree.selection()
        selected_id = selected[0] if selected else None

        # Build sorted list of trucks
        items = []
        with self.trucks_lock:
            for t in self.trucks.values():
                dist = haversine_km(lat, lon, t.lat, t.lon)
                items.append((dist, t))
        items.sort(key=lambda x: x[0])

        # Update Treeview
        self.tree.delete(*self.tree.get_children())

        existing_ids = set()
        for dist, t in items:
            age = int(t.age_sec())
            self.tree.insert(
                "",
                "end",
                iid=t.id,   # <--- iid is truck ID, important for re-select
                values=(
                    t.id,
                    f"{dist:0.3f}",
                    f"{age}",
                    str(t.tcp_port),
                    t.ip,
                ),
            )
            existing_ids.add(t.id)

        # 🔹 Restore selection, if that truck is still present
        if selected_id and selected_id in existing_ids:
            self.tree.selection_set(selected_id)
            self.tree.focus(selected_id)

        if items:
            self.status.configure(
                text=f"{len(items)} truck(s) online · updated just now"
            )
        elif self.listener.seen_any:
            self.status.configure(text="No trucks currently online.")

    def _get_selected_truck(self):
        sel = self.tree.selection()
        if not sel:
            messagebox.showwarning("No selection", "Select a truck in the list first.")
            return None
        truck_id = sel[0]
        with self.trucks_lock:
            return self.trucks.get(truck_id)

    def do_ping_selected(self):
        truck = self._get_selected_truck()
        if not truck:
            return

        user_id = self.ent_user.get().strip() or "USR1"
        addr = self.ent_addr.get().strip()
        note = self.ent_note.get().strip()

        def worker():
            try:
                res = send_ping(truck, user_id, addr, note)
                if res["truck_id"] is None:
                    title = "Bad ACK"
                    body = f"Raw response:\n{res['raw']}"
                    popup = None
                else:
                    title = f"ACK from {res['truck_id']}"
                    body = (
                        f"ETA: {res['eta_min']} min\n"
                        f"Queued: {res['queued']}\n\n"
                        f"Raw: {res['raw']}"
                    )
                    popup = (
                        f"Your message has been sent to {res['truck_id']}.\n\n"
                        f"Message:\n{note or '(no message)'}\n\n"
                        f"The driver is on their way.\n"
                        f"Estimated arrival: {res['eta_min']} minute(s)."
                    )
            except Exception as e:
                title = "Error"
                body = str(e)
                popup = None

            def update():
                self.lbl_ack_title.configure(text=title)
                self.lbl_ack_body.configure(text=body)
                if popup:
                    messagebox.showinfo("Request sent", popup)

            self.root.after(0, update)

        threading.Thread(target=worker, daemon=True).start()

    def on_close(self):
        self.listener.stop()
        self.root.destroy()


# No __name__ guard to avoid your earlier "__name__ is not defined" issue
root = tk.Tk()
app = App(root)
root.mainloop()
