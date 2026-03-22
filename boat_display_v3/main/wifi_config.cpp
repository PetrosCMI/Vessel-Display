// wifi_config.cpp ΓÇö AP mode + HTTP config server
#include "wifi_config.h"
#include "settings.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "WifiConfig";
static httpd_handle_t s_server  = NULL;
static bool           s_active  = false;

// ---------------------------------------------
//  HTML page ΓÇö served from flash
//  Single page: all 5 WiFi slots + SignalK
// ---------------------------------------------
static const char HTML_PAGE[] =
"<!DOCTYPE html><html><head>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Boat Display Config</title>"
"<style>"
"body{font-family:sans-serif;background:#0a0e17;color:#e8f0fe;margin:0;padding:16px}"
"h1{color:#00bfff;font-size:1.4em;margin-bottom:4px}"
"h2{color:#00bfff;font-size:1.1em;margin:20px 0 8px}"
".card{background:#111827;border:1px solid #1e3a5f;border-radius:8px;"
"padding:12px;margin-bottom:10px}"
"label{display:block;color:#607d8b;font-size:0.85em;margin-bottom:3px}"
"input{width:100%;box-sizing:border-box;padding:8px;border-radius:4px;"
"border:1px solid #1e3a5f;background:#1a2538;color:#e8f0fe;font-size:1em}"
"input:focus{border-color:#00bfff;outline:none}"
".pri{color:#607d8b;font-size:0.8em;float:right}"
"button{width:100%;padding:14px;background:#00bfff;color:#000;border:none;"
"border-radius:6px;font-size:1.1em;font-weight:bold;margin-top:16px;cursor:pointer}"
"button:active{background:#0090cc}"
".saved{display:none;background:#00e676;color:#000;padding:10px;border-radius:6px;"
"text-align:center;margin-top:10px;font-weight:bold}"
"</style></head><body>"
"<h1>&#9875; Boat Display Config</h1>"
"<p style='color:#607d8b;font-size:0.9em'>Changes take effect immediately after Save.</p>"
"<form method='POST' action='/save'>"
"<h2>WiFi Networks</h2>"
"<div class='card'><span class='pri'>Priority 1 (highest)</span>"
"<label>SSID</label><input name='s0' maxlength='32' value='{{S0}}'>"
"<label style='margin-top:8px'>Password</label>"
"<input name='p0' maxlength='64' value='{{P0}}'></div>"
"<div class='card'><span class='pri'>Priority 2</span>"
"<label>SSID</label><input name='s1' maxlength='32' value='{{S1}}'>"
"<label style='margin-top:8px'>Password</label>"
"<input name='p1' maxlength='64' value='{{P1}}'></div>"
"<div class='card'><span class='pri'>Priority 3</span>"
"<label>SSID</label><input name='s2' maxlength='32' value='{{S2}}'>"
"<label style='margin-top:8px'>Password</label>"
"<input name='p2' maxlength='64' value='{{P2}}'></div>"
"<div class='card'><span class='pri'>Priority 4</span>"
"<label>SSID</label><input name='s3' maxlength='32' value='{{S3}}'>"
"<label style='margin-top:8px'>Password</label>"
"<input name='p3' maxlength='64' value='{{P3}}'></div>"
"<div class='card'><span class='pri'>Priority 5 (lowest)</span>"
"<label>SSID</label><input name='s4' maxlength='32' value='{{S4}}'>"
"<label style='margin-top:8px'>Password</label>"
"<input name='p4' maxlength='64' value='{{P4}}'></div>"
"<h2>SignalK Server</h2>"
"<div class='card'>"
"<label>Host / IP address</label>"
"<input name='skh' maxlength='63' value='{{SKH}}'>"
"<label style='margin-top:8px'>Port</label>"
"<input name='skp' maxlength='5' value='{{SKP}}'>"
"</div>"
"<button type='submit'>&#10003; Save &amp; Restart</button>"
"</form>"
"<div class='saved' id='sv'>Saved! Device is restarting...</div>"
"</body></html>";

// ---------------------------------------------
//  URL decode helper
// ---------------------------------------------
static void url_decode(const char* src, char* dst, size_t dst_len) {
    size_t di = 0;
    for (size_t i = 0; src[i] && di < dst_len - 1; i++) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            char hex[3] = { src[i+1], src[i+2], 0 };
            dst[di++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else if (src[i] == '+') {
            dst[di++] = ' ';
        } else {
            dst[di++] = src[i];
        }
    }
    dst[di] = '\0';
}

// Extract a form field value from POST body
static void get_field(const char* body, const char* key,
                       char* out, size_t out_len) {
    out[0] = '\0';
    char search[32];
    snprintf(search, sizeof(search), "%s=", key);
    const char* p = strstr(body, search);
    if (!p) return;
    p += strlen(search);
    const char* end = strchr(p, '&');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len >= out_len) len = out_len - 1;
    char encoded[128] = {};
    strncpy(encoded, p, len < sizeof(encoded) ? len : sizeof(encoded)-1);
    url_decode(encoded, out, out_len);
}

// ---------------------------------------------
//  Template substitution for HTML page
// ---------------------------------------------
static char* build_html(void) {
    // Allocate generous buffer
    char* buf = (char*)malloc(8192);
    if (!buf) return NULL;

    // Simple template substitution ΓÇö replace {{XX}} tokens
    char* out = buf;
    const char* src = HTML_PAGE;
    size_t remaining = 8191;

    while (*src && remaining > 0) {
        if (src[0] == '{' && src[1] == '{') {
            // Find closing }}
            const char* end = strstr(src + 2, "}}");
            if (!end) { *out++ = *src++; remaining--; continue; }

            char token[8] = {};
            size_t tlen = end - src - 2;
            if (tlen < sizeof(token)) strncpy(token, src + 2, tlen);

            const char* val = "";
            char portbuf[8];

            if      (strcmp(token, "S0") == 0) val = gSettings.wifi[0].ssid;
            else if (strcmp(token, "S1") == 0) val = gSettings.wifi[1].ssid;
            else if (strcmp(token, "S2") == 0) val = gSettings.wifi[2].ssid;
            else if (strcmp(token, "S3") == 0) val = gSettings.wifi[3].ssid;
            else if (strcmp(token, "S4") == 0) val = gSettings.wifi[4].ssid;
            else if (strcmp(token, "P0") == 0) val = gSettings.wifi[0].pass;
            else if (strcmp(token, "P1") == 0) val = gSettings.wifi[1].pass;
            else if (strcmp(token, "P2") == 0) val = gSettings.wifi[2].pass;
            else if (strcmp(token, "P3") == 0) val = gSettings.wifi[3].pass;
            else if (strcmp(token, "P4") == 0) val = gSettings.wifi[4].pass;
            else if (strcmp(token, "SKH") == 0) val = gSettings.signalk_host;
            else if (strcmp(token, "SKP") == 0) {
                snprintf(portbuf, sizeof(portbuf), "%d", gSettings.signalk_port);
                val = portbuf;
            }

            size_t vlen = strlen(val);
            if (vlen > remaining) vlen = remaining;
            memcpy(out, val, vlen);
            out += vlen;
            remaining -= vlen;
            src = end + 2;
        } else {
            *out++ = *src++;
            remaining--;
        }
    }
    *out = '\0';
    return buf;
}

// ---------------------------------------------
//  HTTP handlers
// ---------------------------------------------
static esp_err_t handle_get(httpd_req_t* req) {
    char* html = build_html();
    if (!html) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, strlen(html));
    free(html);
    return ESP_OK;
}

static esp_err_t handle_save(httpd_req_t* req) {
    // Read POST body
    char body[1024] = {};
    int ret = httpd_req_recv(req, body, sizeof(body) - 1);
    if (ret <= 0) { httpd_resp_send_500(req); return ESP_FAIL; }
    body[ret] = '\0';

    ESP_LOGI(TAG, "Save: %s", body);

    char tmp[WIFI_PASS_LEN];

    // Parse WiFi networks
    for (int i = 0; i < WIFI_MAX_NETWORKS; i++) {
        char skey[4], pkey[4];
        snprintf(skey, sizeof(skey), "s%d", i);
        snprintf(pkey, sizeof(pkey), "p%d", i);

        get_field(body, skey, tmp, WIFI_SSID_LEN);
        strncpy(gSettings.wifi[i].ssid, tmp, WIFI_SSID_LEN - 1);

        get_field(body, pkey, tmp, WIFI_PASS_LEN);
        strncpy(gSettings.wifi[i].pass, tmp, WIFI_PASS_LEN - 1);

        // Enable slot if SSID is not empty
        gSettings.wifi[i].enabled = (gSettings.wifi[i].ssid[0] != '\0');
    }

    // Parse SignalK
    get_field(body, "skh", tmp, SIGNALK_HOST_LEN);
    if (tmp[0]) strncpy(gSettings.signalk_host, tmp, SIGNALK_HOST_LEN - 1);

    get_field(body, "skp", tmp, 8);
    int port = atoi(tmp);
    if (port >= 1 && port <= 65535) gSettings.signalk_port = (uint16_t)port;

    settings_save();

    // Respond with success page
    const char* resp =
        "<!DOCTYPE html><html><head>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Saved</title>"
        "<style>body{font-family:sans-serif;background:#0a0e17;color:#e8f0fe;"
        "display:flex;align-items:center;justify-content:center;height:100vh;margin:0}"
        ".box{text-align:center;padding:32px;background:#111827;border-radius:12px;"
        "border:1px solid #1e3a5f}"
        "h1{color:#00e676}p{color:#607d8b}</style></head>"
        "<body><div class='box'>"
        "<h1>&#10003; Saved!</h1>"
        "<p>Device is connecting to your network.<br>"
        "This page will become unavailable shortly.</p>"
        "</div></body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, strlen(resp));

    // Stop AP after a short delay to allow response to be sent
    // network_reconnect() is called from page_network after detecting
    // config mode has ended
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();  // cleanest restart after config save

    return ESP_OK;
}

// ---------------------------------------------
//  Public API
// ---------------------------------------------
void wifi_config_start(void) {
    if (s_active) return;
    ESP_LOGI(TAG, "Starting config AP: %s", CONFIG_AP_SSID);

    // Switch to AP+STA mode
    esp_wifi_set_mode(WIFI_MODE_APSTA);

    // ── ADD THESE LINES — configure AP netif with DHCP ──────
    esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap_netif) {
        ap_netif = esp_netif_create_default_wifi_ap();
    }
    // Set static IP for the AP interface
    esp_netif_ip_info_t ip_info = {};
    ip_info.ip.addr      = esp_ip4addr_aton(CONFIG_AP_IP);
    ip_info.gw.addr      = esp_ip4addr_aton(CONFIG_AP_IP);
    ip_info.netmask.addr = esp_ip4addr_aton("255.255.255.0");
    esp_netif_dhcps_stop(ap_netif);
    esp_netif_set_ip_info(ap_netif, &ip_info);
    esp_netif_dhcps_start(ap_netif);
    // ────────────────────────────────────────────────────────

    wifi_config_t ap_cfg = {};
    strncpy((char*)ap_cfg.ap.ssid,     CONFIG_AP_SSID,
            sizeof(ap_cfg.ap.ssid) - 1);
    strncpy((char*)ap_cfg.ap.password, CONFIG_AP_PASSWORD,
            sizeof(ap_cfg.ap.password) - 1);
    ap_cfg.ap.ssid_len       = strlen(CONFIG_AP_SSID);
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.authmode       = strlen(CONFIG_AP_PASSWORD) > 0
                               ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);

    // Start HTTP server
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port     = CONFIG_AP_PORT;
    cfg.lru_purge_enable = true;

    if (httpd_start(&s_server, &cfg) == ESP_OK) {
        httpd_uri_t get_uri  = { "/",     HTTP_GET,  handle_get,  NULL };
        httpd_uri_t save_uri = { "/save", HTTP_POST, handle_save, NULL };
        httpd_register_uri_handler(s_server, &get_uri);
        httpd_register_uri_handler(s_server, &save_uri);
        ESP_LOGI(TAG, "HTTP server started on port %d", CONFIG_AP_PORT);
    } else {
        ESP_LOGE(TAG, "HTTP server start failed");
    }

    s_active = true;
}

void wifi_config_stop(void) {
    if (!s_active) return;
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    esp_wifi_set_mode(WIFI_MODE_STA);
    s_active = false;
    ESP_LOGI(TAG, "Config AP stopped");
}

bool wifi_config_active(void) {
    return s_active;
}
