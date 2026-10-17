// Serial ports for the desktop app, shaped after Web Serial so the same TypeScript drives the browser and Tauri.
// Data from a port goes to the webview on a Channel as raw bytes, writes come as raw invoke bodies.

use std::collections::HashMap;
use std::io::{ErrorKind, Read};
use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};
use std::sync::{Arc, Mutex};
use std::thread::JoinHandle;
use std::time::{Duration, Instant};

use serde::Serialize;
use tauri::ipc::{Channel, InvokeBody, InvokeResponseBody, Request};
use tauri::State;

#[derive(Serialize)]
pub struct PortInfo {
    path: String,
    vid: Option<u16>,
    pid: Option<u16>,
    product: Option<String>,
    manufacturer: Option<String>,
    serial: Option<String>,
}

// each port has its own lock: a write to a device that takes no data must not hold up the others
struct OpenPort {
    port: Arc<Mutex<Box<dyn serialport::SerialPort>>>,
    stop: Arc<AtomicBool>,
    reader: Option<JoinHandle<()>>,
}

#[derive(Default)]
pub struct Ports {
    next_id: AtomicU32,
    open: Mutex<HashMap<u32, OpenPort>>,
}

impl Ports {
    // a reloaded page has lost the ids of what it opened, so nothing else would close those ports
    pub fn close_all(&self) {
        let all: Vec<OpenPort> = self.open.lock().unwrap().drain().map(|(_, p)| p).collect();
        for mut p in all {
            p.stop.store(true, Ordering::Relaxed);
            if let Some(t) = p.reader.take() {
                let _ = t.join();
            }
        }
    }
}

#[tauri::command]
pub fn serial_list() -> Result<Vec<PortInfo>, String> {
    let ports = serialport::available_ports().map_err(|e| e.to_string())?;
    Ok(ports
        .into_iter()
        .map(|p| match p.port_type {
            serialport::SerialPortType::UsbPort(u) => PortInfo {
                path: p.port_name,
                vid: Some(u.vid),
                pid: Some(u.pid),
                product: u.product,
                manufacturer: u.manufacturer,
                serial: u.serial_number,
            },
            _ => PortInfo { path: p.port_name, vid: None, pid: None, product: None, manufacturer: None, serial: None },
        })
        .collect())
}

#[tauri::command]
pub fn serial_open(path: String, baud: u32, on_data: Channel<InvokeResponseBody>, ports: State<'_, Ports>) -> Result<u32, String> {
    let port = serialport::new(&path, baud)
        .timeout(Duration::from_millis(20))
        .open()
        .map_err(|e| format!("{path}: {e}"))?;
    let mut rx = port.try_clone().map_err(|e| e.to_string())?;
    let stop = Arc::new(AtomicBool::new(false));
    let stop_reader = stop.clone();
    let reader = std::thread::spawn(move || {
        let mut buf = [0u8; 4096];
        while !stop_reader.load(Ordering::Relaxed) {
            match rx.read(&mut buf) {
                Ok(0) => {}
                Ok(n) => {
                    if on_data.send(InvokeResponseBody::Raw(buf[..n].to_vec())).is_err() {
                        break;
                    }
                }
                Err(e) if e.kind() == ErrorKind::TimedOut => {}
                Err(_) => {
                    // unplugged: an empty message ends the webview's stream, or it would wait for data for ever
                    let _ = on_data.send(InvokeResponseBody::Raw(Vec::new()));
                    break;
                }
            }
        }
    });
    let id = ports.next_id.fetch_add(1, Ordering::Relaxed);
    ports.open.lock().unwrap().insert(id, OpenPort { port: Arc::new(Mutex::new(port)), stop, reader: Some(reader) });
    Ok(id)
}

fn port_id(request: &Request<'_>) -> Result<u32, String> {
    request
        .headers()
        .get("x-port-id")
        .and_then(|v| v.to_str().ok())
        .and_then(|v| v.parse().ok())
        .ok_or_else(|| "missing x-port-id".to_string())
}

fn port_of(ports: &Ports, id: u32) -> Result<Arc<Mutex<Box<dyn serialport::SerialPort>>>, String> {
    ports.open.lock().unwrap().get(&id).map(|p| p.port.clone()).ok_or_else(|| "port not open".to_string())
}

// No flush: on Windows it waits, with no deadline, for a device that takes no data, as an ESP32-C3 whose firmware
// does not read its USB. A write that makes no progress for 1 s gives up instead.
// One USB packet at a time: a flight controller's passthrough takes the data at the UART's rate, and a larger write
// timed out half sent, so its start went out twice when written again (a flash through a Lucid H7 at 115200)
fn write_with_deadline(port: &mut dyn serialport::SerialPort, mut data: &[u8]) -> std::io::Result<()> {
    let mut progress = Instant::now();
    while !data.is_empty() {
        match port.write(&data[..data.len().min(64)]) {
            Ok(n) if n > 0 => {
                data = &data[n..];
                progress = Instant::now();
            }
            Ok(_) => {}
            Err(e) if e.kind() == ErrorKind::TimedOut || e.kind() == ErrorKind::Interrupted => {}
            Err(e) => return Err(e),
        }
        if progress.elapsed() > Duration::from_millis(1000) {
            return Err(std::io::Error::new(ErrorKind::TimedOut, "the device takes no data"));
        }
    }
    Ok(())
}

#[tauri::command]
pub fn serial_write(request: Request<'_>, ports: State<'_, Ports>) -> Result<(), String> {
    let InvokeBody::Raw(data) = request.body() else {
        return Err("expected raw bytes".into());
    };
    let port = port_of(&ports, port_id(&request)?)?;
    let mut port = port.lock().unwrap();
    write_with_deadline(port.as_mut(), data).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn serial_set_signals(id: u32, dtr: Option<bool>, rts: Option<bool>, ports: State<'_, Ports>) -> Result<(), String> {
    let port = port_of(&ports, id)?;
    let mut port = port.lock().unwrap();
    if let Some(v) = dtr {
        port.write_data_terminal_ready(v).map_err(|e| e.to_string())?;
    }
    if let Some(v) = rts {
        port.write_request_to_send(v).map_err(|e| e.to_string())?;
    }
    Ok(())
}

#[tauri::command]
pub fn serial_close(id: u32, ports: State<'_, Ports>) -> Result<(), String> {
    let p = ports.open.lock().unwrap().remove(&id);
    if let Some(mut p) = p {
        p.stop.store(true, Ordering::Relaxed);
        if let Some(t) = p.reader.take() {
            let _ = t.join(); // returns within the 20 ms read timeout
        }
    }
    Ok(())
}
