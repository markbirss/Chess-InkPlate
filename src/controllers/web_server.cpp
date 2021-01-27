
#include "global.hpp"

#if CHESS_INKPLATE_BUILD

#include "logging.hpp"
#include "viewers/msg_viewer.hpp"
#include "models/config.hpp"

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>

#include "esp_err.h"
#include "esp_log.h"

#include "esp_vfs.h"
#include "esp_http_server.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "models/config.hpp"

static constexpr char const * TAG = "WebServer";

static constexpr int32_t      FILE_PATH_MAX     = 256;
static constexpr int32_t      MAX_FILE_SIZE     = 25*1024*1024;
static constexpr int32_t      SCRATCH_BUFSIZE   = 8192;

#define                       MAX_FILE_SIZE_STR   "25MB"

static httpd_handle_t server = nullptr;

struct FileServerData {
  char base_path[ESP_VFS_PATH_MAX + 1];
  char   scratch[SCRATCH_BUFSIZE];
};

static FileServerData * server_data = nullptr;

// Redirects incoming GET request for /index.html 

static esp_err_t 
index_html_get_handler(httpd_req_t *req)
{
  httpd_resp_set_status(req, "307 Temporary Redirect");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_send(req, NULL, 0);  // Response body can be empty
  return ESP_OK;
}

// Respond with an icon file embedded in flash.

static esp_err_t 
favicon_get_handler(httpd_req_t *req)
{
  extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
  extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
  const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
  httpd_resp_set_type(req, "image/x-icon");
  httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
  return ESP_OK;
}

/* Send HTTP response with a run-time generated html consisting of
 * a list of all files and folders under the requested path.
 * In case of SPIFFS this returns empty list when path is any
 * string other than '/', since SPIFFS doesn't support directories */
static esp_err_t 
http_resp_dir_html(httpd_req_t *req, const char * dirpath)
{
  char entrypath[FILE_PATH_MAX];
  char entrysize[16];
  const char * entrytype;

  struct dirent * entry;
  struct stat entry_stat;

  ESP_LOGD(TAG, "Opening dir: %s.", dirpath);

  const size_t dirpath_len = strlen(dirpath);

  /* Retrieve the base path of file storage to construct the full path */
  strlcpy(entrypath, dirpath, sizeof(entrypath));
  entrypath[strlen(entrypath) - 1] = 0;
  DIR * dir = opendir(entrypath);
  entrypath[strlen(entrypath)] = '/';

  if (dir == nullptr) {
    ESP_LOGE(TAG, "Failed to stat dir : %s (%s)", dirpath, strerror(errno));
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
    return ESP_FAIL;
  }

  httpd_resp_sendstr_chunk(req, 
    "<!DOCTYPE html><html>"
    "<head>"
    "<meta charset=\"UTF-8\">"
    "<title>EPub-InkPlate Books Server</title>"
    "<style>"
    "table {font-family: Arial, Helvetica, sans-serif;}"
    "table.list {width: 100%;}"
    "table.list {border-collapse: collapse;}"
    "table.list td {border: 1px solid #ddd; padding: 8px;}"
    "table.list tr:nth-child(even){background-color: #f2f2f2;}"
    "table.list td:nth-child(1), table.list th:nth-child(1){text-align: left;}"
    "table.list td:nth-child(2), table.list th:nth-child(2){text-align: center;}"
    "table.list td:nth-child(3), table.list th:nth-child(3){text-align: right; }"
    "table.list td:nth-child(4), table.list th:nth-child(4){text-align: center;}"
    "table.list th {border: 1px solid #077C95; padding: 12px 8px; background-color: #077C95; color: white;}"
    "table.list tr:hover {background-color: #ddd;}"
    "</style>"
    "<script>"
    "function sortTable(n) {"
    "var table, rows, switching, i, x, y, shouldSwitch, dir, switchcount = 0;"
    "table = document.getElementById(\"sorted\");"
    "switching = true; dir = \"asc\";"
    "while (switching) {"
    "switching = false; rows = table.rows;"
    "for (i = 1; i < (rows.length - 1); i++) {"
    "shouldSwitch = false;"
    "x = rows[i].getElementsByTagName(\"TD\")[n];"
    "y = rows[i + 1].getElementsByTagName(\"TD\")[n];"
    "if (dir == \"asc\") {"
    "if (x.innerHTML.toLowerCase() > y.innerHTML.toLowerCase()) {"
    "shouldSwitch= true; break;"
    "}} else if (dir == \"desc\") {"
    "if (x.innerHTML.toLowerCase() < y.innerHTML.toLowerCase()) {"
    "shouldSwitch = true; break;}}}"
    "if (shouldSwitch) {"
    "rows[i].parentNode.insertBefore(rows[i + 1], rows[i]);"
    "switching = true; switchcount ++; } else {"
    "if (switchcount == 0 && dir == \"asc\") {"
    "dir = \"desc\"; switching = true;"
    "}}}}"
    "</script>"
    "</head>"
    "<body>");

  extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
  extern const unsigned char upload_script_end[]   asm("_binary_upload_script_html_end");
  const size_t upload_script_size = (upload_script_end - upload_script_start);

  httpd_resp_send_chunk(req, (const char *) upload_script_start, upload_script_size);

  httpd_resp_sendstr_chunk(req,
    "<table class=\"fixed list\" id=\"sorted\">"
    "<colgroup><col width=\"70%\"/><col width=\"8%\"/><col width=\"14%\"/><col width=\"8%\"/></colgroup>"
    "<thead><tr><th onclick=\"sortTable(0)\">Name</th><th>Type</th><th>Size (Bytes)</th><th>Delete</th></tr></thead>"
    "<tbody>");

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(&entry->d_name[strlen(entry->d_name) - 5], ".game") != 0) continue;
    
    entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

    strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
    if (stat(entrypath, &entry_stat) == -1) {
      ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
      continue;
    }
    sprintf(entrysize, "%ld", entry_stat.st_size);
    ESP_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);

    httpd_resp_sendstr_chunk(req, "<tr><td><a href=\"");
    httpd_resp_sendstr_chunk(req, req->uri);
    httpd_resp_sendstr_chunk(req, entry->d_name);
    if (entry->d_type == DT_DIR) {
      httpd_resp_sendstr_chunk(req, "/");
    }
    httpd_resp_sendstr_chunk(req, "\">");
    httpd_resp_sendstr_chunk(req, entry->d_name);
    httpd_resp_sendstr_chunk(req, "</a></td><td>");
    httpd_resp_sendstr_chunk(req, entrytype);
    httpd_resp_sendstr_chunk(req, "</td><td>");
    httpd_resp_sendstr_chunk(req, entrysize);
    httpd_resp_sendstr_chunk(req, "</td><td>");
    httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/delete");
    httpd_resp_sendstr_chunk(req, req->uri);
    httpd_resp_sendstr_chunk(req, entry->d_name);
    httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Delete</button></form>");
    httpd_resp_sendstr_chunk(req, "</td></tr>\n");
  }
  closedir(dir);

  httpd_resp_sendstr_chunk(req,"</tbody></table>");

  httpd_resp_sendstr_chunk(req,
    "<script>window.addEventListener(\"load\", function(){sortTable(0);})</script>" 
    "</body></html>");

  httpd_resp_sendstr_chunk(req, NULL);
  return ESP_OK;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

static esp_err_t 
set_content_type_from_file(httpd_req_t * req, const char * filename)
{
  if (IS_FILE_EXT(filename, ".game")) {
    return httpd_resp_set_type(req, "text/plain");
  } 

  return httpd_resp_set_type(req, "text/plain");
}

static unsigned char 
bin(char ch)
{
  if ((ch >= '0') && (ch <= '9')) return ch - '0';
  if ((ch >= 'A') && (ch <= 'F')) return ch - 'A' + 10;
  if ((ch >= 'a') && (ch <= 'f')) return ch - 'a' + 10;
  return 0;
}

static const char * 
get_path_from_uri(char * dest, const char * base_path, const char * uri, size_t destsize)
{
  const size_t base_pathlen = strlen(base_path);
  size_t pathlen = strlen(uri);

  const char *quest = strchr(uri, '?');
  if (quest) {
    pathlen = std::min((int)pathlen, (int)(quest - uri));
  }
  const char *hash = strchr(uri, '#');
  if (hash) {
    pathlen = std::min((int)pathlen, (int)(hash - uri));
  }

  if (base_pathlen + pathlen + 1 > destsize) {
    return NULL;
  }

  strcpy(dest, base_path);
  const char * str_in = uri;
  char *      str_out = dest + base_pathlen;
  int           count = pathlen + 1;

  while (count > 0) {
    if (str_in[0] == '%') {
      *str_out++ = (bin(str_in[1]) << 4) + bin(str_in[2]);
      count  -= 3;
      str_in += 3;
    }
    else {
      *str_out++ = *str_in++;
      count--;
    }
  }

  return dest + base_pathlen;
}

// ----- download_handler() -----

static esp_err_t
download_handler(httpd_req_t * req)
{
  char filepath[FILE_PATH_MAX];
  FILE * fd = NULL;
  struct stat file_stat;

  const char * filename = get_path_from_uri(
    filepath, 
    ((FileServerData *) req->user_ctx)->base_path,
    req->uri, 
    sizeof(filepath));

  if (!filename) {
    ESP_LOGE(TAG, "Filename is too long");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
    return ESP_FAIL;
  }

  if (filename[strlen(filename) - 1] == '/') {
    return http_resp_dir_html(req, filepath);
  }

  if (stat(filepath, &file_stat) == -1) {
      /* If file not present check if URL
        * corresponds to one of the hardcoded paths */
    if (strcmp(filename, "/index.html") == 0) {
      return index_html_get_handler(req);
    } else if (strcmp(filename, "/favicon.ico") == 0) {
      return favicon_get_handler(req);
    }
    ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
    return ESP_FAIL;
  }

  fd = fopen(filepath, "r");
  if (!fd) {
    ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
  set_content_type_from_file(req, filename);

  // Retrieve the pointer to scratch buffer for temporary storage
  char *chunk = ((FileServerData *)req->user_ctx)->scratch;
  size_t chunksize;
  do {
    chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

    if (chunksize > 0) {
      if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
        fclose(fd);
        ESP_LOGE(TAG, "File sending failed!");
        httpd_resp_sendstr_chunk(req, NULL);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
        return ESP_FAIL;
      }
    }
  } while (chunksize != 0);

  fclose(fd);
  ESP_LOGI(TAG, "File sending complete");

  /* Respond with an empty chunk to signal HTTP response completion */
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

// ----- upload_handler() -----

static esp_err_t 
upload_handler(httpd_req_t *req)
{
  char filepath[FILE_PATH_MAX];
  FILE *fd = NULL;
  struct stat file_stat;

  /* Skip leading "/upload" from URI to get filename */
  /* Note sizeof() counts NULL termination hence the -1 */
  const char *filename = get_path_from_uri(filepath, ((FileServerData *)req->user_ctx)->base_path,
                                            req->uri + sizeof("/upload") - 1, sizeof(filepath));
  if (!filename) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
    return ESP_FAIL;
  }

  if (filename[strlen(filename) - 1] == '/') {
    ESP_LOGE(TAG, "Invalid filename : %s", filename);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
    return ESP_FAIL;
  }

  if (stat(filepath, &file_stat) == 0) {
    ESP_LOGE(TAG, "File already exists : %s", filepath);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
    return ESP_FAIL;
  }

  if (req->content_len > MAX_FILE_SIZE) {
    ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                        "File size must be less than "
                        MAX_FILE_SIZE_STR "!");
    return ESP_FAIL;
  }

  fd = fopen(filepath, "w");
  if (!fd) {
    ESP_LOGE(TAG, "Failed to create file : %s", filepath);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Receiving file : %s...", filename);

  /* Retrieve the pointer to scratch buffer for temporary storage */
  char *buf = ((FileServerData *) req->user_ctx)->scratch;
  int received;

  /* Content length of the request gives
    * the size of the file being uploaded */
  int remaining = req->content_len;

  while (remaining > 0) {

    ESP_LOGI(TAG, "Remaining size : %d", remaining);
    if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
      if (received == HTTPD_SOCK_ERR_TIMEOUT) {
        continue;
      }

      fclose(fd);
      unlink(filepath);

      ESP_LOGE(TAG, "File reception failed!");
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
      return ESP_FAIL;
    }

    /* Write buffer content to file on storage */
    if (received && (received != fwrite(buf, 1, received, fd))) {
      /* Couldn't write everything to file!
        * Storage may be full? */
      fclose(fd);
      unlink(filepath);

      ESP_LOGE(TAG, "File write failed!");
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
      return ESP_FAIL;
    }

    remaining -= received;
  }
  fclose(fd);
  ESP_LOGI(TAG, "File reception complete");

  /* Redirect onto root to see the updated file list */
  httpd_resp_set_status(req, "303 See Other");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_sendstr(req, "File uploaded successfully");
  return ESP_OK;
}

// ----- delete_handler() -----

static esp_err_t 
delete_handler(httpd_req_t *req)
{
  char filepath[FILE_PATH_MAX];
  struct stat file_stat;

  /* Skip leading "/delete" from URI to get filename */
  /* Note sizeof() counts NULL termination hence the -1 */
  const char * filename = get_path_from_uri(filepath, ((FileServerData *)req->user_ctx)->base_path,
                                            req->uri  + sizeof("/delete") - 1, sizeof(filepath));
  if (!filename) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
    return ESP_FAIL;
  }

  if (filename[strlen(filename) - 1] == '/') {
    ESP_LOGE(TAG, "Invalid filename : %s", filename);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
    return ESP_FAIL;
  }

  if (stat(filepath, &file_stat) == -1) {
    ESP_LOGE(TAG, "File does not exist : %s", filepath);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Deleting file : %s", filepath);
  unlink(filepath);

  /* Redirect onto root to see the updated file list */
  httpd_resp_set_status(req, "303 See Other");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_sendstr(req, "File deleted successfully");
  return ESP_OK;
}

// ----- http_server_start() -----

static esp_err_t 
http_server_start()
{
  if (server_data) {
    ESP_LOGE(TAG, "File server already started");
    return ESP_ERR_INVALID_STATE;
  }

  /* Allocate memory for server data */
  server_data = (FileServerData *) calloc(1, sizeof(FileServerData));
  if (!server_data) {
    ESP_LOGE(TAG, "Failed to allocate memory for server data");
    return ESP_ERR_NO_MEM;
  }
  strcpy(server_data->base_path, "/sdcard/books");

  httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();

  int32_t port;
  config.get(Config::Ident::PORT, &port);
  httpd_config.uri_match_fn = httpd_uri_match_wildcard;
  httpd_config.server_port = (uint16_t) port;

  ESP_LOGI(TAG, "Starting HTTP Server");
  esp_err_t res = httpd_start(&server, &httpd_config);
  if (res != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start file server (%s)!", esp_err_to_name(res));
    return ESP_FAIL;
  }

  httpd_uri_t file_download = {
    .uri       = "/*",  // Match all URIs of type /path/to/file
    .method    = HTTP_GET,
    .handler   = download_handler,
    .user_ctx  = server_data 
  };
  httpd_register_uri_handler(server, &file_download);

  httpd_uri_t file_upload = {
    .uri       = "/upload/*",   // Match all URIs of type /upload/path/to/file
    .method    = HTTP_POST,
    .handler   = upload_handler,
    .user_ctx  = server_data 
  };
  httpd_register_uri_handler(server, &file_upload);

  httpd_uri_t file_delete = {
    .uri       = "/delete/*",   // Match all URIs of type /delete/path/to/file
    .method    = HTTP_POST,
    .handler   = delete_handler,
    .user_ctx  = server_data
  };
  httpd_register_uri_handler(server, &file_delete);

  return ESP_OK;
}

// ----- http_server_stop() -----

static void
http_server_stop()
{
  httpd_stop(server);
  free(server_data);
  server_data = nullptr;
}

// ----- sta_event_handler() -----

// The event group allows multiple bits for each event, but we 
// only care about two events:
// - we are connected to the AP with an IP
// - we failed to connect after the maximum amount of retries

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// FreeRTOS event group to signal when we are connected

static EventGroupHandle_t wifi_event_group = nullptr;
static bool wifi_first_start = true;

static constexpr int8_t ESP_MAXIMUM_RETRY = 6;

static int s_retry_num = 0;
static esp_ip4_addr_t ip_address;

static void sta_event_handler(void            * arg, 
                              esp_event_base_t  event_base,
                              int32_t           event_id, 
                              void            * event_data)
{
  ESP_LOGI(TAG, "STA Event, Base: %08x, Event: %d.", (unsigned int) event_base, event_id);

  if (event_base == WIFI_EVENT) {
    if (event_id == WIFI_EVENT_STA_START) {
      esp_wifi_connect();
    } 
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
      if (wifi_first_start) {
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
          vTaskDelay(pdMS_TO_TICKS(10E3));
          ESP_LOGI(TAG, "retry to connect to the AP");
          esp_wifi_connect();
          s_retry_num++;
        } 
        else {
          xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
          ESP_LOGI(TAG,"connect to the AP fail");
        }
      }
      else {
        ESP_LOGI(TAG, "Wifi Disconnected.");
        vTaskDelay(pdMS_TO_TICKS(10E3));
        ESP_LOGI(TAG, "retry to connect to the AP");
        esp_wifi_connect();
      }
    } 
  }
  else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t * event = (ip_event_got_ip_t*) event_data;
      ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
      ip_address = event->ip_info.ip;
      s_retry_num = 0;
      xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
      wifi_first_start = false;
    }
  }
}

// ----- wifi_start() -----

static bool 
wifi_start(void)
{
  bool connected = false;
  wifi_first_start = true;

  if (wifi_event_group == nullptr) wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(
    WIFI_EVENT, 
    ESP_EVENT_ANY_ID,    
    &sta_event_handler, 
    NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(
    IP_EVENT,   
    IP_EVENT_STA_GOT_IP, 
    &sta_event_handler, 
    NULL));

  std::string wifi_ssid;
  std::string wifi_pwd;

  config.get(Config::Ident::SSID, wifi_ssid);
  config.get(Config::Ident::PWD,  wifi_pwd );

  wifi_config_t wifi_config;

  bzero(&wifi_config, sizeof(wifi_config_t));

  wifi_config.sta.bssid_set          = 0;
  wifi_config.sta.pmf_cfg.capable    = true;
  wifi_config.sta.pmf_cfg.required   = false;
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

  strcpy((char *) wifi_config.sta.ssid,     wifi_ssid.c_str());
  strcpy((char *) wifi_config.sta.password, wifi_pwd.c_str());

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  // Waiting until either the connection is established (WIFI_CONNECTED_BIT) 
  // or connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). 
  // The bits are set by event_handler() (see above)

  EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
    WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
    pdFALSE,
    pdFALSE,
    portMAX_DELAY);

  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
             wifi_ssid.c_str(), wifi_pwd.c_str());
    connected = true;
  } 
  else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGE(TAG, "Failed to connect to SSID:%s, password:%s",
             wifi_ssid.c_str(), wifi_pwd.c_str());
  }
  else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }

  if (!connected) {
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
  }
  return connected;
}

void
wifi_stop()
{
  ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT,   ESP_EVENT_ANY_ID,     &sta_event_handler));
  ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, &sta_event_handler));

  vEventGroupDelete(wifi_event_group);
  wifi_event_group = nullptr;

  esp_event_loop_delete_default();

  esp_wifi_disconnect();
  esp_wifi_stop();
  esp_wifi_deinit();
}

bool
start_web_server()
{
  msg_viewer.show(MsgViewer::Severity::WIFI, false, true, 
    "Web Server Starting", 
    "The Web server is now establishing the connexion with the WiFi router. Please wait.");

  if (wifi_start()) {
    EventBits_t bits = xEventGroupGetBits(wifi_event_group);
    if (bits & WIFI_CONNECTED_BIT) {
      msg_viewer.show(MsgViewer::Severity::WIFI, true, true, 
        "Web Server", 
        "The Web server is now running at ip " IPSTR ". To stop it, please press a key.", IP2STR(&ip_address));
      return http_server_start() == ESP_OK;
    }
    else {
      msg_viewer.show(MsgViewer::Severity::ALERT, true, true, 
        "Web Server Failed", 
        "The Web server was not able to start. Correct the situation and try again.", IP2STR(&ip_address));
    }
  }
  else {
    wifi_stop();
  }
  return false;
}

void
stop_web_server()
{
  http_server_stop();
  wifi_stop();
}

#endif