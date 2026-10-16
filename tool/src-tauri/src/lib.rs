mod serial;

use tauri::Manager;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .manage(serial::Ports::default())
        .on_page_load(|webview, payload| {
            if payload.event() == tauri::webview::PageLoadEvent::Started {
                webview.state::<serial::Ports>().close_all();
            }
        })
        .invoke_handler(tauri::generate_handler![
            serial::serial_list,
            serial::serial_open,
            serial::serial_write,
            serial::serial_set_signals,
            serial::serial_close,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
